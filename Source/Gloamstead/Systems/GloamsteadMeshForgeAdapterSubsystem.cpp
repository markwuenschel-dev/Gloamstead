#include "Systems/GloamsteadMeshForgeAdapterSubsystem.h"
#include "Systems/GloamsteadMeshForgeProvider.h"
#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

namespace
{
	const FLinearColor ColHeart      = FLinearColor(1.00f, 0.75f, 0.20f); // warm gold
	const FLinearColor ColRestored   = FLinearColor(0.20f, 0.90f, 0.35f); // green
	const FLinearColor ColRestorable = FLinearColor(0.95f, 0.70f, 0.15f); // amber "restore me"
	const FLinearColor ColCorrupted  = FLinearColor(0.60f, 0.20f, 0.80f); // purple
	const FLinearColor ColRadius     = FLinearColor(0.20f, 0.70f, 1.00f); // cyan affordance

	FLinearColor PhaseColor(EGloamsteadDayPhase Phase)
	{
		switch (Phase)
		{
		case EGloamsteadDayPhase::Day:   return FLinearColor(0.35f, 0.85f, 1.00f); // bright
		case EGloamsteadDayPhase::Dusk:  return FLinearColor(1.00f, 0.45f, 0.15f); // orange
		case EGloamsteadDayPhase::Night: return FLinearColor(0.25f, 0.10f, 0.45f); // dark purple
		case EGloamsteadDayPhase::Dawn:  return FLinearColor(1.00f, 0.85f, 0.45f); // gold
		default:                         return FLinearColor::White;
		}
	}
}

// Development gate for the runtime debug proxies. The MeshForge visibility adapter is a DIAGNOSTIC overlay
// (Heart pillar, ritual markers, lantern beacons, interaction disc) — not the game's shipping visuals — so it
// is OFF by default. Toggle live with `gloam.MeshForge.SpawnDebugProxies 1`. Automation builds proxies via
// Test_BuildFor(), which bypasses this gate, so tests and the GloamsteadForge evidence are unaffected.
static TAutoConsoleVariable<bool> CVarSpawnDebugRitualProxies(
	TEXT("gloam.MeshForge.SpawnDebugProxies"),
	false,
	TEXT("Spawn the MeshForge visibility adapter's runtime debug proxies in a live game world (default off)."),
	ECVF_Default);

void UGloamsteadMeshForgeAdapterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// The visibility layer only renders in a live game world; automation/editor-preview stay clean.
	if (!InWorld.IsGameWorld())
	{
		return;
	}

	// Runtime debug proxies are gated OFF by default (see CVarSpawnDebugRitualProxies) — they are a
	// diagnostic overlay of abstract primitives, not shipping visuals. Automation bypasses this gate via
	// Test_BuildFor(), so tests/evidence still exercise the full build.
	if (!CVarSpawnDebugRitualProxies.GetValueOnGameThread())
	{
		return;
	}

	BuildFor(&InWorld);
	BindSourceEvents(&InWorld);

	// Only emit the shared, source-controlled report when this world actually produced a sanctuary.
	// The adapter's subsystem is created for EVERY game world (including unrelated automation worlds with
	// no Heart or ritual points); those must not clobber the auditable artifact with an empty, coverage-
	// failing report. The report should reflect a real Heart + ritual-point build, or be left untouched.
	if (Proxies.Num() > 0)
	{
		FString ReportPath;
		EmitReport(ReportPath);
	}

	// PCG points can generate after BeginPlay; retry a few times so ritual markers appear.
	if (CountProxiesOfType(EGMFProxyType::RitualPoint) == 0)
	{
		InWorld.GetTimerManager().SetTimer(RebuildTimer, this,
			&UGloamsteadMeshForgeAdapterSubsystem::RetryRitualProxies, 1.0f, /*bLoop*/ true);
	}
}

void UGloamsteadMeshForgeAdapterSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebuildTimer);
	}
	UnbindSourceEvents();
	Super::Deinitialize();
}

