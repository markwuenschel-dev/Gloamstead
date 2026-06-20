// First-night proof-of-loop wiring invariants for AGloamsteadFirstNightDirector:
// system binding, lantern detection, the dusk-until-restoration gate, the Tutorial night,
// and night completion advancing to Dawn. Mirrors the playbook's "Automated wiring test".
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

	// Restoring the lantern opens the gate: the director advances the phase authority to Dusk.
	Director->HandleStructureRestored(MakeRestoration(ERitualType::LanternPost));
	TestTrue(TEXT("lantern restoration advances to Dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
	TestTrue(TEXT("lantern recorded as restored"), Director->IsLanternRestored());

	// The dusk readability cue is the director's reaction to the phase change.
	Director->Test_DuskCueCount = 0;
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Day, EGloamsteadDayPhase::Dusk);
	TestTrue(TEXT("beat is Dusk"), Director->GetCurrentBeat() == EFirstNightBeat::Dusk);
	TestEqual(TEXT("dusk cue presented once"), Director->Test_DuskCueCount, 1);
	return true;
}

// Full deterministic sequence: intro presentation, dusk gate, Tutorial night, encroachment,
// and night completion advancing to Dawn with the payoff beat.
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

	// Restore lantern -> the director opens the gate and advances to Dusk.
	Director->HandleStructureRestored(MakeRestoration(ERitualType::LanternPost));
	TestTrue(TEXT("advanced to Dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
	TestEqual(TEXT("lantern restored beat fired"), Director->Test_LanternRestoredCount, 1);

	// Dusk cue on the phase change.
	Director->Test_DuskCueCount = 0;
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Day, EGloamsteadDayPhase::Dusk);
	TestTrue(TEXT("beat is Dusk"), Director->GetCurrentBeat() == EFirstNightBeat::Dusk);
	TestEqual(TEXT("dusk cue fired"), Director->Test_DuskCueCount, 1);

	// Dusk -> Night (driven by the director; in play this is a timer).
	Director->RequestAdvanceToNight();
	TestTrue(TEXT("advanced to Night"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night);

	// The night runtime announces the Tutorial night; the director begins the encroachment beat.
	Director->HandleNightStarted(ENightConsequenceType::Tutorial);
	TestTrue(TEXT("observed night type is Tutorial"), Director->GetObservedNightType() == ENightConsequenceType::Tutorial);
	TestTrue(TEXT("beat is Night"), Director->GetCurrentBeat() == EFirstNightBeat::Night);
	TestEqual(TEXT("encroachment presented once"), Director->Test_EncroachmentCount, 1);

	// Night completion advances to Dawn and fires the payoff.
	Director->RequestAdvanceToDawn();
	TestTrue(TEXT("advanced to Dawn"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dawn);
	Director->Test_DawnPayoffCount = 0;
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Night, EGloamsteadDayPhase::Dawn);
	TestTrue(TEXT("beat is Complete"), Director->GetCurrentBeat() == EFirstNightBeat::Complete);
	TestEqual(TEXT("dawn payoff presented once"), Director->Test_DawnPayoffCount, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
