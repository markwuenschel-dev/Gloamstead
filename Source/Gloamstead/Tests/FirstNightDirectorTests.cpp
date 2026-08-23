// First-night proof-of-loop wiring invariants for AGloamsteadFirstNightDirector:
// system binding, lantern detection, the dusk-until-restoration gate, the Tutorial night,
// and permanent detachment once DayNight completes the first dawn. Mirrors the playbook's
// "Automated wiring test".
//
// Binding is verified with IsAlreadyBound (a pure inspection of each delegate's invocation list).
// Behaviour is driven by calling the director's handlers directly: dynamic-multicast Broadcast
// dispatches through UObject::ProcessEvent, which does not run for worldless NewObject'd actors in
// the automation context, so exercising the handlers directly is the reliable way to test the logic.
#include "Misc/AutomationTest.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/NightConsequenceRuntime.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FRestorationEventPayload MakeRestoration(ERitualType Type)
	{
		FRestorationEventPayload Payload;
		Payload.RitualType = Type;
		Payload.WorldLocation = FVector(100.f, 200.f, 0.f);
		Payload.WarningTagSatisfied = FName(TEXT("LightPath"));
		return Payload;
	}
}

// The director registers handlers on all three required systems, and detects a lantern restoration
// (ignoring the wrong ritual type and ignoring repeats).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFirstNightBindsAndDetectsLanternTest,
	"Gloamstead.FirstNight.BindsAndDetectsLantern",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFirstNightBindsAndDetectsLanternTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	UGloamsteadDayNightSubsystem* DayNight = NewObject<UGloamsteadDayNightSubsystem>();
	UNightConsequenceRuntime* Runtime = NewObject<UNightConsequenceRuntime>();

	AGloamsteadFirstNightDirector* Director = NewObject<AGloamsteadFirstNightDirector>();
	Director->Test_BindTo(PCG, DayNight, Runtime);

	// Bound to every required system (the wiring contract). IsAlreadyBound is a macro that takes
	// the member-function pointer, mirroring how BindDelegates() registered each handler.
	TestTrue(TEXT("bound to OnStructureRestored"),
		PCG->OnStructureRestored.IsAlreadyBound(Director, &AGloamsteadFirstNightDirector::HandleStructureRestored));
	TestTrue(TEXT("bound to OnPhaseChanged"),
		DayNight->OnPhaseChanged.IsAlreadyBound(Director, &AGloamsteadFirstNightDirector::HandlePhaseChanged));
	TestTrue(TEXT("bound to OnNightStarted"),
		Runtime->OnNightStarted.IsAlreadyBound(Director, &AGloamsteadFirstNightDirector::HandleNightStarted));

	// Wrong ritual type is ignored.
	Director->HandleStructureRestored(MakeRestoration(ERitualType::GardenBed));
	TestFalse(TEXT("garden bed does not satisfy the lantern requirement"), Director->IsLanternRestored());
	TestEqual(TEXT("no lantern restore recorded for wrong type"), Director->Test_LanternRestoredCount, 0);

	// Required lantern restoration is detected exactly once.
	Director->HandleStructureRestored(MakeRestoration(ERitualType::LanternPost));
	TestTrue(TEXT("lantern restoration detected"), Director->IsLanternRestored());
	TestEqual(TEXT("lantern restore recorded once"), Director->Test_LanternRestoredCount, 1);

	// Idempotent: a second lantern restoration does not re-fire the beat.
	Director->HandleStructureRestored(MakeRestoration(ERitualType::LanternPost));
	TestEqual(TEXT("repeat lantern restore is ignored"), Director->Test_LanternRestoredCount, 1);
	return true;
}

