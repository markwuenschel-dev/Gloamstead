// Gloamstead MeshForge Adapter (Wave 6A) — source-level proofs.
//
//  1. Contract/overclaim (headless): descriptors and proxy instances cannot claim generated ownership or a
//     generated asset path they do not have; the GMF validators fail closed.
//  2. Report validation (headless): a report missing the Heart / ritual coverage, touching binary content, or
//     over-claiming generated assets is rejected.
//  3. LIVE world: the adapter spawns visible engine-primitive proxies (Heart + ritual points) bound to real
//     sources, WITHOUT mutating gameplay state, and emits an honest report.
#include "Misc/AutomationTest.h"
#include "Data/GloamsteadMeshForgeTypes.h"
#include "Systems/GloamsteadMeshForgeProvider.h"
#include "Systems/GloamsteadMeshForgeAdapterSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/VeilHeart.h"
#include "Settings/GloamsteadGeneratedAssetSettings.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/RitualTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGloamsteadMeshForgeProxyInstance MakeCleanRuntimeInstance()
	{
		FGloamsteadMeshForgeProxyInstance I;
		I.Spec.ProxyId = TEXT("heart");
		I.Spec.ProxyType = EGMFProxyType::Heart;
		I.Binding.SourceSystem = EGMFSourceSystem::VeilHeart;
		I.ProviderType = EGMFProviderType::EnginePrimitiveRuntimeProxy;
		I.OwnershipClass = EGMFOwnershipClass::CodeOwnedRuntimeProxy;
		I.bRuntimeOnly = true;
		I.GeneratedAssetPath = FString();
		I.bSpawned = true;
		I.bVisibleProxyCreated = true;
		return I;
	}

	FGloamsteadMeshForgeProxyInstance MakeCleanGeneratedInstance()
	{
		FGloamsteadMeshForgeProxyInstance I;
		I.Spec.ProxyId = TEXT("generated_heart");
		I.Spec.ProxyType = EGMFProxyType::Heart;
		I.Binding.SourceSystem = EGMFSourceSystem::VeilHeart;
		I.ProviderType = EGMFProviderType::GeneratedOwnedMeshForgeAsset;
		I.OwnershipClass = EGMFOwnershipClass::GeneratedOwned;
		I.bRuntimeOnly = false;
		I.GeneratedVersionRoot = TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1");
		I.GeneratedAssetPath = I.GeneratedVersionRoot + TEXT("/SM_Heart.SM_Heart");
		I.GeneratedBundleId = TEXT("sanctuary-v1");
		I.GeneratedReceiptSha256 = TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
		I.GeneratedObjectSha256 = TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		I.GeneratedOwnershipId = TEXT("gloamstead");
		I.GeneratedLicenseId = TEXT("LicenseRef-001");
		I.bSpawned = true;
		I.bVisibleProxyCreated = true;
		return I;
	}
}