void UGloamsteadMeshForgeAdapterSubsystem::RetryRitualProxies()
{
	++RebuildAttempts;
	BuildFor(GetWorld());
	if (CountProxiesOfType(EGMFProxyType::RitualPoint) > 0 || RebuildAttempts >= 5)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RebuildTimer);
		}
		// Same guard as OnWorldBeginPlay: never persist an empty report from a world with no sanctuary.
		if (Proxies.Num() > 0)
		{
			FString ReportPath;
			EmitReport(ReportPath);
		}
	}
}

void UGloamsteadMeshForgeAdapterSubsystem::BuildProxies()
{
	BuildFor(GetWorld());
}

void UGloamsteadMeshForgeAdapterSubsystem::Test_BuildFor(UWorld* World)
{
	BuildFor(World);
}

void UGloamsteadMeshForgeAdapterSubsystem::BuildFor(UWorld* World)
{
	if (!World)
	{
		return;
	}
	ClearProxies();
	EnsureProvider();

	AVeilHeart* Heart = FindHeart(World);
	if (Heart)
	{
		BuildHeartProxy(World, Heart);
		BuildInteractionRadiusProxy(World, Heart);
		BuildNightFeedbackProxy(World, Heart);
	}

	if (UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>())
	{
		BuildRitualPointProxies(World, PCG);
	}
}

void UGloamsteadMeshForgeAdapterSubsystem::EnsureProvider()
{
	if (!Provider)
	{
		Provider = NewObject<UGloamsteadEnginePrimitiveMeshForgeProvider>(this);
	}
}

void UGloamsteadMeshForgeAdapterSubsystem::ClearProxies()
{
	for (const FGloamsteadMeshForgeProxyInstance& I : Proxies)
	{
		if (I.SpawnedActor.IsValid())
		{
			I.SpawnedActor->Destroy();
		}
	}
	Proxies.Reset();
	NightFeedbackProxyIndex = -1;
}

AVeilHeart* UGloamsteadMeshForgeAdapterSubsystem::FindHeart(UWorld* World) const
{
	if (!World)
	{
		return nullptr;
	}
	TArray<AActor*> Hearts;
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	return Hearts.Num() > 0 ? Cast<AVeilHeart>(Hearts[0]) : nullptr;
}

void UGloamsteadMeshForgeAdapterSubsystem::BuildHeartProxy(UWorld* World, AVeilHeart* Heart)
{
	FGloamsteadMeshForgeSourceBinding Binding;
	Binding.SourceSystem = EGMFSourceSystem::VeilHeart;
	Binding.SourceObject = Heart;
	Binding.WorldLocation = Heart->GetActorLocation() + FVector(0, 0, 120);
	Binding.bLocationResolved = true;

	FGloamsteadMeshForgeProxySpec Spec;
	Spec.ProxyType = EGMFProxyType::Heart;
	Spec.ProxyId = TEXT("heart");
	Spec.Color = ColHeart;
	Spec.Scale = 1.5f;
	Spec.bInteractionRelevant = true;

	Proxies.Add(Provider->CreateProxy(Spec, Binding, World));
}

