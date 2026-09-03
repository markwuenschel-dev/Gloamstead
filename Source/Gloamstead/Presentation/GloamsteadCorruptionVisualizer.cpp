#include "Presentation/GloamsteadCorruptionVisualizer.h"

#include "PCG/GloamsteadPCGSubsystem.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
	PointGrowths.Reset();
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

const TCHAR* UGloamsteadCorruptionVisualizer::GetGrowthMeshPathFor(float Corruption)
{
	// Forged by procedural/houdini/forge_gloam_assets.py. Three variants rather than one scaled
	// mesh: a worse bloom is a busier, differently-shaped thing, and scaling a single crystal up
	// only ever reads as "the same rot, nearer".
	if (Corruption >= 0.70f)
	{
		return TEXT("/Game/Gloamstead/World/SM_Gloam_Growth_Large.SM_Gloam_Growth_Large");
	}
	if (Corruption >= 0.40f)
	{
		return TEXT("/Game/Gloamstead/World/SM_Gloam_Growth_Medium.SM_Gloam_Growth_Medium");
	}
	if (Corruption > 0.15f)
	{
		return TEXT("/Game/Gloamstead/World/SM_Gloam_Growth_Small.SM_Gloam_Growth_Small");
	}
	return nullptr;
}

const TCHAR* UGloamsteadCorruptionVisualizer::GetGrowthBaseMaterialPath()
{
	// The forged growths were imported with import_materials=False, which is right - a Houdini FBX
	// should not bring its own shading into this project - but nothing then ASSIGNED one, so every
	// growth rendered with WorldGridMaterial: the grey engine checkerboard. The kit meshes never hit
	// this because AGloamsteadRestoredStructure assigns MI_Sanctuary_* to each one it builds.
	return TEXT("/Game/Gloamstead/Kit/Materials/MI_Sanctuary_Stone_Dark.MI_Sanctuary_Stone_Dark");
}

FLinearColor UGloamsteadCorruptionVisualizer::GetGrowthTintFor(float Corruption)
{
	// Corruption palette from docs/art/04_art_direction.md: bruised blue through black-violet to
	// mineral obsidian. Deliberately NOT an emissive - the art direction reserves luminous accents
	// for restoration, and rot that glows would read as something the player wants.
	const float Alpha = FMath::Clamp((Corruption - 0.15f) / 0.85f, 0.f, 1.f);
	const FLinearColor Bruised(0.44f, 0.38f, 0.58f);
	const FLinearColor Obsidian(0.16f, 0.09f, 0.26f);
	return FMath::Lerp(Bruised, Obsidian, Alpha);
}

int32 UGloamsteadCorruptionVisualizer::Test_GetVisibleGrowthCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<UStaticMeshComponent>& Growth : PointGrowths)
	{
		if (Growth && Growth->IsVisible() && Growth->GetStaticMesh())
		{
			++Count;
		}
	}
	return Count;
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
	PointGrowths.SetNum(PointCount);

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
			if (TObjectPtr<UStaticMeshComponent>& HideGrowth = PointGrowths[Index])
			{
				HideGrowth->SetVisibility(false);
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

		// The forged growth, standing on the same traced surface as the stain. This is the half of
		// corruption the player can see without looking at the floor.
		if (const TCHAR* GrowthPath = GetGrowthMeshPathFor(Corruption))
		{
			TObjectPtr<UStaticMeshComponent>& GrowthSlot = PointGrowths[Index];
			if (!GrowthSlot)
			{
				UStaticMeshComponent* Created = NewObject<UStaticMeshComponent>(DecalHolder);
				Created->SetupAttachment(DecalHolder->GetRootComponent());
				// Rot the player walks past, never into: it must not block a route the sanctuary
				// needs, and a restoration should never be gated on geometry that grew over it.
				Created->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Created->RegisterComponent();
				GrowthSlot = Created;
			}
			if (UStaticMesh* GrowthMesh = LoadObject<UStaticMesh>(nullptr, GrowthPath))
			{
				if (GrowthSlot->GetStaticMesh() != GrowthMesh)
				{
					GrowthSlot->SetStaticMesh(GrowthMesh);
					// Re-created with the mesh: a dynamic instance is bound to the slot count of
					// the mesh it was made for, and the three severity tiers are three meshes.
					GrowthSlot->EmptyOverrideMaterials();
				}

				UMaterialInstanceDynamic* GrowthTint =
					Cast<UMaterialInstanceDynamic>(GrowthSlot->GetMaterial(0));
				if (!GrowthTint)
				{
					if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(
							nullptr, GetGrowthBaseMaterialPath()))
					{
						GrowthTint = UMaterialInstanceDynamic::Create(Base, GrowthSlot);
						if (GrowthTint)
						{
							// Every slot, not just slot 0: a forged mesh with more than one section
							// would otherwise show the checkerboard on all the others.
							const int32 SlotCount = FMath::Max(1, GrowthSlot->GetNumMaterials());
							for (int32 Slot = 0; Slot < SlotCount; ++Slot)
							{
								GrowthSlot->SetMaterial(Slot, GrowthTint);
							}
						}
					}
					else
					{
						UE_LOG(LogTemp, Error,
							TEXT("CorruptionVisualizer: could not load %s, so the gloam growths "
								 "render as the engine grid material."), GetGrowthBaseMaterialPath());
					}
				}
				if (GrowthTint)
				{
					GrowthTint->SetVectorParameterValue(
						TEXT("TintColor"), GetGrowthTintFor(Corruption));
				}
				// Turned by index so a row of corrupted points is not a row of identical crystals.
				GrowthSlot->SetWorldLocation(Surface);
				GrowthSlot->SetWorldRotation(FRotator(0.f, (Index * 47) % 360, 0.f));
				GrowthSlot->SetVisibility(true);
			}
			else if (GrowthSlot->GetStaticMesh() == nullptr)
			{
				UE_LOG(LogTemp, Error,
					TEXT("CorruptionVisualizer: could not load forged growth %s; corruption keeps "
						 "its stain but loses its geometry."), GrowthPath);
			}
		}

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
		UE_LOG(LogTemp, Log,
			TEXT("CorruptionVisualizer: %d of %d ritual point(s) now show a visible bloom "
				 "(%d forged growth(s) standing)."),
			Shown, PointCount, Test_GetVisibleGrowthCount());
	}
}
