#include "Presentation/GloamsteadCorruptionVisualizer.h"

#include "PCG/GloamsteadPCGSubsystem.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SceneComponent.h"
#include "HAL/IConsoleManager.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"

namespace
{
	// The three authored rot decals, which shipped with no C++ consumer of any kind. They layer by
	// severity: a faint bloom is an ash stain, a bad one is ash over grime over lichen. M_Decal_AshStain
	// exposes no scalar parameters, so layering is how severity reads in the art rather than in a number.
	struct FStainLayer { const TCHAR* Path; float MinCorruption; };
	const FStainLayer GStainLayers[] = {
		{ TEXT("/Game/Gloamstead/Kit/Decals/M_Decal_AshStain.M_Decal_AshStain"), 0.00f },
		{ TEXT("/Game/Gloamstead/Kit/Decals/M_Decal_Grime.M_Decal_Grime"),       0.40f },
		{ TEXT("/Game/Gloamstead/Kit/Decals/M_Decal_Lichen.M_Decal_Lichen"),     0.70f },
	};
	constexpr int32 GStainLayerCount = UE_ARRAY_COUNT(GStainLayers);

	// On by default - this is shipping presentation, not a diagnostic overlay. The switch exists so the
	// stains can be A/B'd against a clean sanctuary from an identical camera when tuning the art.
	static TAutoConsoleVariable<bool> CVarShowCorruptionStains(
		TEXT("gloam.Corruption.ShowStains"),
		true,
		TEXT("Render the corruption stain decals on corrupted ritual points (default on)."),
		ECVF_Default);
}

void UGloamsteadCorruptionVisualizer::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Presentation only lives in a live game world; automation and editor preview stay clean.
	if (!InWorld.IsGameWorld())
	{
		return;
	}

	CachedPCG = InWorld.GetSubsystem<UGloamsteadPCGSubsystem>();
	if (!CachedPCG)
	{
		return;
	}

	CorruptionChangedHandle = CachedPCG->OnCorruptionChanged.AddUObject(
		this, &UGloamsteadCorruptionVisualizer::HandleCorruptionChanged);
	StateRebuiltHandle = CachedPCG->AddAuthoritativeStateRebuiltListener(
		FSimpleDelegate::CreateUObject(this, &UGloamsteadCorruptionVisualizer::HandleCorruptionChanged));
	bBound = true;

	// The sanctuary can already carry corruption at BeginPlay (the graph seeds it, and a save restores
	// it), so show the world as it actually is before the first pressure beat rather than after it.
	RefreshAll();
}

void UGloamsteadCorruptionVisualizer::Deinitialize()
{
	if (bBound && IsValid(CachedPCG))
	{
		CachedPCG->OnCorruptionChanged.Remove(CorruptionChangedHandle);
		CachedPCG->RemoveAuthoritativeStateRebuiltListener(StateRebuiltHandle);
	}
	bBound = false;
	CorruptionChangedHandle.Reset();
	StateRebuiltHandle.Reset();
	PointDecals.Reset();
	if (DecalHolder)
	{
		DecalHolder->Destroy();
		DecalHolder = nullptr;
	}
	CachedPCG = nullptr;

	Super::Deinitialize();
}

void UGloamsteadCorruptionVisualizer::HandleCorruptionChanged()
{
	RefreshAll();
}

int32 UGloamsteadCorruptionVisualizer::Test_GetVisibleDecalCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<UDecalComponent>& Decal : PointDecals)
	{
		if (Decal && Decal->IsVisible())
		{
			++Count;
		}
	}
	return Count;
}

