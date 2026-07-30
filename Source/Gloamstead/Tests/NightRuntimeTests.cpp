// Corrected Wave 2 — real night consequence runtime invariants.
//
// These are worldless and driven directly (no dynamic-multicast Broadcast, which does not dispatch on
// worldless NewObject'd objects in automation): they exercise the night STRATEGIES on seeded PCG state,
// the runtime's type->strategy mapping, and that a night's sanctuary mutation survives save/load.
#include "Misc/AutomationTest.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/NightStrategy.h"
#include "Systems/NightPressureActor.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/RitualTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Seed point states with an explicit per-index corruption profile.
	TArray<FRitualPointState> MakeStates(const TArray<float>& Corruptions, float Light, bool bRestored)
	{
		TArray<FRitualPointState> States;
		States.Reserve(Corruptions.Num());
		for (float Corruption : Corruptions)
		{
			FRitualPointState State;
			State.LightLevel = Light;
			State.CorruptionLevel = Corruption;
			State.bIsRestored = bRestored;
			States.Add(State);
		}
		return States;
	}

	FNightRuntimeContext MakeContext(ENightConsequenceType Type, UGloamsteadPCGSubsystem* PCG)
	{
		FNightRuntimeContext Ctx;
		Ctx.NightType = Type;
		Ctx.DuskSnapshot = PCG->BuildSanctuarySnapshot();
		Ctx.TargetPointIndex = PCG->FindMostCorruptedPointIndex(/*bOnlyUnrestored*/ true);
		if (Ctx.TargetPointIndex >= 0)
		{
			Ctx.TargetStartCorruption = PCG->GetCorruptionLevel(Ctx.TargetPointIndex);
		}
		return Ctx;
	}

	// Seed one restored target (index 0) plus unrestored neighbors — the shape a Retrieval night reacts to.
	TArray<FRitualPointState> MakeRetrievalStates(float TargetCorruption, float TargetLight)
	{
		TArray<FRitualPointState> States;
		FRitualPointState T; T.bIsRestored = true;  T.LightLevel = TargetLight; T.CorruptionLevel = TargetCorruption; States.Add(T);
		FRitualPointState A; A.bIsRestored = false; A.LightLevel = 0.2f;         A.CorruptionLevel = 0.3f;             States.Add(A);
		FRitualPointState B; B.bIsRestored = false; B.LightLevel = 0.2f;         B.CorruptionLevel = 0.2f;             States.Add(B);
		return States;
	}

	// A player restoration action against a specific point, clearing some corruption.
	FRestorationEventPayload MakeRestore(int32 PointIndex, float CorruptionCleared)
	{
		FRestorationEventPayload P;
		P.PointIndex = PointIndex;
		P.CorruptionCleared = CorruptionCleared;
		return P;
	}
}

// Corruption night, player cleanses the bloom -> Success, objective resolved.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightCorruptionSuccessTest,
	"Gloamstead.NightRuntime.CorruptionCleansedIsSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightCorruptionSuccessTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeStates({ 0.8f, 0.1f, 0.1f, 0.1f }, /*Light*/ 0.3f, /*bRestored*/ false));

	UNightCorruptionStrategy* Strategy = NewObject<UNightCorruptionStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Corruption, PCG), PCG);

	const FNightObjective ObjAtStart = Strategy->GetObjective();
	TestTrue(TEXT("objective is CleanseCorruptionBloom"), ObjAtStart.Kind == ENightObjectiveKind::CleanseCorruptionBloom);
	TestEqual(TEXT("target is the most-corrupted point"), ObjAtStart.TargetPointIndex, 0);
	TestFalse(TEXT("objective starts unresolved"), Strategy->IsObjectiveResolved());

	// Night presses the bloom.
	Strategy->ApplyPressureStep(PCG);

	// Player cleanses the bloom (restores the target point, clearing its corruption).
	FRestorationEventPayload Cleanse;
	Cleanse.PointIndex = 0;
	Cleanse.CorruptionCleared = 1.0f;
	PCG->ApplyRestoration(0, Cleanse);
	Strategy->NotifyRestoration(Cleanse, PCG);
	TestTrue(TEXT("cleansing the bloom resolves the objective"), Strategy->IsObjectiveResolved());

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestTrue(TEXT("resolved corruption night is Success"), Outcome.Result == ENightOutcomeResult::Success);
	TestTrue(TEXT("success carries the cleansed tag"), Outcome.ResultTag == FName(TEXT("CorruptionCleansed")));
	TestTrue(TEXT("outcome records objective resolved"), Outcome.bObjectiveResolved);
	return true;
}