void UGloamsteadMeshForgeAdapterSubsystem::BuildRitualPointProxies(UWorld* World, UGloamsteadPCGSubsystem* PCG)
{
	const int32 N = PCG->GetRitualPointCount();
	for (int32 i = 0; i < N; ++i)
	{
		FPCGPoint Point;
		if (!PCG->GetPointByIndex(i, Point))
		{
			continue;
		}
		const bool bRestored = PCG->IsPointRestored(i);
		const float Corruption = PCG->GetCorruptionLevel(i);

		FGloamsteadMeshForgeSourceBinding Binding;
		Binding.SourceSystem = EGMFSourceSystem::PCGSubsystem;
		Binding.SourceObject = PCG;
		Binding.SourcePointIndex = i;
		// Record ritual-type provenance, read read-only from the point's PCG metadata (0 == ERitualType::Invalid
		// when a synthetic/unlabelled point carries no attribute). The adapter never writes this attribute.
		Binding.RitualType = static_cast<ERitualType>(PCG->GetIntAttribute(Point, TEXT("RitualType"), 0));
		Binding.WorldLocation = Point.Transform.GetLocation() + FVector(0, 0, 40);
		Binding.bLocationResolved = true;

		FGloamsteadMeshForgeProxySpec Spec;
		Spec.ProxyId = FString::Printf(TEXT("ritual_%d"), i);
		Spec.Scale = 1.0f;
		if (bRestored)
		{
			Spec.ProxyType = EGMFProxyType::RitualPoint;
			Spec.Color = ColRestored;
			Spec.bInteractionRelevant = false;
		}
		else if (Corruption >= 0.5f)
		{
			Spec.ProxyType = EGMFProxyType::RitualPoint;
			Spec.Color = ColCorrupted;
			Spec.bInteractionRelevant = true;
		}
		else
		{
			Spec.ProxyType = EGMFProxyType::LanternRestore;
			Spec.ProxyId = FString::Printf(TEXT("lantern_%d"), i);
			Spec.Color = ColRestorable;
			Spec.bInteractionRelevant = true;
		}

		Proxies.Add(Provider->CreateProxy(Spec, Binding, World));
	}
}

void UGloamsteadMeshForgeAdapterSubsystem::BuildInteractionRadiusProxy(UWorld* World, AVeilHeart* Heart)
{
	FGloamsteadMeshForgeSourceBinding Binding;
	Binding.SourceSystem = EGMFSourceSystem::VeilHeart;
	Binding.SourceObject = Heart;
	Binding.WorldLocation = Heart->GetActorLocation() + FVector(0, 0, 5);
	Binding.bLocationResolved = true;

	FGloamsteadMeshForgeProxySpec Spec;
	Spec.ProxyType = EGMFProxyType::InteractionRadius;
	Spec.ProxyId = TEXT("interaction_radius_heart");
	Spec.Color = ColRadius;
	Spec.Scale = 3.0f;
	Spec.bInteractionRelevant = false;

	Proxies.Add(Provider->CreateProxy(Spec, Binding, World));
}

void UGloamsteadMeshForgeAdapterSubsystem::BuildNightFeedbackProxy(UWorld* World, AVeilHeart* Heart)
{
	FGloamsteadMeshForgeSourceBinding Binding;
	Binding.SourceSystem = EGMFSourceSystem::DayNight;
	Binding.SourceObject = World->GetSubsystem<UGloamsteadDayNightSubsystem>();
	Binding.WorldLocation = Heart->GetActorLocation() + FVector(0, 0, 400);
	Binding.bLocationResolved = true;

	FGloamsteadMeshForgeProxySpec Spec;
	Spec.ProxyType = EGMFProxyType::NightFeedback;
	Spec.ProxyId = TEXT("night_feedback");
	Spec.Color = PhaseColor(EGloamsteadDayPhase::Day);
	Spec.Scale = 2.0f;
	Spec.bInteractionRelevant = false;

	NightFeedbackProxyIndex = Proxies.Num();
	Proxies.Add(Provider->CreateProxy(Spec, Binding, World));
}

void UGloamsteadMeshForgeAdapterSubsystem::BindSourceEvents(UWorld* World)
{
	if (bBound || !World)
	{
		return;
	}
	if (UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>())
	{
		DayNight->OnPhaseChanged.AddDynamic(this, &UGloamsteadMeshForgeAdapterSubsystem::HandlePhaseChanged);
	}
	if (UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>())
	{
		PCG->OnStructureRestored.AddDynamic(this, &UGloamsteadMeshForgeAdapterSubsystem::HandleStructureRestored);
	}
	bBound = true;
}

void UGloamsteadMeshForgeAdapterSubsystem::UnbindSourceEvents()
{
	if (!bBound)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>())
		{
			DayNight->OnPhaseChanged.RemoveDynamic(this, &UGloamsteadMeshForgeAdapterSubsystem::HandlePhaseChanged);
		}
		if (UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>())
		{
			PCG->OnStructureRestored.RemoveDynamic(this, &UGloamsteadMeshForgeAdapterSubsystem::HandleStructureRestored);
		}
	}
	bBound = false;
}