// 1. Descriptor + instance overclaim rejection.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamMeshForgeContractTest,
	"Gloamstead.MeshForge.ContractsRejectOverclaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamMeshForgeContractTest::RunTest(const FString& /*Parameters*/)
{
	// The engine-primitive provider's own descriptor is honest.
	UGloamsteadEnginePrimitiveMeshForgeProvider* Provider = NewObject<UGloamsteadEnginePrimitiveMeshForgeProvider>();
	const FGloamsteadMeshForgeProviderDescriptor D = Provider->GetDescriptor();
	TestEqual(TEXT("engine-primitive descriptor is valid"), GMFValidateDescriptor(D).Num(), 0);
	TestTrue(TEXT("provider type is engine_primitive"), GMFProviderTypeToken(D.ProviderType) == TEXT("engine_primitive_runtime_proxy"));
	TestTrue(TEXT("ownership is code_owned"), GMFOwnershipClassToken(D.OwnershipClass) == TEXT("code_owned_runtime_proxy"));
	TestFalse(TEXT("does not support generated assets"), D.bSupportsGeneratedAssets);

	// Descriptor overclaims.
	FGloamsteadMeshForgeProviderDescriptor Own = D; Own.OwnershipClass = EGMFOwnershipClass::GeneratedOwned;
	TestTrue(TEXT("runtime provider claiming generated ownership -> GMF014"), GMFValidateDescriptor(Own).Contains(TEXT("GMF014")));
	FGloamsteadMeshForgeProviderDescriptor Gen = D; Gen.bSupportsGeneratedAssets = true;
	TestTrue(TEXT("runtime provider claiming generated support -> GMF014"), GMFValidateDescriptor(Gen).Contains(TEXT("GMF014")));
	FGloamsteadMeshForgeProviderDescriptor NoId = D; NoId.ProviderId = FString();
	TestTrue(TEXT("undeclared provider id -> GMF002"), GMFValidateDescriptor(NoId).Contains(TEXT("GMF002")));

	// A clean runtime instance validates.
	const FGloamsteadMeshForgeProxyInstance Clean = MakeCleanRuntimeInstance();
	TestEqual(TEXT("clean runtime instance is valid"), GMFValidateInstance(Clean).Num(), 0);

	// Instance overclaims.
	FGloamsteadMeshForgeProxyInstance OwnI = MakeCleanRuntimeInstance(); OwnI.OwnershipClass = EGMFOwnershipClass::GeneratedOwned;
	TestTrue(TEXT("engine-primitive claiming generated ownership -> GMF014"), GMFValidateInstance(OwnI).Contains(TEXT("GMF014")));
	FGloamsteadMeshForgeProxyInstance PathI = MakeCleanRuntimeInstance(); PathI.GeneratedAssetPath = TEXT("/Game/Fake");
	TestTrue(TEXT("runtime proxy with an asset path -> GMF015"), GMFValidateInstance(PathI).Contains(TEXT("GMF015")));
	FGloamsteadMeshForgeProxyInstance NoBind = MakeCleanRuntimeInstance(); NoBind.Binding.SourceSystem = EGMFSourceSystem::None;
	TestTrue(TEXT("no source binding -> GMF004"), GMFValidateInstance(NoBind).Contains(TEXT("GMF004")));
	FGloamsteadMeshForgeProxyInstance Ghost = MakeCleanRuntimeInstance(); Ghost.bSpawned = false; Ghost.bVisibleProxyCreated = true;
	TestTrue(TEXT("visible-without-spawn -> GMF009"), GMFValidateInstance(Ghost).Contains(TEXT("GMF009")));
	FGloamsteadMeshForgeProxyInstance NoSpec = MakeCleanRuntimeInstance(); NoSpec.Spec.ProxyId = FString();
	TestTrue(TEXT("empty spec id -> GMF008"), GMFValidateInstance(NoSpec).Contains(TEXT("GMF008")));
	return true;
}

