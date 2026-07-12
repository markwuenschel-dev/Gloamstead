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
		PCG->Test_SeedPoints({ FVector(0,0,0), FVector(300,0,0), FVector(600,0,0) });
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

#endif // WITH_DEV_AUTOMATION_TESTS