// Dusk stays locked until the lantern is restored; restoring it advances Day -> Dusk and the
// dusk cue plays on the phase change. If the lantern is never restored, night never begins.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFirstNightDuskGateTest,
	"Gloamstead.FirstNight.DuskLockedUntilLanternRestored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFirstNightDuskGateTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	UGloamsteadDayNightSubsystem* DayNight = NewObject<UGloamsteadDayNightSubsystem>();

	AGloamsteadFirstNightDirector* Director = NewObject<AGloamsteadFirstNightDirector>();
	Director->Test_BindTo(PCG, DayNight, /*Runtime*/ nullptr);

	TestTrue(TEXT("world opens on Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// Explicit dusk request before restoration is refused (the gate).
	Director->RequestAdvanceToDusk();
	TestTrue(TEXT("dusk request before restoration keeps Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// A non-lantern restoration must not open the gate either.
	Director->HandleStructureRestored(MakeRestoration(ERitualType::GardenBed));
	TestTrue(TEXT("garden bed does not unlock dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// Restoring the lantern opens the gate. The director does NOT take the transition itself: it unlocks
	// the first rest so the player brings the night by resting at the Heart.
	TestFalse(TEXT("rest is locked before restoration"), DayNight->CanRestNow());
	Director->HandleStructureRestored(MakeRestoration(ERitualType::LanternPost));
	TestTrue(TEXT("lantern recorded as restored"), Director->IsLanternRestored());
	TestTrue(TEXT("lantern restoration unlocks the first rest"), DayNight->IsFirstRestUnlocked());
	TestTrue(TEXT("the Heart will now accept a rest"), DayNight->CanRestNow());
	TestTrue(TEXT("restoration alone does not advance the phase"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// The player's rest is what brings dusk.
	TestTrue(TEXT("rest succeeds"), DayNight->RequestRest());
	TestTrue(TEXT("resting advances to Dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);

	// The dusk readability cue is the director's reaction to the phase change.
	Director->Test_DuskCueCount = 0;
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Day, EGloamsteadDayPhase::Dusk);
	TestTrue(TEXT("beat is Dusk"), Director->GetCurrentBeat() == EFirstNightBeat::Dusk);
	TestEqual(TEXT("dusk cue presented once"), Director->Test_DuskCueCount, 1);
	return true;
}

// Full deterministic sequence: intro presentation, dusk gate, Tutorial night, encroachment,
// and DayNight's cadence advancing to Dawn with the payoff beat.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFirstNightSequenceToDawnTest,
	"Gloamstead.FirstNight.SequenceAdvancesToDawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFirstNightSequenceToDawnTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	UGloamsteadDayNightSubsystem* DayNight = NewObject<UGloamsteadDayNightSubsystem>();
	UNightConsequenceRuntime* Runtime = NewObject<UNightConsequenceRuntime>();

	AGloamsteadFirstNightDirector* Director = NewObject<AGloamsteadFirstNightDirector>();
	Director->Test_BindTo(PCG, DayNight, Runtime);

	// The slice is locked to a Tutorial first night.
	TestTrue(TEXT("first night type is Tutorial"), Director->FirstNightType == ENightConsequenceType::Tutorial);

	// Day intro: warning + lantern target presented.
	Director->Test_BeginIntro();
	TestTrue(TEXT("beat starts at Intro"), Director->GetCurrentBeat() == EFirstNightBeat::Intro);
	TestEqual(TEXT("warning presented once"), Director->Test_WarningCount, 1);
	TestEqual(TEXT("lantern target presented once"), Director->Test_LanternTargetCount, 1);

	// Dusk gate holds until restoration.
	Director->RequestAdvanceToDusk();
	TestTrue(TEXT("still Day before restoration"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// Restore lantern -> the director opens the gate; the player's rest is what advances to Dusk.
	Director->HandleStructureRestored(MakeRestoration(ERitualType::LanternPost));
	TestEqual(TEXT("lantern restored beat fired"), Director->Test_LanternRestoredCount, 1);
	TestTrue(TEXT("first rest unlocked"), DayNight->IsFirstRestUnlocked());
	TestTrue(TEXT("player rests at the Heart"), DayNight->RequestRest());
	TestTrue(TEXT("advanced to Dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);

	// Dusk cue on the phase change.
	Director->Test_DuskCueCount = 0;
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Day, EGloamsteadDayPhase::Dusk);
	TestTrue(TEXT("beat is Dusk"), Director->GetCurrentBeat() == EFirstNightBeat::Dusk);
	TestEqual(TEXT("dusk cue fired"), Director->Test_DuskCueCount, 1);

	// Legacy Blueprint compatibility requests must not bypass the readable Dusk
	// cadence before the tutorial detaches. DayNight is the only authority that
	// may take Dusk -> Night.
	Director->RequestAdvanceToNight();
	TestTrue(TEXT("the active tutorial director cannot bypass Dusk cadence"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);

	// Dusk -> Night is DayNight cadence-owned. The test advances its authority directly;
	// in play its cadence timer takes the same guarded route.
	DayNight->AdvanceToNextPhase();
	TestTrue(TEXT("advanced to Night"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night);

	// The night runtime announces the Tutorial night; the director begins the encroachment beat.
	Director->HandleNightStarted(ENightConsequenceType::Tutorial);
	TestTrue(TEXT("observed night type is Tutorial"), Director->GetObservedNightType() == ENightConsequenceType::Tutorial);
	TestTrue(TEXT("beat is Night"), Director->GetCurrentBeat() == EFirstNightBeat::Night);
	TestEqual(TEXT("encroachment presented once"), Director->Test_EncroachmentCount, 1);

	// The active tutorial director likewise cannot force an early Dawn; only
	// DayNight's deadline/early-objective path owns that transition.
	Director->RequestAdvanceToDawn();
	TestTrue(TEXT("the active tutorial director cannot bypass Night cadence"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night);

	// Night completion advances to Dawn and fires the payoff.
	DayNight->AdvanceToNextPhase();
	TestTrue(TEXT("advanced to Dawn"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dawn);
	Director->Test_DawnPayoffCount = 0;
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Night, EGloamsteadDayPhase::Dawn);
	TestTrue(TEXT("beat is Complete"), Director->GetCurrentBeat() == EFirstNightBeat::Complete);
	TestEqual(TEXT("dawn payoff presented once"), Director->Test_DawnPayoffCount, 1);
	TestTrue(TEXT("the tutorial director permanently detaches after Cycle I dawn"), Director->IsTutorialDetached());
	return true;
}

// The tutorial actor is only a Cycle I lesson. Once its dawn payoff has completed it must
// relinquish all cadence, Heart-copy, and global-presentation participation before Cycle II.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFirstNightDetachesAfterDawnTest,
	"Gloamstead.FirstNight.DetachesAfterTutorialDawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFirstNightDetachesAfterDawnTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	UGloamsteadDayNightSubsystem* DayNight = NewObject<UGloamsteadDayNightSubsystem>();
	UNightConsequenceRuntime* Runtime = NewObject<UNightConsequenceRuntime>();
	AGloamsteadFirstNightDirector* Director = NewObject<AGloamsteadFirstNightDirector>();
	Director->Test_BindTo(PCG, DayNight, Runtime);
	Director->Test_BeginIntro();
	Director->HandleStructureRestored(MakeRestoration(ERitualType::LanternPost));
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Day, EGloamsteadDayPhase::Dusk);
	Director->HandleNightStarted(ENightConsequenceType::Tutorial);

	FNightRuntimeOutcome TutorialOutcome;
	TutorialOutcome.NightType = ENightConsequenceType::Tutorial;
	TutorialOutcome.Result = ENightOutcomeResult::Success;
	Director->HandleHeartDawnReflection(TutorialOutcome);
	TestEqual(TEXT("the Cycle I Heart reflection is delivered before detachment"), Director->Test_HeartReflectionCount, 1);

	Director->HandlePhaseChanged(EGloamsteadDayPhase::Night, EGloamsteadDayPhase::Dawn);
	TestTrue(TEXT("Cycle I dawn permanently detaches the tutorial director"), Director->IsTutorialDetached());
	TestFalse(TEXT("detached director no longer listens for restorations"),
		PCG->OnStructureRestored.IsAlreadyBound(Director, &AGloamsteadFirstNightDirector::HandleStructureRestored));
	TestFalse(TEXT("detached director no longer listens for phase changes"),
		DayNight->OnPhaseChanged.IsAlreadyBound(Director, &AGloamsteadFirstNightDirector::HandlePhaseChanged));
	TestFalse(TEXT("detached director no longer listens for night starts"),
		Runtime->OnNightStarted.IsAlreadyBound(Director, &AGloamsteadFirstNightDirector::HandleNightStarted));
	TestFalse(TEXT("DayNight, not the tutorial director, owns the early-objective delegate"),
		Runtime->OnNightShouldEnd.IsAlreadyBound(Director, &AGloamsteadFirstNightDirector::HandleNightShouldEnd));

	const int32 DuskCueCount = Director->Test_DuskCueCount;
	const int32 EncroachmentCount = Director->Test_EncroachmentCount;
	const int32 DawnPayoffCount = Director->Test_DawnPayoffCount;
	const int32 ReflectionCount = Director->Test_HeartReflectionCount;
	FNightRuntimeOutcome CycleTwoOutcome;
	CycleTwoOutcome.NightType = ENightConsequenceType::Corruption;
	CycleTwoOutcome.Result = ENightOutcomeResult::Failure;

	// Direct calls simulate stale delegate work that arrives after teardown. Every tutorial-only
	// presentation path must stay inert for a later Corruption cycle.
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Day, EGloamsteadDayPhase::Dusk);
	Director->HandleNightStarted(ENightConsequenceType::Corruption);
	Director->HandleHeartDawnReflection(CycleTwoOutcome);
	TestEqual(TEXT("later Dusk cannot replay the lantern cue"), Director->Test_DuskCueCount, DuskCueCount);
	TestEqual(TEXT("later Night cannot replay tutorial encroachment"), Director->Test_EncroachmentCount, EncroachmentCount);
	TestEqual(TEXT("later Dawn cannot replay the tutorial payoff"), Director->Test_DawnPayoffCount, DawnPayoffCount);
	TestEqual(TEXT("later Dawn cannot emit lantern-only reflection copy"), Director->Test_HeartReflectionCount, ReflectionCount);

	DayNight->SetPhase(EGloamsteadDayPhase::Dusk);
	Director->RequestAdvanceToNight();
	TestEqual(TEXT("detached director cannot advance later Dusk"), DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Dusk);
	DayNight->SetPhase(EGloamsteadDayPhase::Night);
	Director->RequestAdvanceToDawn();
	TestEqual(TEXT("detached director cannot advance later Night"), DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Night);
	return true;
}

// The runtime may observe its resolved objective through more than one path. DayNight owns the
// delegate and must collapse every repeat into exactly one Night -> Dawn transition.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamDayNightEarlyObjectiveDawnOneShotTest,
	"Gloamstead.DayNight.EarlyObjectiveCompletesExactlyOneDawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamDayNightEarlyObjectiveDawnOneShotTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadDayNightSubsystem* DayNight = NewObject<UGloamsteadDayNightSubsystem>();
	UNightConsequenceRuntime* Runtime = NewObject<UNightConsequenceRuntime>();
	DayNight->Test_BindCadenceRuntime(Runtime);
	TestTrue(TEXT("DayNight binds the runtime's early-objective delegate"),
		Runtime->OnNightShouldEnd.IsAlreadyBound(DayNight, &UGloamsteadDayNightSubsystem::HandleNightShouldEnd));

	DayNight->SetPhase(EGloamsteadDayPhase::Night);
	DayNight->HandleNightShouldEnd();
	TestEqual(TEXT("the first objective-complete event reaches Dawn"), DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Dawn);
	TestEqual(TEXT("the first objective-complete event requests one Dawn"), DayNight->Test_GetCadenceDawnRequestCount(), 1);
	TestFalse(TEXT("dawn unbinds the stale runtime early-objective delegate"),
		Runtime->OnNightShouldEnd.IsAlreadyBound(DayNight, &UGloamsteadDayNightSubsystem::HandleNightShouldEnd));

	DayNight->HandleNightShouldEnd();
	TestEqual(TEXT("a repeated objective-complete event stays at the already-reached Dawn"), DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Dawn);
	TestEqual(TEXT("a repeated objective-complete event cannot request a second Dawn"), DayNight->Test_GetCadenceDawnRequestCount(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