// 2. Aggregate report validation.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamMeshForgeReportValidationTest,
	"Gloamstead.MeshForge.ReportValidationFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamMeshForgeReportValidationTest::RunTest(const FString& /*Parameters*/)
{
	FGloamsteadMeshForgeVisibilityReport R;
	R.ProviderType = EGMFProviderType::EnginePrimitiveRuntimeProxy;
	R.OwnershipClass = EGMFOwnershipClass::CodeOwnedRuntimeProxy;
	R.ProxyCount = 3;
	R.RuntimeOnlyProxyCount = 3;
	R.HeartProxyCount = 1;
	R.RitualPointProxyCount = 1;
	R.GeneratedAssetCount = 0;
	R.bBinaryContentTouched = false;
	TestEqual(TEXT("clean engine-primitive report is valid"), GMFValidateReport(R).Num(), 0);

	FGloamsteadMeshForgeVisibilityReport NoHeart = R; NoHeart.HeartProxyCount = 0;
	TestTrue(TEXT("no heart proxy -> GMF010"), GMFValidateReport(NoHeart).Contains(TEXT("GMF010")));
	FGloamsteadMeshForgeVisibilityReport NoRitual = R; NoRitual.RitualPointProxyCount = 0;
	TestTrue(TEXT("no ritual proxy -> GMF011"), GMFValidateReport(NoRitual).Contains(TEXT("GMF011")));
	FGloamsteadMeshForgeVisibilityReport Bin = R; Bin.bBinaryContentTouched = true;
	TestTrue(TEXT("binary content touched -> GMF016"), GMFValidateReport(Bin).Contains(TEXT("GMF016")));
	FGloamsteadMeshForgeVisibilityReport Gen = R; Gen.GeneratedAssetCount = 1;
	TestTrue(TEXT("generated assets on runtime provider -> GMF015"), GMFValidateReport(Gen).Contains(TEXT("GMF015")));
	FGloamsteadMeshForgeVisibilityReport Own = R; Own.OwnershipClass = EGMFOwnershipClass::GeneratedOwned;
	TestTrue(TEXT("runtime report claiming generated ownership -> GMF014"), GMFValidateReport(Own).Contains(TEXT("GMF014")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamMeshForgeGeneratedProvenanceTest,
	"Gloamstead.MeshForge.GeneratedProvenanceIsReceiptClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamMeshForgeGeneratedProvenanceTest::RunTest(const FString& /*Parameters*/)
{
	FGloamsteadMeshForgeProxyInstance Instance = MakeCleanGeneratedInstance();
	TestEqual(TEXT("closed generated instance is valid"), GMFValidateInstance(Instance).Num(), 0);

	FGloamsteadMeshForgeProxyInstance Escaped = Instance;
	Escaped.GeneratedAssetPath = TEXT("/Game/Other/SM_Heart.SM_Heart");
	TestTrue(TEXT("version-root escape -> GMF020"), GMFValidateInstance(Escaped).Contains(TEXT("GMF020")));

	FGloamsteadMeshForgeVisibilityReport Report;
	Report.ProviderType = EGMFProviderType::GeneratedOwnedMeshForgeAsset;
	Report.OwnershipClass = EGMFOwnershipClass::GeneratedOwned;
	Report.ProxyCount = 2;
	Report.HeartProxyCount = 1;
	Report.RitualPointProxyCount = 1;
	Report.GeneratedAssetCount = 2;
	Report.ActiveGeneratedVersionRoot = Instance.GeneratedVersionRoot;
	Report.ActiveGeneratedBundleId = Instance.GeneratedBundleId;
	Report.ActiveGeneratedReceiptSha256 = Instance.GeneratedReceiptSha256;
	Report.Proxies.Add(Instance);
	FGloamsteadMeshForgeProxyInstance Ritual = Instance;
	Ritual.Spec.ProxyId = TEXT("generated_ritual");
	Ritual.Spec.ProxyType = EGMFProxyType::RitualPoint;
	Ritual.GeneratedAssetPath = Ritual.GeneratedVersionRoot + TEXT("/SM_Ritual.SM_Ritual");
	Report.Proxies.Add(Ritual);
	TestEqual(TEXT("receipt-closed generated report is valid"), GMFValidateReport(Report).Num(), 0);

	Report.Proxies[1].bSpawned = false;
	Report.Proxies[1].bVisibleProxyCreated = false;
	Report.GeneratedAssetCount = 1;
	TestTrue(TEXT("unspawned generated attempt is not visibility coverage -> GMF024"),
		GMFValidateReport(Report).Contains(TEXT("GMF024")));
	Report.Proxies[1].bSpawned = true;
	Report.Proxies[1].bVisibleProxyCreated = true;
	Report.GeneratedAssetCount = 2;

	Report.Proxies[1].GeneratedReceiptSha256 = TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
	TestTrue(TEXT("mixed receipts -> GMF019"), GMFValidateReport(Report).Contains(TEXT("GMF019")));
	return true;
}

// 3. LIVE world: proxies spawn visibly from real sources; gameplay state is untouched; report is honest.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamMeshForgeLiveWorldTest,
	"Gloamstead.MeshForge.AdapterSpawnsVisibleProxiesInLiveWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamMeshForgeLiveWorldTest::RunTest(const FString& /*Parameters*/)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("live world created"), World))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	UGloamsteadMeshForgeAdapterSubsystem* Adapter = World->GetSubsystem<UGloamsteadMeshForgeAdapterSubsystem>();
	const bool bReady = PCG && Adapter;
	TestTrue(TEXT("PCG + adapter subsystems present"), bReady);

	if (bReady)
	{
		// Seed 3 ritual points: index 0 restored, index 1 corrupted, index 2 restorable.
		PCG->Test_SeedPoints(
			{ FVector(0,0,0), FVector(300,0,0), FVector(600,0,0) },
			{ 0.15f, 1.4f, -0.2f },
			{ TEXT("ash_remembers_water"), NAME_None, TEXT("lantern_warns") });
		TArray<FRitualPointState> States;
		{
			FRitualPointState A; A.bIsRestored = true;  A.LightLevel = 0.6f; A.CorruptionLevel = 0.1f; States.Add(A);
			FRitualPointState B; B.bIsRestored = false; B.LightLevel = 0.2f; B.CorruptionLevel = 0.7f; States.Add(B);
			FRitualPointState C; C.bIsRestored = false; C.LightLevel = 0.3f; C.CorruptionLevel = 0.1f; States.Add(C);
		}
		PCG->Test_SeedPointStates(States);

		AVeilHeart* Heart = World->SpawnActor<AVeilHeart>();
		TestNotNull(TEXT("Heart spawned"), Heart);

		// Capture gameplay state before the adapter runs (it must not mutate it).
		const TArray<FRitualPointState> Before = PCG->Test_PeekPointStates();
		TArray<float> WetnessBefore;
		TArray<FName> WarningsBefore;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FPCGPoint Point;
			PCG->GetPointByIndex(Index, Point);
			WetnessBefore.Add(PCG->GetFloatAttribute(Point, TEXT("Wetness"), -1.f));
			WarningsBefore.Add(PCG->GetNameAttribute(Point, TEXT("RecommendedForWarning"), NAME_None));
		}

		Adapter->Test_BuildFor(World);

		// Heart + all three ritual points are visible.
		TestTrue(TEXT("a Heart proxy spawned"), Adapter->CountProxiesOfType(EGMFProxyType::Heart) >= 1);
		const int32 RitualProxies = Adapter->CountProxiesOfType(EGMFProxyType::RitualPoint)
			+ Adapter->CountProxiesOfType(EGMFProxyType::LanternRestore);
		TestEqual(TEXT("all three ritual points got a proxy"), RitualProxies, 3);
		TestTrue(TEXT("an interaction-radius proxy spawned"), Adapter->CountProxiesOfType(EGMFProxyType::InteractionRadius) >= 1);

		int32 Visible = 0;
		for (const FGloamsteadMeshForgeProxyInstance& I : Adapter->GetProxies())
		{
			if (I.bSpawned && I.bVisibleProxyCreated) { ++Visible; }
		}
		TestTrue(TEXT("proxies are visible (mesh assigned)"), Visible == Adapter->GetProxies().Num() && Visible > 0);
		TMap<int32, const FGloamsteadMeshForgeProxyInstance*> PointProxies;
		for (const FGloamsteadMeshForgeProxyInstance& I : Adapter->GetProxies())
		{
			if (I.Binding.SourceSystem == EGMFSourceSystem::PCGSubsystem)
			{
				PointProxies.Add(I.Binding.SourcePointIndex, &I);
			}
		}
		TestTrue(TEXT("point zero warning is projected"), PointProxies.Contains(0)
			&& PointProxies[0]->Spec.ProjectedWarningTag == TEXT("ash_remembers_water"));
		TestTrue(TEXT("point zero wetness is projected"), PointProxies.Contains(0)
			&& FMath::IsNearlyEqual(PointProxies[0]->Spec.ProjectedWetness, 0.15f));
		TestTrue(TEXT("wetness projection clamps high source values"), PointProxies.Contains(1)
			&& FMath::IsNearlyEqual(PointProxies[1]->Spec.ProjectedWetness, 1.f));
		TestTrue(TEXT("wetness projection clamps low source values"), PointProxies.Contains(2)
			&& FMath::IsNearlyZero(PointProxies[2]->Spec.ProjectedWetness));

		// Gameplay authority unchanged — the adapter only read.
		const TArray<FRitualPointState>& After = PCG->Test_PeekPointStates();
		TestEqual(TEXT("PCG point count unchanged by the adapter"), After.Num(), Before.Num());
		bool bStateUnchanged = (After.Num() == Before.Num());
		for (int32 i = 0; i < After.Num() && i < Before.Num(); ++i)
		{
			bStateUnchanged &= (After[i].bIsRestored == Before[i].bIsRestored)
				&& FMath::IsNearlyEqual(After[i].CorruptionLevel, Before[i].CorruptionLevel)
				&& FMath::IsNearlyEqual(After[i].LightLevel, Before[i].LightLevel);
		}
		TestTrue(TEXT("adapter did not mutate gameplay state"), bStateUnchanged);
		bool bProjectionSourcesUnchanged = true;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FPCGPoint Point;
			PCG->GetPointByIndex(Index, Point);
			bProjectionSourcesUnchanged &= FMath::IsNearlyEqual(
				PCG->GetFloatAttribute(Point, TEXT("Wetness"), -1.f), WetnessBefore[Index]);
			bProjectionSourcesUnchanged &= PCG->GetNameAttribute(
				Point, TEXT("RecommendedForWarning"), NAME_None) == WarningsBefore[Index];
		}
		TestTrue(TEXT("adapter did not mutate projection-source metadata"), bProjectionSourcesUnchanged);

		// A phase change drives the night-feedback proxy without crashing (live delegate dispatch).
		if (UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>())
		{
			DayNight->SetPhase(EGloamsteadDayPhase::Dusk);
		}

		// The emitted report is honest and complete.
		FString ReportPath;
		TestTrue(TEXT("visibility report emitted"), Adapter->EmitReport(ReportPath));
		const FGloamsteadMeshForgeVisibilityReport Rep = Adapter->BuildVisibilityReport();
		TestEqual(TEXT("live report has no failure codes"), GMFValidateReport(Rep).Num(), 0);
		TestTrue(TEXT("report provider is engine_primitive"), GMFProviderTypeToken(Rep.ProviderType) == TEXT("engine_primitive_runtime_proxy"));
		TestTrue(TEXT("report ownership is code_owned"), GMFOwnershipClassToken(Rep.OwnershipClass) == TEXT("code_owned_runtime_proxy"));
		TestEqual(TEXT("no generated assets claimed"), Rep.GeneratedAssetCount, 0);
		TestEqual(TEXT("all proxies are runtime-only"), Rep.RuntimeOnlyProxyCount, Rep.ProxyCount);
		TestFalse(TEXT("no binary content touched"), Rep.bBinaryContentTouched);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamMeshForgeAmbiguousHeartTest,
	"Gloamstead.MeshForge.RegistryAmbiguityNeverChoosesFirstHeart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamMeshForgeAmbiguousHeartTest::RunTest(const FString& /*Parameters*/)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("world created"), World)) { return false; }
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	UGloamsteadMeshForgeAdapterSubsystem* Adapter = World->GetSubsystem<UGloamsteadMeshForgeAdapterSubsystem>();
	TestNotNull(TEXT("adapter exists"), Adapter);
	AVeilHeart* First = World->SpawnActor<AVeilHeart>(FVector::ZeroVector, FRotator::ZeroRotator);
	AVeilHeart* Second = World->SpawnActor<AVeilHeart>(FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	TestNotNull(TEXT("first heart spawned"), First);
	TestNotNull(TEXT("second heart spawned"), Second);
	if (Adapter)
	{
		Adapter->Test_BuildFor(World);
		TestEqual(TEXT("ambiguous registry result creates no Heart proxy"),
			Adapter->CountProxiesOfType(EGMFProxyType::Heart), 0);
		TestTrue(TEXT("ambiguity is reported"),
			Adapter->BuildVisibilityReport().FailureCodes.Contains(TEXT("GSS007")));
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamMeshForgeProviderSelectionTest,
	"Gloamstead.MeshForge.ProviderSelectionFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamMeshForgeProviderSelectionTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadMeshForgeAdapterSubsystem* Adapter =
		NewObject<UGloamsteadMeshForgeAdapterSubsystem>();
	TestNotNull(TEXT("adapter created"), Adapter);
	if (!Adapter)
	{
		return false;
	}

	AddExpectedError(TEXT("GMF025"), EAutomationExpectedErrorFlags::Contains, 1);
	TestNull(TEXT("missing settings cannot select any provider"),
		Adapter->Test_CreateProviderForSettings(nullptr, /*bPrimitiveFallbackGateOpen*/ true));
	TestTrue(TEXT("missing settings records a typed failure"),
		Adapter->GetAdapterFailureCodes().Contains(TEXT("GMF025")));

	UGloamsteadGeneratedAssetSettings* Invalid =
		NewObject<UGloamsteadGeneratedAssetSettings>();
	Invalid->ProviderMode = static_cast<EGloamsteadMeshForgeProviderMode>(255);
	AddExpectedError(TEXT("GMF026"), EAutomationExpectedErrorFlags::Contains, 1);
	TestNull(TEXT("corrupted enum cannot select the primitive fallback"),
		Adapter->Test_CreateProviderForSettings(Invalid, /*bPrimitiveFallbackGateOpen*/ true));
	TestTrue(TEXT("invalid provider mode records a typed failure"),
		Adapter->GetAdapterFailureCodes().Contains(TEXT("GMF026")));

	UGloamsteadGeneratedAssetSettings* Fallback =
		NewObject<UGloamsteadGeneratedAssetSettings>();
	Fallback->ProviderMode = EGloamsteadMeshForgeProviderMode::EnginePrimitiveDevelopmentFallback;
	AddExpectedError(TEXT("GMF027"), EAutomationExpectedErrorFlags::Contains, 1);
	TestNull(TEXT("primitive fallback remains disabled while its runtime gate is closed"),
		Adapter->Test_CreateProviderForSettings(Fallback, /*bPrimitiveFallbackGateOpen*/ false));
	TestTrue(TEXT("closed primitive gate records a typed failure"),
		Adapter->GetAdapterFailureCodes().Contains(TEXT("GMF027")));

	UGloamsteadMeshForgeProvider* ExplicitFallback =
		Adapter->Test_CreateProviderForSettings(Fallback, /*bPrimitiveFallbackGateOpen*/ true);
	TestNotNull(TEXT("explicit checked fallback selects a provider"), ExplicitFallback);
	TestTrue(TEXT("explicit checked fallback is the engine-primitive provider"),
		ExplicitFallback
		&& ExplicitFallback->GetDescriptor().ProviderType == EGMFProviderType::EnginePrimitiveRuntimeProxy);

	UGloamsteadGeneratedAssetSettings* Generated =
		NewObject<UGloamsteadGeneratedAssetSettings>();
	Generated->ProviderMode = EGloamsteadMeshForgeProviderMode::GeneratedCatalog;
	UGloamsteadMeshForgeProvider* GeneratedProvider =
		Adapter->Test_CreateProviderForSettings(Generated, /*bPrimitiveFallbackGateOpen*/ false);
	TestNotNull(TEXT("generated catalog selects a provider without the primitive gate"), GeneratedProvider);
	TestTrue(TEXT("generated mode selects only the generated provider"),
		GeneratedProvider
		&& GeneratedProvider->GetDescriptor().ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset);

	Generated->Catalog = TSoftObjectPtr<UGloamsteadGeneratedAssetCatalog>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/DA_Catalog.DA_Catalog")));
	Generated->ExpectedActiveBundleId = TEXT("sanctuary-v1");
	Generated->ExpectedReceiptSha256 = TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	Generated->ExpectedTargetBuildIdentitySha256 = TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
	UGloamsteadGeneratedAssetMeshForgeProvider* FirstConfigured =
		Cast<UGloamsteadGeneratedAssetMeshForgeProvider>(
			Adapter->Test_CreateProviderForSettings(Generated, false));
	TestNotNull(TEXT("first generated configuration creates a generated provider"), FirstConfigured);
	if (!FirstConfigured)
	{
		return false;
	}
	const uint64 StaleLoadGeneration = FirstConfigured->Test_BeginPendingCatalogLoad();
	const FSoftObjectPath StaleCatalogPath = Generated->Catalog.ToSoftObjectPath();
	TestTrue(TEXT("identical same-mode settings reuse the provider"),
		Adapter->Test_EnsureProviderForSettings(Generated, false) == FirstConfigured);

	Generated->ExpectedActiveBundleId = TEXT("sanctuary-v2");
	UGloamsteadMeshForgeProvider* BundleDrift =
		Adapter->Test_EnsureProviderForSettings(Generated, false);
	TestTrue(TEXT("same-mode active-pointer drift recreates the provider"), BundleDrift != FirstConfigured);
	TestTrue(TEXT("replaced provider's pending preload is cancelled"),
		FirstConfigured->GetState() == EGMFGeneratedProviderState::Uninitialized);
	bool bStaleCompletionRan = false;
	FirstConfigured->Test_CompleteCatalogLoad(StaleLoadGeneration, StaleCatalogPath, nullptr,
		FSimpleDelegate::CreateLambda([&bStaleCompletionRan]() { bStaleCompletionRan = true; }));
	TestFalse(TEXT("cancelled provider cannot deliver a stale adapter completion"), bStaleCompletionRan);

	Generated->ExpectedReceiptSha256 = TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
	UGloamsteadMeshForgeProvider* ReceiptDrift =
		Adapter->Test_EnsureProviderForSettings(Generated, false);
	TestTrue(TEXT("same-mode receipt drift recreates the provider"), ReceiptDrift != BundleDrift);
	Generated->ExpectedTargetBuildIdentitySha256 =
		TEXT("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
	UGloamsteadMeshForgeProvider* IdentityDrift =
		Adapter->Test_EnsureProviderForSettings(Generated, false);
	TestTrue(TEXT("same-mode runtime identity drift recreates the provider"), IdentityDrift != ReceiptDrift);
	Generated->Catalog = TSoftObjectPtr<UGloamsteadGeneratedAssetCatalog>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v2/DA_Catalog.DA_Catalog")));
	UGloamsteadMeshForgeProvider* CatalogDrift =
		Adapter->Test_EnsureProviderForSettings(Generated, false);
	TestTrue(TEXT("same-mode catalog-path drift recreates the provider"), CatalogDrift != IdentityDrift);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