void UGloamsteadMeshForgeAdapterSubsystem::HandlePhaseChanged(EGloamsteadDayPhase /*OldPhase*/, EGloamsteadDayPhase NewPhase)
{
	if (Proxies.IsValidIndex(NightFeedbackProxyIndex))
	{
		if (AGloamsteadMeshForgeProxyActor* Actor = Cast<AGloamsteadMeshForgeProxyActor>(Proxies[NightFeedbackProxyIndex].SpawnedActor.Get()))
		{
			Actor->SetVisualColor(PhaseColor(NewPhase), /*bEmissive*/ true);
		}
	}
}

void UGloamsteadMeshForgeAdapterSubsystem::HandleStructureRestored(const FRestorationEventPayload& Payload)
{
	// The restored point turns green — the player sees their mend take hold.
	for (const FGloamsteadMeshForgeProxyInstance& I : Proxies)
	{
		if (I.Binding.SourceSystem == EGMFSourceSystem::PCGSubsystem && I.Binding.SourcePointIndex == Payload.PointIndex)
		{
			if (AGloamsteadMeshForgeProxyActor* Actor = Cast<AGloamsteadMeshForgeProxyActor>(I.SpawnedActor.Get()))
			{
				Actor->SetVisualColor(ColRestored, /*bEmissive*/ true);
			}
		}
	}
}

int32 UGloamsteadMeshForgeAdapterSubsystem::CountProxiesOfType(EGMFProxyType Type) const
{
	int32 Count = 0;
	for (const FGloamsteadMeshForgeProxyInstance& I : Proxies)
	{
		if (I.Spec.ProxyType == Type)
		{
			++Count;
		}
	}
	return Count;
}

FGloamsteadMeshForgeVisibilityReport UGloamsteadMeshForgeAdapterSubsystem::BuildVisibilityReport() const
{
	FGloamsteadMeshForgeVisibilityReport R;
	R.ReportId = TEXT("gloamstead_meshforge_visibility");

	FGloamsteadMeshForgeProviderDescriptor Desc;
	if (Provider)
	{
		Desc = Provider->GetDescriptor();
	}
	R.ProviderType = Desc.ProviderType;
	R.OwnershipClass = Desc.OwnershipClass;

	const int32 Lantern = CountProxiesOfType(EGMFProxyType::LanternRestore);
	R.ProxyCount = Proxies.Num();
	R.HeartProxyCount = CountProxiesOfType(EGMFProxyType::Heart);
	// "Ritual point" coverage counts every point-derived proxy (plain ritual points + lantern-restore markers).
	R.RitualPointProxyCount = CountProxiesOfType(EGMFProxyType::RitualPoint) + Lantern;
	R.LanternProxyCount = Lantern;
	R.InteractionRadiusProxyCount = CountProxiesOfType(EGMFProxyType::InteractionRadius);
	R.NightFeedbackProxyCount = CountProxiesOfType(EGMFProxyType::NightFeedback);

	for (FGloamsteadMeshForgeProxyInstance I : Proxies)
	{
		if (!I.bRuntimeOnly)
		{
			// Should never happen for the engine-primitive provider; validators catch it if it does.
		}
		if (I.bRuntimeOnly) { ++R.RuntimeOnlyProxyCount; }
		if (!I.GeneratedAssetPath.IsEmpty()) { ++R.GeneratedAssetCount; }
		I.FailureCodes = GMFValidateInstance(I);
		R.Proxies.Add(I);
	}

	R.bBinaryContentTouched = false; // this wave spawns runtime primitives only; it authors nothing
	R.FailureCodes = GMFValidateReport(R);
	return R;
}

bool UGloamsteadMeshForgeAdapterSubsystem::EmitReport(FString& OutPrimaryPath) const
{
	if (!Provider)
	{
		return false;
	}
	const FGloamsteadMeshForgeVisibilityReport Report = BuildVisibilityReport();
	return GloamsteadMeshForgeReport::WriteReports(Report, Provider->GetDescriptor(),
		GloamsteadMeshForgeReport::DefaultReportDir(), OutPrimaryPath);
}