// Corruption night, bloom untouched -> Failure, scar, bloom worsened.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightCorruptionFailureTest,
	"Gloamstead.NightRuntime.CorruptionUntouchedIsFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightCorruptionFailureTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeStates({ 0.5f, 0.2f, 0.2f }, /*Light*/ 0.2f, /*bRestored*/ false));

	UNightCorruptionStrategy* Strategy = NewObject<UNightCorruptionStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Corruption, PCG), PCG);

	// Night presses, player never acts.
	Strategy->ApplyPressureStep(PCG);
	Strategy->ApplyPressureStep(PCG);

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestFalse(TEXT("objective is unresolved on failure"), Strategy->IsObjectiveResolved());
	TestTrue(TEXT("untouched corruption night is Failure"), Outcome.Result == ENightOutcomeResult::Failure);
	TestTrue(TEXT("failure carries the scar tag"), Outcome.ResultTag == FName(TEXT("CorruptionScar")));
	TestTrue(TEXT("bloom worsened over the night"), Outcome.TargetCorruptionDelta > 0.f);
	return true;
}

// Corruption night, bloom reduced but not cleared -> Partial.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightCorruptionPartialTest,
	"Gloamstead.NightRuntime.CorruptionReducedIsPartial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightCorruptionPartialTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeStates({ 0.6f, 0.1f }, /*Light*/ 0.3f, /*bRestored*/ false));

	UNightCorruptionStrategy* Strategy = NewObject<UNightCorruptionStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Corruption, PCG), PCG);
	// Cleanse threshold for a 0.6 bloom is min(0.3, 0.2) = 0.2.

	// Partial cleanse: reduce the bloom below start but above the resolve threshold, without restoring it.
	PCG->AddCorruptionAtIndex(0, -0.25f); // 0.6 -> 0.35 (> 0.2, so not resolved)

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestFalse(TEXT("partial night leaves the objective unresolved"), Strategy->IsObjectiveResolved());
	TestTrue(TEXT("reduced-but-not-cleared corruption night is Partial"), Outcome.Result == ENightOutcomeResult::Partial);
	TestTrue(TEXT("partial carries the lingers tag"), Outcome.ResultTag == FName(TEXT("CorruptionLingers")));
	TestTrue(TEXT("bloom was reduced"), Outcome.TargetCorruptionDelta < 0.f);
	return true;
}

// No corrupted/unrestored point -> quiet corruption night -> Success (nothing threatened).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightCorruptionNoBloomTest,
	"Gloamstead.NightRuntime.CorruptionNoBloomIsQuiet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightCorruptionNoBloomTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	// All points already restored -> FindMostCorruptedPointIndex(bOnlyUnrestored) returns -1.
	PCG->Test_SeedPointStates(MakeStates({ 0.4f, 0.4f }, /*Light*/ 0.6f, /*bRestored*/ true));

	UNightCorruptionStrategy* Strategy = NewObject<UNightCorruptionStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Corruption, PCG), PCG);
	TestTrue(TEXT("no bloom -> objective None"), Strategy->GetObjective().Kind == ENightObjectiveKind::None);
	TestTrue(TEXT("no bloom -> already resolved"), Strategy->IsObjectiveResolved());

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestTrue(TEXT("a quiet night is Success"), Outcome.Result == ENightOutcomeResult::Success);
	TestTrue(TEXT("quiet night carries the quiet tag"), Outcome.ResultTag == FName(TEXT("QuietNight")));
	return true;
}

// Tutorial night is a bounded, always-winnable teaching beat.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightTutorialSuccessTest,
	"Gloamstead.NightRuntime.TutorialAlwaysResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightTutorialSuccessTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeStates({ 0.3f, 0.3f, 0.3f, 0.3f }, /*Light*/ 0.4f, /*bRestored*/ false));

	UNightTutorialStrategy* Strategy = NewObject<UNightTutorialStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Tutorial, PCG), PCG);
	TestTrue(TEXT("objective is TutorialTeach"), Strategy->GetObjective().Kind == ENightObjectiveKind::TutorialTeach);

	// Teaching spread is bounded: repeated steps do not keep escalating.
	Strategy->ApplyPressureStep(PCG);
	const float AfterFirst = PCG->GetSanctuaryAverageCorruptionLevel();
	Strategy->ApplyPressureStep(PCG);
	const float AfterSecond = PCG->GetSanctuaryAverageCorruptionLevel();
	TestEqual(TEXT("tutorial spread applies only once"), AfterSecond, AfterFirst, KINDA_SMALL_NUMBER);

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestTrue(TEXT("tutorial night always resolves"), Strategy->IsObjectiveResolved());
	TestTrue(TEXT("tutorial night is Success"), Outcome.Result == ENightOutcomeResult::Success);
	TestTrue(TEXT("tutorial carries its completion tag"), Outcome.ResultTag == FName(TEXT("TutorialComplete")));
	return true;
}

