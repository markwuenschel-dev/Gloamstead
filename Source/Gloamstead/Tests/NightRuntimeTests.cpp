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

	TestTrue(TEXT("Tutorial -> UNightTutorialStrategy"), Tutorial && Tutorial->IsA(UNightTutorialStrategy::StaticClass()));
	TestTrue(TEXT("Corruption -> UNightCorruptionStrategy"), Corruption && Corruption->IsA(UNightCorruptionStrategy::StaticClass()));
	TestTrue(TEXT("unsupported (Omen) -> benign base UNightStrategy"), Omen && Omen->GetClass() == UNightStrategy::StaticClass());

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

#endif // WITH_DEV_AUTOMATION_TESTS
