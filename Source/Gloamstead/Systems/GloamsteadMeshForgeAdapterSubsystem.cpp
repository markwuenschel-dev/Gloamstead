#include "Systems/GloamsteadMeshForgeAdapterSubsystem.h"
#include "Gloamstead.h"
#include "Systems/GloamsteadMeshForgeProvider.h"
#include "Systems/VeilHeart.h"
#include "Systems/GloamsteadSurveySubjectRegistry.h"
#include "Settings/GloamsteadGeneratedAssetSettings.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

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

	FName PhaseToken(EGloamsteadDayPhase Phase)
	{
		switch (Phase)
		{
		case EGloamsteadDayPhase::Day: return TEXT("day");
		case EGloamsteadDayPhase::Dusk: return TEXT("dusk");
		case EGloamsteadDayPhase::Night: return TEXT("night");
		case EGloamsteadDayPhase::Dawn: return TEXT("dawn");
		default: return NAME_None;
		}
	}

	FString BuildProviderConfigurationFingerprint(
		const UGloamsteadGeneratedAssetSettings& Settings,
		bool bPrimitiveFallbackGateOpen)
	{
		if (Settings.ProviderMode == EGloamsteadMeshForgeProviderMode::EnginePrimitiveDevelopmentFallback)
		{
			return FString::Printf(TEXT("primitive@1|gate=%d"), bPrimitiveFallbackGateOpen ? 1 : 0);
		}
		return FString::Printf(TEXT("generated@1|catalog=%s|bundle=%s|receipt=%s|runtime=%s"),
			*Settings.Catalog.ToSoftObjectPath().ToString(),
			*Settings.ExpectedActiveBundleId,
			*Settings.ExpectedReceiptSha256.ToLower(),
			*Settings.ExpectedTargetBuildIdentitySha256.ToLower());
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
	const UGloamsteadGeneratedAssetSettings* Settings = GetDefault<UGloamsteadGeneratedAssetSettings>();
	if (!Settings)
	{
		RejectProviderSelection(TEXT("GMF025"), TEXT("generated-asset settings are unavailable"));
		return;
	}

	bool bPrimitiveFallbackGateOpen = false;
	switch (Settings->ProviderMode)
	{
	case EGloamsteadMeshForgeProviderMode::GeneratedCatalog:
		break;
	case EGloamsteadMeshForgeProviderMode::EnginePrimitiveDevelopmentFallback:
		bPrimitiveFallbackGateOpen = CVarSpawnDebugRitualProxies.GetValueOnGameThread();
		if (!bPrimitiveFallbackGateOpen)
		{
			// A valid development fallback that is deliberately disabled is not a runtime fault.
			// It must nevertheless remain completely inert: no provider and no proxy construction.
			Provider = nullptr;
			return;
		}
		break;
	default:
		RejectProviderSelection(TEXT("GMF026"), TEXT("provider mode is outside the declared enum"));
		return;
	}

	BuildFor(&InWorld, Settings, bPrimitiveFallbackGateOpen);
	if (!Provider)
	{
		return;
	}
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
	ReleaseProvider();
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

#if WITH_DEV_AUTOMATION_TESTS
void UGloamsteadMeshForgeAdapterSubsystem::Test_BuildFor(UWorld* World)
{
	// Automation explicitly exercises the checked development fallback even though the live CVar defaults off.
	BuildFor(World, GetDefault<UGloamsteadGeneratedAssetSettings>(),
		/*bPrimitiveFallbackGateOpen*/ true);
}
#endif

void UGloamsteadMeshForgeAdapterSubsystem::BuildFor(UWorld* World)
{
	const UGloamsteadGeneratedAssetSettings* Settings = GetDefault<UGloamsteadGeneratedAssetSettings>();
	const bool bPrimitiveFallbackGateOpen = Settings
		&& Settings->ProviderMode == EGloamsteadMeshForgeProviderMode::EnginePrimitiveDevelopmentFallback
		&& CVarSpawnDebugRitualProxies.GetValueOnGameThread();
	BuildFor(World, Settings, bPrimitiveFallbackGateOpen);
}

void UGloamsteadMeshForgeAdapterSubsystem::BuildFor(
	UWorld* World,
	const UGloamsteadGeneratedAssetSettings* Settings,
	bool bPrimitiveFallbackGateOpen)
{
#if WITH_DEV_AUTOMATION_TESTS
	++TestBuildInvocationCount;
#endif
	if (!World)
	{
		return;
	}
	ClearProxies();
	AdapterFailureCodes.Reset();
	if (!EnsureProvider(Settings, bPrimitiveFallbackGateOpen))
	{
		return;
	}
	if (UGloamsteadGeneratedAssetMeshForgeProvider* Generated =
		Cast<UGloamsteadGeneratedAssetMeshForgeProvider>(Provider))
	{
		PendingBuildWorld = World;
		if (Generated->GetState() == EGMFGeneratedProviderState::Uninitialized)
		{
			const uint64 ExpectedGeneration = ProviderGeneration;
			const uint64 ExpectedLoadRequestGeneration = ++ProviderLoadRequestGeneration;
			const TWeakObjectPtr<UGloamsteadGeneratedAssetMeshForgeProvider> ExpectedProvider = Generated;
			Generated->PreloadCatalogAsyncWithResult(
				FGloamsteadGeneratedCatalogLoadCompletion::CreateWeakLambda(this,
				[this, ExpectedGeneration, ExpectedLoadRequestGeneration, ExpectedProvider](
					const FGloamsteadGeneratedCatalogLoadResult& Result)
				{
					HandleGeneratedProviderPreloadComplete(ExpectedGeneration,
						ExpectedLoadRequestGeneration, ExpectedProvider, Result);
				}));
			if (Generated->HasFailed()) { AdapterFailureCodes = Generated->GetFailureCodes(); }
			return;
		}
		if (!Generated->IsReadyForBuild())
		{
			if (Generated->HasFailed()) { AdapterFailureCodes = Generated->GetFailureCodes(); }
			return;
		}
		if (!Generated->RevalidateRuntimeIdentity())
		{
			AdapterFailureCodes = Generated->GetFailureCodes();
			return;
		}
	}

	AVeilHeart* Heart = ResolveHeart(World);
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

bool UGloamsteadMeshForgeAdapterSubsystem::EnsureProvider(
	const UGloamsteadGeneratedAssetSettings* Settings,
	bool bPrimitiveFallbackGateOpen)
{
	if (!Settings)
	{
		RejectProviderSelection(TEXT("GMF025"), TEXT("generated-asset settings are unavailable"));
		return false;
	}
	const FString DesiredFingerprint = BuildProviderConfigurationFingerprint(
		*Settings, bPrimitiveFallbackGateOpen);
	if (Provider && ProviderConfigurationFingerprint == DesiredFingerprint)
	{
		switch (Settings->ProviderMode)
		{
		case EGloamsteadMeshForgeProviderMode::GeneratedCatalog:
			return Cast<UGloamsteadGeneratedAssetMeshForgeProvider>(Provider) != nullptr;
		case EGloamsteadMeshForgeProviderMode::EnginePrimitiveDevelopmentFallback:
			return bPrimitiveFallbackGateOpen
				&& Cast<UGloamsteadEnginePrimitiveMeshForgeProvider>(Provider) != nullptr;
		default:
			break;
		}
	}
	if (Provider)
	{
		ReleaseProvider();
	}

	switch (Settings->ProviderMode)
	{
	case EGloamsteadMeshForgeProviderMode::GeneratedCatalog:
		break;
	case EGloamsteadMeshForgeProviderMode::EnginePrimitiveDevelopmentFallback:
		if (!bPrimitiveFallbackGateOpen)
		{
			RejectProviderSelection(TEXT("GMF027"),
				TEXT("engine-primitive development fallback is not enabled by its runtime gate"));
			return false;
		}
		break;
	default:
		RejectProviderSelection(TEXT("GMF026"), TEXT("provider mode is outside the declared enum"));
		return false;
	}

	Provider = CreateProviderForMode(Settings, bPrimitiveFallbackGateOpen);
	if (Provider)
	{
		ProviderConfigurationFingerprint = DesiredFingerprint;
		++ProviderGeneration;
	}
	return Provider != nullptr;
}

void UGloamsteadMeshForgeAdapterSubsystem::ReleaseProvider()
{
	// Retire the adapter request before Deactivate synchronously delivers provider cancellation.
	++ProviderLoadRequestGeneration;
	if (UGloamsteadGeneratedAssetMeshForgeProvider* Generated =
		Cast<UGloamsteadGeneratedAssetMeshForgeProvider>(Provider))
	{
		Generated->Deactivate();
	}
	Provider = nullptr;
	ProviderConfigurationFingerprint.Reset();
	PendingBuildWorld.Reset();
	++ProviderGeneration;
}

UGloamsteadMeshForgeProvider* UGloamsteadMeshForgeAdapterSubsystem::CreateProviderForMode(
	const UGloamsteadGeneratedAssetSettings* Settings,
	bool bPrimitiveFallbackGateOpen)
{
	if (!Settings)
	{
		RejectProviderSelection(TEXT("GMF025"), TEXT("generated-asset settings are unavailable"));
		return nullptr;
	}

	switch (Settings->ProviderMode)
	{
	case EGloamsteadMeshForgeProviderMode::GeneratedCatalog:
		{
			UGloamsteadGeneratedAssetMeshForgeProvider* Generated =
				NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>(this);
			Generated->Configure(*Settings);
			return Generated;
		}
	case EGloamsteadMeshForgeProviderMode::EnginePrimitiveDevelopmentFallback:
		if (bPrimitiveFallbackGateOpen)
		{
			return NewObject<UGloamsteadEnginePrimitiveMeshForgeProvider>(this);
		}
		RejectProviderSelection(TEXT("GMF027"),
			TEXT("engine-primitive development fallback is not enabled by its runtime gate"));
		return nullptr;
	default:
		RejectProviderSelection(TEXT("GMF026"), TEXT("provider mode is outside the declared enum"));
		return nullptr;
	}
}

void UGloamsteadMeshForgeAdapterSubsystem::RejectProviderSelection(
	const TCHAR* FailureCode,
	const TCHAR* Detail)
{
	// Invalidate any provider selected before a settings/configuration change. A stale generated or
	// development provider must never survive a newly invalid selection decision.
	ReleaseProvider();
	AdapterFailureCodes.AddUnique(FailureCode);
	UE_LOG(LogGloamstead, Error, TEXT("MeshForge provider selection failed closed [%s]: %s"),
		FailureCode, Detail);
}

#if WITH_DEV_AUTOMATION_TESTS
UGloamsteadMeshForgeProvider* UGloamsteadMeshForgeAdapterSubsystem::Test_CreateProviderForSettings(
	const UGloamsteadGeneratedAssetSettings* Settings,
	bool bPrimitiveFallbackGateOpen)
{
	ReleaseProvider();
	AdapterFailureCodes.Reset();
	EnsureProvider(Settings, bPrimitiveFallbackGateOpen);
	return Provider;
}

UGloamsteadMeshForgeProvider* UGloamsteadMeshForgeAdapterSubsystem::Test_EnsureProviderForSettings(
	const UGloamsteadGeneratedAssetSettings* Settings,
	bool bPrimitiveFallbackGateOpen)
{
	AdapterFailureCodes.Reset();
	EnsureProvider(Settings, bPrimitiveFallbackGateOpen);
	return Provider;
}

void UGloamsteadMeshForgeAdapterSubsystem::Test_UseProviderForSettings(
	UGloamsteadMeshForgeProvider* InProvider,
	const UGloamsteadGeneratedAssetSettings* Settings,
	bool bPrimitiveFallbackGateOpen)
{
	ReleaseProvider();
	Provider = InProvider;
	ProviderConfigurationFingerprint = Settings
		? BuildProviderConfigurationFingerprint(*Settings, bPrimitiveFallbackGateOpen)
		: FString();
	++ProviderGeneration;
}

void UGloamsteadMeshForgeAdapterSubsystem::Test_BuildForSettings(
	UWorld* World,
	const UGloamsteadGeneratedAssetSettings* Settings,
	bool bPrimitiveFallbackGateOpen)
{
	BuildFor(World, Settings, bPrimitiveFallbackGateOpen);
}
#endif

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

void UGloamsteadMeshForgeAdapterSubsystem::HandleGeneratedProviderPreloadComplete(
	uint64 ExpectedProviderGeneration,
	uint64 ExpectedLoadRequestGeneration,
	TWeakObjectPtr<UGloamsteadGeneratedAssetMeshForgeProvider> ExpectedProvider,
	const FGloamsteadGeneratedCatalogLoadResult& Result)
{
	if (ExpectedProviderGeneration != ProviderGeneration
		|| ExpectedLoadRequestGeneration != ProviderLoadRequestGeneration
		|| ExpectedProvider.Get() != Provider)
	{
		return;
	}
#if WITH_DEV_AUTOMATION_TESTS
	++TestPendingLoadTerminalCount;
	if (Result.Terminal == EGMFGeneratedCatalogLoadTerminal::Accepted)
	{
		++TestAcceptedLoadTerminalCount;
	}
#endif
	UGloamsteadGeneratedAssetMeshForgeProvider* Generated =
		Cast<UGloamsteadGeneratedAssetMeshForgeProvider>(Provider);
	if (!Generated)
	{
		return;
	}
	UWorld* World = PendingBuildWorld.Get();
	PendingBuildWorld.Reset();
	if (Result.Terminal == EGMFGeneratedCatalogLoadTerminal::Rejected)
	{
		AdapterFailureCodes = Generated->GetFailureCodes();
		UE_LOG(LogTemp, Error, TEXT("Generated MeshForge catalog failed closed: %s"),
			*FString::Join(AdapterFailureCodes, TEXT(",")));
		FString ReportPath;
		EmitReport(ReportPath);
		return;
	}
	if (Result.Terminal != EGMFGeneratedCatalogLoadTerminal::Accepted
		|| !Generated->IsCatalogLoadResultCurrent(Result))
	{
		// Cancelled/stale terminals retire the pending adapter request without observing whatever
		// generation the same provider UObject may own now.
		return;
	}
	if (World)
	{
		BuildFor(World);
		FString ReportPath;
		EmitReport(ReportPath);
	}
}

AVeilHeart* UGloamsteadMeshForgeAdapterSubsystem::ResolveHeart(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	UGloamsteadSurveySubjectRegistry* Registry = World->GetSubsystem<UGloamsteadSurveySubjectRegistry>();
	if (!Registry)
	{
		AdapterFailureCodes.AddUnique(TEXT("GSS001"));
		return nullptr;
	}
	FGloamsteadSurveySubject Subject;
	if (!Registry->ResolveSubject(TEXT("sanctuary.heart"), Subject))
	{
		for (const FString& Code : Subject.FailureCodes) { AdapterFailureCodes.AddUnique(Code); }
		return nullptr;
	}
	AActor* ResolvedActor = FindObject<AActor>(nullptr, *Subject.ActorObjectPath);
	AVeilHeart* Heart = Cast<AVeilHeart>(ResolvedActor);
	if (!Heart)
	{
		AdapterFailureCodes.AddUnique(TEXT("GSS008"));
	}
	return Heart;
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
	Spec.GeneratedAssetRole = TEXT("sanctuary.heart");
	Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Before;
	Spec.ProjectedWarningTag = Binding.WarningTag;

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
		Binding.WarningTag = PCG->GetNameAttribute(Point, TEXT("RecommendedForWarning"), NAME_None);
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
			Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Restored;
		}
		else if (Corruption >= 0.5f)
		{
			Spec.ProxyType = EGMFProxyType::RitualPoint;
			Spec.Color = ColCorrupted;
			Spec.bInteractionRelevant = true;
			Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Corrupted;
		}
		else
		{
			Spec.ProxyType = EGMFProxyType::LanternRestore;
			Spec.ProxyId = FString::Printf(TEXT("lantern_%d"), i);
			Spec.Color = ColRestorable;
			Spec.bInteractionRelevant = true;
			Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Before;
		}
		Spec.GeneratedAssetRole = Spec.ProxyType == EGMFProxyType::LanternRestore
			? FName(TEXT("sanctuary.lantern_restore"))
			: FName(TEXT("sanctuary.ritual_point"));
		Spec.ProjectedWetness = FMath::Clamp(PCG->GetFloatAttribute(Point, TEXT("Wetness"), 0.f), 0.f, 1.f);
		Spec.ProjectedWarningTag = Binding.WarningTag;

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
	Spec.GeneratedAssetRole = TEXT("sanctuary.interaction_radius");
	Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Before;
	Spec.ProjectedWarningTag = Binding.WarningTag;

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
	const EGloamsteadDayPhase Phase = Binding.SourceObject.IsValid()
		? CastChecked<UGloamsteadDayNightSubsystem>(Binding.SourceObject.Get())->GetCurrentPhase()
		: EGloamsteadDayPhase::Day;
	Spec.Color = PhaseColor(Phase);
	Spec.Scale = 2.0f;
	Spec.bInteractionRelevant = false;
	Spec.GeneratedAssetRole = TEXT("sanctuary.night_feedback");
	Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Before;
	Spec.ProjectedDayPhase = PhaseToken(Phase);
	Spec.ProjectedWarningTag = Binding.WarningTag;

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
		Proxies[NightFeedbackProxyIndex].Spec.ProjectedDayPhase = PhaseToken(NewPhase);
		if (AGloamsteadMeshForgeProxyActor* Actor = Cast<AGloamsteadMeshForgeProxyActor>(Proxies[NightFeedbackProxyIndex].SpawnedActor.Get()))
		{
			Actor->SetVisualColor(PhaseColor(NewPhase), /*bEmissive*/ true);
		}
	}
}

