// First-night proof-of-loop wiring invariants for AGloamsteadFirstNightDirector:
// system binding, lantern detection, the dusk-until-restoration gate, the Tutorial night,
// and night completion advancing to Dawn. Mirrors the playbook's "Automated wiring test".
#include "Misc/AutomationTest.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/NightConsequenceRuntime.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "UObject/Script.h" // FEditorScriptExecutionGuard

#if WITH_DEV_AUTOMATION_TESTS

// Dynamic-delegate Broadcast dispatches through UObject::ProcessEvent, which is gated off for
// actor UFUNCTIONs in the editor/automation context. The guard re-enables it for these tests so
// the director's bound handlers actually run when a system broadcasts.

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

// The director binds to OnStructureRestored and detects a lantern restoration — and ignores
// the wrong ritual type. Broadcasting through the real delegate proves the AddDynamic wiring.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFirstNightBindsAndDetectsLanternTest,
	"Gloamstead.FirstNight.BindsAndDetectsLantern",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFirstNightBindsAndDetectsLanternTest::RunTest(const FString& /*Parameters*/)
{
	FEditorScriptExecutionGuard ScriptGuard;

	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	UGloamsteadDayNightSubsystem* DayNight = NewObject<UGloamsteadDayNightSubsystem>();
	UNightConsequenceRuntime* Runtime = NewObject<UNightConsequenceRuntime>();

	AGloamsteadFirstNightDirector* Director = NewObject<AGloamsteadFirstNightDirector>();
	Director->Test_BindTo(PCG, DayNight, Runtime);

	// Wrong ritual type is ignored.
	PCG->OnStructureRestored.Broadcast(MakeRestoration(ERitualType::GardenBed));
	TestFalse(TEXT("garden bed does not satisfy the lantern requirement"), Director->IsLanternRestored());
	TestEqual(TEXT("no lantern restore recorded for wrong type"), Director->Test_LanternRestoredCount, 0);

	// Required lantern restoration is detected exactly once.
	PCG->OnStructureRestored.Broadcast(MakeRestoration(ERitualType::LanternPost));
	TestTrue(TEXT("lantern restoration detected via bound delegate"), Director->IsLanternRestored());
	TestEqual(TEXT("lantern restore recorded once"), Director->Test_LanternRestoredCount, 1);

	// Idempotent: a second lantern restoration does not re-fire the beat.
	PCG->OnStructureRestored.Broadcast(MakeRestoration(ERitualType::LanternPost));
	TestEqual(TEXT("repeat lantern restore is ignored"), Director->Test_LanternRestoredCount, 1);
	return true;
}

// Dusk stays locked until the lantern is restored; restoring it advances Day -> Dusk and
// fires the dusk cue. If the lantern is never restored, night never begins.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFirstNightDuskGateTest,
	"Gloamstead.FirstNight.DuskLockedUntilLanternRestored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFirstNightDuskGateTest::RunTest(const FString& /*Parameters*/)
{
	FEditorScriptExecutionGuard ScriptGuard;

	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	UGloamsteadDayNightSubsystem* DayNight = NewObject<UGloamsteadDayNightSubsystem>();

	AGloamsteadFirstNightDirector* Director = NewObject<AGloamsteadFirstNightDirector>();
	Director->Test_BindTo(PCG, DayNight, /*Runtime*/ nullptr);

	TestTrue(TEXT("world opens on Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// Explicit dusk request before restoration is refused (the gate).
	Director->RequestAdvanceToDusk();
	TestTrue(TEXT("dusk request before restoration keeps Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// A non-lantern restoration must not open the gate either.
	PCG->OnStructureRestored.Broadcast(MakeRestoration(ERitualType::GardenBed));
	TestTrue(TEXT("garden bed does not unlock dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// Restoring the lantern advances to Dusk and plays the dusk cue.
	PCG->OnStructureRestored.Broadcast(MakeRestoration(ERitualType::LanternPost));
	TestTrue(TEXT("lantern restoration advances to Dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
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
	FEditorScriptExecutionGuard ScriptGuard;

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

	// Restore lantern -> Dusk.
	PCG->OnStructureRestored.Broadcast(MakeRestoration(ERitualType::LanternPost));
	TestTrue(TEXT("advanced to Dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
	TestEqual(TEXT("lantern restored beat fired"), Director->Test_LanternRestoredCount, 1);
	TestEqual(TEXT("dusk cue fired"), Director->Test_DuskCueCount, 1);

	// Dusk -> Night (driven by the director; in play this is a timer).
	Director->RequestAdvanceToNight();
	TestTrue(TEXT("advanced to Night"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night);

	// The night runtime announces the Tutorial night; the director begins the encroachment beat.
	Runtime->OnNightStarted.Broadcast(ENightConsequenceType::Tutorial);
	TestTrue(TEXT("observed night type is Tutorial"), Director->GetObservedNightType() == ENightConsequenceType::Tutorial);
	TestTrue(TEXT("beat is Night"), Director->GetCurrentBeat() == EFirstNightBeat::Night);
	TestEqual(TEXT("encroachment presented once"), Director->Test_EncroachmentCount, 1);

	// Night completion advances to Dawn and fires the payoff.
	Director->RequestAdvanceToDawn();
	TestTrue(TEXT("advanced to Dawn"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dawn);
	TestTrue(TEXT("beat is Complete"), Director->GetCurrentBeat() == EFirstNightBeat::Complete);
	TestEqual(TEXT("dawn payoff presented once"), Director->Test_DawnPayoffCount, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