// The runtime maps night types to the right strategy classes; unsupported types fall back to benign.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightRuntimeStrategyMappingTest,
	"Gloamstead.NightRuntime.StrategyMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightRuntimeStrategyMappingTest::RunTest(const FString& /*Parameters*/)
{
	UNightConsequenceRuntime* Runtime = NewObject<UNightConsequenceRuntime>();

	UNightStrategy* Tutorial = Runtime->Test_MakeStrategyFor(ENightConsequenceType::Tutorial);
	UNightStrategy* Corruption = Runtime->Test_MakeStrategyFor(ENightConsequenceType::Corruption);
	UNightStrategy* Omen = Runtime->Test_MakeStrategyFor(ENightConsequenceType::Omen);
	UNightStrategy* Retrieval = Runtime->Test_MakeStrategyFor(ENightConsequenceType::Retrieval);
	UNightStrategy* Mirror = Runtime->Test_MakeStrategyFor(ENightConsequenceType::Mirror);

	TestTrue(TEXT("Tutorial -> UNightTutorialStrategy"), Tutorial && Tutorial->IsA(UNightTutorialStrategy::StaticClass()));
	TestTrue(TEXT("Corruption -> UNightCorruptionStrategy"), Corruption && Corruption->IsA(UNightCorruptionStrategy::StaticClass()));
	TestTrue(TEXT("Omen -> UNightOmenStrategy"), Omen && Omen->IsA(UNightOmenStrategy::StaticClass()));
	TestTrue(TEXT("Retrieval -> UNightRetrievalStrategy"), Retrieval && Retrieval->IsA(UNightRetrievalStrategy::StaticClass()));
	TestTrue(TEXT("still-unimplemented (Mirror) -> benign base UNightStrategy"), Mirror && Mirror->GetClass() == UNightStrategy::StaticClass());

	TestTrue(TEXT("no outcome before any night runs"), Runtime->GetLastOutcome().Result == ENightOutcomeResult::None);
	Runtime->EndNight(); // safe no-op when no night is active
	return true;
}

// A corruption night's escalated sanctuary state survives the existing save/load path.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightMutationPersistsTest,
	"Gloamstead.NightRuntime.NightMutationSurvivesSaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightMutationPersistsTest::RunTest(const FString& /*Parameters*/)
{
	const FString Slot = TEXT("W2_NightContinuity_Test");

	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeStates({ 0.5f, 0.2f, 0.1f }, /*Light*/ 0.2f, /*bRestored*/ false));

	// Run a corruption night's escalation, mutating sanctuary corruption.
	UNightCorruptionStrategy* Strategy = NewObject<UNightCorruptionStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Corruption, PCG), PCG);
	Strategy->ApplyPressureStep(PCG);
	Strategy->ApplyPressureStep(PCG);

	const float PostNightCorruption = PCG->GetSanctuaryAverageCorruptionLevel();
	TestTrue(TEXT("night escalated the sanctuary corruption"), PostNightCorruption > 0.f);

	TestTrue(TEXT("post-night state saves"), PCG->SaveToSlot(Slot));

	// Wipe live state, then load it back.
	PCG->Test_SeedPointStates(MakeStates({ 0.f, 0.f, 0.f }, 0.f, false));
	TestEqual(TEXT("state wiped before load"), PCG->GetSanctuaryAverageCorruptionLevel(), 0.f, KINDA_SMALL_NUMBER);

	TestTrue(TEXT("post-night state loads"), PCG->LoadFromSlot(Slot));
	TestEqual(TEXT("night corruption mutation survived save/load"),
		PCG->GetSanctuaryAverageCorruptionLevel(), PostNightCorruption, KINDA_SMALL_NUMBER);
	return true;
}