void UGloamsteadMeshForgeAdapterSubsystem::HandleStructureRestored(const FRestorationEventPayload& Payload)
{
	if (Provider && Provider->GetDescriptor().ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset)
	{
		// Re-resolve the exact Restored catalog key from the now-current read-only gameplay state.
		BuildFor(GetWorld());
		return;
	}
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
	R.FailureCodes = AdapterFailureCodes;
	if (const UGloamsteadGeneratedAssetMeshForgeProvider* Generated =
		Cast<UGloamsteadGeneratedAssetMeshForgeProvider>(Provider))
	{
		for (const FString& Code : Generated->GetFailureCodes()) { R.FailureCodes.AddUnique(Code); }
		if (const UGloamsteadGeneratedAssetCatalog* Catalog = Generated->GetCatalog())
		{
			R.ActiveGeneratedVersionRoot = Catalog->VersionRoot;
			R.ActiveGeneratedBundleId = Catalog->BundleId;
			R.ActiveGeneratedReceiptSha256 = Catalog->ReceiptSha256;
		}
	}

	auto CountVisibleType = [this](EGMFProxyType Type)
	{
		int32 Count = 0;
		for (const FGloamsteadMeshForgeProxyInstance& Instance : Proxies)
		{
			if (Instance.Spec.ProxyType == Type && Instance.bSpawned && Instance.bVisibleProxyCreated)
			{
				++Count;
			}
		}
		return Count;
	};
	const int32 Lantern = CountVisibleType(EGMFProxyType::LanternRestore);
	R.ProxyCount = Proxies.Num();
	R.HeartProxyCount = CountVisibleType(EGMFProxyType::Heart);
	// "Ritual point" coverage counts every point-derived proxy (plain ritual points + lantern-restore markers).
	R.RitualPointProxyCount = CountVisibleType(EGMFProxyType::RitualPoint) + Lantern;
	R.LanternProxyCount = Lantern;
	R.InteractionRadiusProxyCount = CountVisibleType(EGMFProxyType::InteractionRadius);
	R.NightFeedbackProxyCount = CountVisibleType(EGMFProxyType::NightFeedback);

	for (FGloamsteadMeshForgeProxyInstance I : Proxies)
	{
		if (!I.bRuntimeOnly)
		{
			// Should never happen for the engine-primitive provider; validators catch it if it does.
		}
		if (I.bRuntimeOnly) { ++R.RuntimeOnlyProxyCount; }
		if (!I.GeneratedAssetPath.IsEmpty() && I.bSpawned && I.bVisibleProxyCreated) { ++R.GeneratedAssetCount; }
		for (const FString& Code : GMFValidateInstance(I)) { I.FailureCodes.AddUnique(Code); }
		R.Proxies.Add(I);
	}

	R.bBinaryContentTouched = false; // loading a catalog asset authors or mutates no binary content
	for (const FString& Code : GMFValidateReport(R)) { R.FailureCodes.AddUnique(Code); }
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