void UGloamsteadCorruptionVisualizer::RefreshAll()
{
	UWorld* World = GetWorld();
	if (!World || !CachedPCG)
	{
		return;
	}

	if (!CVarShowCorruptionStains.GetValueOnGameThread())
	{
		for (const TObjectPtr<UDecalComponent>& Existing : PointDecals)
		{
			if (Existing)
			{
				Existing->SetVisibility(false);
			}
		}
		if (LastShownCount != 0)
		{
			LastShownCount = 0;
			UE_LOG(LogTemp, Log, TEXT("CorruptionVisualizer: stains disabled by gloam.Corruption.ShowStains."));
		}
		return;
	}

	const int32 PointCount = CachedPCG->GetRitualPointCount();
	if (PointCount <= 0)
	{
		// Not a fault: PCG has not published its points yet. The rebuild listener re-runs this.
		UE_LOG(LogTemp, Verbose, TEXT("CorruptionVisualizer: no ritual points yet; waiting for PCG."));
		return;
	}

	UMaterialInterface* LayerMaterials[GStainLayerCount] = {};
	for (int32 Layer = 0; Layer < GStainLayerCount; ++Layer)
	{
		LayerMaterials[Layer] = LoadObject<UMaterialInterface>(nullptr, GStainLayers[Layer].Path);
	}
	if (!LayerMaterials[0])
	{
		// Say it once, loudly. Silently rendering nothing is exactly how corruption stayed invisible.
		UE_LOG(LogTemp, Error,
			TEXT("CorruptionVisualizer: could not load %s, so corruption has no visual. Restore that asset "
				 "or point this subsystem at the decal you want."),
			GStainLayers[0].Path);
		return;
	}

	if (!DecalHolder)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		DecalHolder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (!DecalHolder)
		{
			return;
		}
		DecalHolder->SetRootComponent(
			NewObject<USceneComponent>(DecalHolder, TEXT("CorruptionDecalRoot")));
		DecalHolder->GetRootComponent()->RegisterComponent();
	}

	PointDecals.SetNum(PointCount * GStainLayerCount);

	int32 Shown = 0;
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		const float Corruption = CachedPCG->GetCorruptionLevel(Index);
		const bool bVisible = Corruption > VisibleThreshold;

		if (!bVisible)
		{
			for (int32 Layer = 0; Layer < GStainLayerCount; ++Layer)
			{
				if (TObjectPtr<UDecalComponent>& Hide = PointDecals[Index * GStainLayerCount + Layer])
				{
					Hide->SetVisibility(false);
				}
			}
			continue;
		}

		FPCGPoint Point;
		if (!CachedPCG->GetPointByIndex(Index, Point))
		{
			continue;
		}
		const FVector Location = Point.Transform.GetLocation();

		// The ritual point's Z is the graph's, not the floor's - the plaza the player walks on can sit
		// well above or below it. Trace for the real surface so the stain lands on the ground the player
		// sees, and keep the projection box deep enough to survive an uneven floor.
		FVector Surface = Location;
		if (UWorld* TraceWorld = GetWorld())
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(CorruptionStain), /*bTraceComplex*/ false);
			Params.AddIgnoredActor(DecalHolder);
			if (TraceWorld->LineTraceSingleByChannel(
					Hit, Location + FVector(0.f, 0.f, 1000.f), Location - FVector(0.f, 0.f, 2000.f),
					ECC_WorldStatic, Params))
			{
				Surface = Hit.ImpactPoint;
			}
		}

		const float Alpha = FMath::Clamp(
			(Corruption - VisibleThreshold) / FMath::Max(1.f - VisibleThreshold, KINDA_SMALL_NUMBER), 0.f, 1.f);
		const float Radius = FMath::Lerp(MinRadius, MaxRadius, Alpha);

		for (int32 Layer = 0; Layer < GStainLayerCount; ++Layer)
		{
			TObjectPtr<UDecalComponent>& Slot = PointDecals[Index * GStainLayerCount + Layer];
			const bool bLayerActive = LayerMaterials[Layer] != nullptr
				&& Corruption >= GStainLayers[Layer].MinCorruption;
			if (!bLayerActive)
			{
				if (Slot)
				{
					Slot->SetVisibility(false);
				}
				continue;
			}

			if (!Slot)
			{
				UDecalComponent* Created = NewObject<UDecalComponent>(DecalHolder);
				Created->SetupAttachment(DecalHolder->GetRootComponent());
				Created->SetDecalMaterial(LayerMaterials[Layer]);
				// Project straight down onto the ground the ritual point sits on. Each layer is turned a
				// little so the three do not stamp an identical silhouette three times.
				Created->SetRelativeRotation(FRotator(-90.f, 40.f * Layer, 0.f));
				// Never fade with distance: a stain the player can only see standing on it is no warning.
				Created->SetFadeScreenSize(0.f);
				Created->RegisterComponent();
				Created->CreateDynamicMaterialInstance();
				Slot = Created;
			}
			UDecalComponent* Decal = Slot;

			// Deeper layers sit slightly tighter, so severity reads as a darkening core rather than a
			// single flat wash that just gets wider.
			const float LayerRadius = Radius * (1.f - 0.18f * Layer);
			Decal->SetWorldLocation(Surface + FVector(0.f, 0.f, 150.f));
			Decal->DecalSize = FVector(600.f, LayerRadius, LayerRadius);
			Decal->SetVisibility(true);
			Decal->MarkRenderStateDirty();

			// These decal materials expose no named scalar parameters today, so severity reads through
			// layering and footprint. The calls are kept so that adding an Opacity/Corruption parameter
			// to the art starts driving it with no code change here.
			if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial()))
			{
				Dynamic->SetScalarParameterValue(TEXT("Opacity"), Alpha);
				Dynamic->SetScalarParameterValue(TEXT("Corruption"), Corruption);
			}
		}
		++Shown;
	}

	// Report changes, not every beat: a night refreshes this twice per pressure step.
	if (Shown != LastShownCount)
	{
		LastShownCount = Shown;
		UE_LOG(LogTemp, Log, TEXT("CorruptionVisualizer: %d of %d ritual point(s) now show a visible bloom."),
			Shown, PointCount);
	}
}