// The light-reactive pressure actor's menace scales inversely with sanctuary light.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightPressureMenaceTest,
	"Gloamstead.NightRuntime.PressureMenaceInverseToLight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightPressureMenaceTest::RunTest(const FString& /*Parameters*/)
{
	TestEqual(TEXT("dark sanctuary -> full menace"), ANightPressureActor::ComputeMenaceFromLight(0.f), 1.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("bright sanctuary -> no menace"), ANightPressureActor::ComputeMenaceFromLight(1.f), 0.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("half-lit -> half menace"), ANightPressureActor::ComputeMenaceFromLight(0.5f), 0.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("over-bright clamps to zero"), ANightPressureActor::ComputeMenaceFromLight(1.5f), 0.f, KINDA_SMALL_NUMBER);
	return true;
}

// ===== Omen night (Night Types II) =====

// Omen night, player heeds the sign (restores the marked point) -> Success, objective resolved.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightOmenHeededTest,
	"Gloamstead.NightRuntime.OmenHeededIsSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightOmenHeededTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeStates({ 0.7f, 0.1f, 0.1f }, /*Light*/ 0.3f, /*bRestored*/ false));

	UNightOmenStrategy* Strategy = NewObject<UNightOmenStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Omen, PCG), PCG);
	TestTrue(TEXT("objective is HeedOmen"), Strategy->GetObjective().Kind == ENightObjectiveKind::HeedOmen);
	TestEqual(TEXT("omen marks the most-vulnerable point"), Strategy->GetObjective().TargetPointIndex, 0);

	Strategy->ApplyPressureStep(PCG); // the omen deepens

	// Player interprets the sign: restores the MARKED point, clearing its corruption.
	FRestorationEventPayload Heed = MakeRestore(0, 1.0f);
	PCG->ApplyRestoration(0, Heed);
	Strategy->NotifyRestoration(Heed, PCG);
	TestTrue(TEXT("heeding the marked point resolves the omen"), Strategy->IsObjectiveResolved());

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestTrue(TEXT("heeded omen is Success"), Outcome.Result == ENightOutcomeResult::Success);
	TestTrue(TEXT("success carries the heeded tag"), Outcome.ResultTag == FName(TEXT("OmenHeeded")));
	TestTrue(TEXT("the marked point's corruption dropped"), Outcome.TargetCorruptionDelta < 0.f);
	return true;
}

// Omen night, player acts on the wrong point -> Partial (read the region, not the sign).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightOmenCloudedTest,
	"Gloamstead.NightRuntime.OmenMisreadIsPartial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightOmenCloudedTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeStates({ 0.7f, 0.2f }, /*Light*/ 0.3f, /*bRestored*/ false));

	UNightOmenStrategy* Strategy = NewObject<UNightOmenStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Omen, PCG), PCG); // omen marks point 0

	// Player restores a DIFFERENT point (1) — acted, but not on the sign.
	FRestorationEventPayload Elsewhere = MakeRestore(1, 1.0f);
	PCG->ApplyRestoration(1, Elsewhere);
	Strategy->NotifyRestoration(Elsewhere, PCG);

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestFalse(TEXT("misread omen leaves the objective unresolved"), Strategy->IsObjectiveResolved());
	TestTrue(TEXT("acting off-target is Partial"), Outcome.Result == ENightOutcomeResult::Partial);
	TestTrue(TEXT("partial carries the clouded tag"), Outcome.ResultTag == FName(TEXT("OmenClouded")));
	return true;
}

// Omen night, ignored entirely -> Failure, the sign festers into a corruption seed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightOmenIgnoredTest,
	"Gloamstead.NightRuntime.OmenIgnoredIsFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightOmenIgnoredTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeStates({ 0.5f, 0.2f }, /*Light*/ 0.2f, /*bRestored*/ false));

	UNightOmenStrategy* Strategy = NewObject<UNightOmenStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Omen, PCG), PCG);

	Strategy->ApplyPressureStep(PCG);
	Strategy->ApplyPressureStep(PCG); // ignored — the omen deepens each step

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestFalse(TEXT("ignored omen is unresolved"), Strategy->IsObjectiveResolved());
	TestTrue(TEXT("ignored omen is Failure"), Outcome.Result == ENightOutcomeResult::Failure);
	TestTrue(TEXT("failure carries the ignored tag"), Outcome.ResultTag == FName(TEXT("OmenIgnored")));
	TestTrue(TEXT("the ignored omen left a corruption seed"), Outcome.TargetCorruptionDelta > 0.f);
	return true;
}

// ===== Retrieval night (Night Types II) =====

// Retrieval night, player re-stabilizes the reclaimed point -> Success, point stays restored.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightRetrievalRepelledTest,
	"Gloamstead.NightRuntime.RetrievalRepelledIsSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightRetrievalRepelledTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeRetrievalStates(/*TargetCorruption*/ 0.1f, /*TargetLight*/ 0.6f));

	UNightRetrievalStrategy* Strategy = NewObject<UNightRetrievalStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Retrieval, PCG), PCG);
	TestTrue(TEXT("objective is HoldRestored"), Strategy->GetObjective().Kind == ENightObjectiveKind::HoldRestored);
	TestEqual(TEXT("retrieval targets the restored point"), Strategy->GetObjective().TargetPointIndex, 0);

	Strategy->ApplyPressureStep(PCG); // the night gnaws at the mended point

	// Player defends: re-stabilizes the target, clearing the reclaim corruption.
	// The night's grip takes the flag first (RevertRestoration), then the defender re-lights the point.
	// ApplyRestoration refuses a point that is still flagged restored (GloamsteadPCGSubsystem.cpp:303-307)
	// and explicitly allows reclaimed points to be mended again, so this is the shape the guard permits.
	TestTrue(TEXT("the night's grip clears the restored flag"), PCG->RevertRestoration(0));
	FRestorationEventPayload Defend = MakeRestore(0, 1.0f);
	TestTrue(TEXT("re-lighting the reclaimed point is accepted"), PCG->ApplyRestoration(0, Defend));
	Strategy->NotifyRestoration(Defend, PCG);
	TestTrue(TEXT("re-stabilizing resolves the retrieval"), Strategy->IsObjectiveResolved());

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestTrue(TEXT("defended retrieval is Success"), Outcome.Result == ENightOutcomeResult::Success);
	TestTrue(TEXT("success carries the repelled tag"), Outcome.ResultTag == FName(TEXT("RetrievalRepelled")));
	TestTrue(TEXT("the defended point stays restored"), PCG->IsPointRestored(0));
	return true;
}

// Retrieval night, player intervenes but not enough -> Partial, point survives scarred.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightRetrievalSeamTest,
	"Gloamstead.NightRuntime.RetrievalSeamIsPartial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightRetrievalSeamTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeRetrievalStates(0.1f, 0.6f));

	UNightRetrievalStrategy* Strategy = NewObject<UNightRetrievalStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Retrieval, PCG), PCG);

	Strategy->ApplyPressureStep(PCG);
	Strategy->ApplyPressureStep(PCG); // corruption climbs to ~0.3

	// Partial defense: reduces the reclaim but not below the hold threshold. Same reclaim-then-re-light
	// shape as the repelled test — ApplyRestoration will not touch a point that is still flagged restored.
	TestTrue(TEXT("the night's grip clears the restored flag"), PCG->RevertRestoration(0));
	FRestorationEventPayload Partial = MakeRestore(0, 0.15f);
	TestTrue(TEXT("re-lighting the reclaimed point is accepted"), PCG->ApplyRestoration(0, Partial));
	Strategy->NotifyRestoration(Partial, PCG);

	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestFalse(TEXT("scarred retrieval is unresolved"), Strategy->IsObjectiveResolved());
	TestTrue(TEXT("half-held retrieval is Partial"), Outcome.Result == ENightOutcomeResult::Partial);
	TestTrue(TEXT("partial carries the seam tag"), Outcome.ResultTag == FName(TEXT("RetrievalSeam")));
	TestTrue(TEXT("a scarred point is still restored"), PCG->IsPointRestored(0));
	return true;
}

// Retrieval night, no intervention -> Failure, the night reclaims the point (fail-forward).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightRetrievalReclaimedTest,
	"Gloamstead.NightRuntime.RetrievalReclaimedIsFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightRetrievalReclaimedTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPointStates(MakeRetrievalStates(0.1f, 0.6f));

	UNightRetrievalStrategy* Strategy = NewObject<UNightRetrievalStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Retrieval, PCG), PCG);

	Strategy->ApplyPressureStep(PCG);
	Strategy->ApplyPressureStep(PCG);
	Strategy->ApplyPressureStep(PCG); // no defense — the reclaim completes

	TestTrue(TEXT("the point is restored before the reclaim"), PCG->IsPointRestored(0));
	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestFalse(TEXT("reclaimed retrieval is unresolved"), Strategy->IsObjectiveResolved());
	TestTrue(TEXT("unopposed retrieval is Failure"), Outcome.Result == ENightOutcomeResult::Failure);
	TestTrue(TEXT("failure carries the reclaimed tag"), Outcome.ResultTag == FName(TEXT("RetrievalReclaimed")));
	TestFalse(TEXT("the reclaimed point is no longer restored"), PCG->IsPointRestored(0));
	TestTrue(TEXT("the reclaimed point worsened"), Outcome.TargetCorruptionDelta > 0.f);
	return true;
}

// Retrieval night with nothing restored -> honest quiet no-target fallback.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightRetrievalNoTargetTest,
	"Gloamstead.NightRuntime.RetrievalNoTargetFallsBackQuiet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightRetrievalNoTargetTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	// Nothing restored: the night has nothing to reclaim.
	PCG->Test_SeedPointStates(MakeStates({ 0.3f, 0.3f }, /*Light*/ 0.2f, /*bRestored*/ false));

	UNightRetrievalStrategy* Strategy = NewObject<UNightRetrievalStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Retrieval, PCG), PCG);
	TestTrue(TEXT("no restored target -> fallback flagged"), Strategy->bNoTargetFallback);
	TestTrue(TEXT("no restored target -> objective None"), Strategy->GetObjective().Kind == ENightObjectiveKind::None);

	Strategy->ApplyPressureStep(PCG); // fallback applies no pressure
	const FNightRuntimeOutcome Outcome = Strategy->ResolveNight(PCG);
	TestTrue(TEXT("no-target retrieval is Success"), Outcome.Result == ENightOutcomeResult::Success);
	TestTrue(TEXT("no-target carries its fallback tag"), Outcome.ResultTag == FName(TEXT("RetrievalNoTarget")));
	return true;
}

// A retrieval reclaim (un-restore + escalation) survives save/load — continuity for fail-forward state.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightRetrievalContinuityTest,
	"Gloamstead.NightRuntime.RetrievalReclaimSurvivesSaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightRetrievalContinuityTest::RunTest(const FString& /*Parameters*/)
{
	const FString Slot = TEXT("W4_RetrievalContinuity_Test");

	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	// Build the restored state through the real path so PointStates AND RestoredPointIndices stay consistent
	// (Test_SeedPointStates only installs PointStates; it does not populate the restored-index set).
	PCG->Test_SeedPointStates(MakeStates({ 0.1f, 0.3f, 0.2f }, /*Light*/ 0.6f, /*bRestored*/ false));
	PCG->ApplyRestoration(0, MakeRestore(0, /*CorruptionCleared*/ 0.0f)); // genuinely restore point 0 (keeps 0.1 corruption)
	TestEqual(TEXT("one point restored before the night"), PCG->GetRestoredPointCount(), 1);

	UNightRetrievalStrategy* Strategy = NewObject<UNightRetrievalStrategy>();
	Strategy->EnterNight(MakeContext(ENightConsequenceType::Retrieval, PCG), PCG);
	Strategy->ApplyPressureStep(PCG);
	Strategy->ApplyPressureStep(PCG);
	Strategy->ApplyPressureStep(PCG);
	Strategy->ResolveNight(PCG); // reclaims point 0

	TestFalse(TEXT("point reclaimed by the night"), PCG->IsPointRestored(0));
	const int32 PostCount = PCG->GetRestoredPointCount();
	const float PostCorruption = PCG->GetCorruptionLevel(0);

	TestTrue(TEXT("post-reclaim state saves"), PCG->SaveToSlot(Slot));
	PCG->Test_SeedPointStates(MakeRetrievalStates(0.9f, 0.9f)); // wipe to a clearly different state
	TestTrue(TEXT("live state changed before load"), PCG->IsPointRestored(0));

	TestTrue(TEXT("post-reclaim state loads"), PCG->LoadFromSlot(Slot));
	TestFalse(TEXT("reclaim (un-restore) survived save/load"), PCG->IsPointRestored(0));
	TestEqual(TEXT("restored count survived save/load"), PCG->GetRestoredPointCount(), PostCount);
	TestEqual(TEXT("reclaim corruption survived save/load"), PCG->GetCorruptionLevel(0), PostCorruption, KINDA_SMALL_NUMBER);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
