// Headless end-to-end integration test for the first-night proof-of-loop (Phase 0 / Phase3 §3.A).
//
// Unlike FirstNightDirectorTests (which verifies director wiring in isolation) and NightBrainTests
// (which scores selection in isolation), this drives the WHOLE chain on SEEDED point data and proves
// the pieces are actually connected:
//
//   seed PCG point states (Test_SeedPointStates, no world/PCG init)
//     -> the production snapshot builder reads those states (BuildSanctuarySnapshot)
//     -> the night brain selects a night type that REFLECTS the seeded snapshot
//        (lit sanctuary -> non-Corruption; neglected/corrupted sanctuary -> Corruption)
//     -> the first-night director runs its Cycle I presentation beats for the selected night,
//        with the encroachment cue scaled by the seeded sanctuary light
//     -> the Veil Heart's dawn reflection consumes and clears the cycle's satisfied warnings.
//
// Everything stays worldless: it uses the documented test seams (Test_SeedPointStates / Test_BindTo /
// Test_BeginIntro / Test_SelectNightType) and calls the director's handlers directly, because dynamic-
// multicast Broadcast does not dispatch on worldless NewObject'd actors in the automation context.
#include "Misc/AutomationTest.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Data/RitualTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Seed a set of uniform point states. Averages over PointStates are the production snapshot's
	// light/corruption inputs, so this directly controls how the night brain scores its rules.
	TArray<FRitualPointState> MakeUniformPointStates(int32 Count, float Light, float Corruption, bool bRestored)
	{
		TArray<FRitualPointState> States;
		States.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			FRitualPointState State;
			State.LightLevel = Light;
			State.CorruptionLevel = Corruption;
			State.bIsRestored = bRestored;
			States.Add(State);
		}
		return States;
	}

	// A two-rule catalog whose outcome is decided purely by the seeded averages (no always-on Tutorial
	// rule). Lit + low-corruption favours Omen (needs light, low corruption); dark + corrupted favours
	// Corruption. This is what lets the seeded snapshot actually flip the selection.
	UNightConsequenceCatalog* MakeSnapshotSensitiveCatalog()
	{
		UNightConsequenceCatalog* Catalog = NewObject<UNightConsequenceCatalog>();
		Catalog->FallbackNightType = ENightConsequenceType::Tutorial;
		Catalog->bForceTutorialOnFirstNight = false;

		FNightConsequenceRule LitRule;
		LitRule.NightType = ENightConsequenceType::Omen;
		LitRule.Weight = 4.f;
		LitRule.MinAverageLight = 0.5f;      // only when the sanctuary is well lit
		LitRule.MaxAverageLight = 1.f;
		LitRule.MinAverageCorruption = 0.f;
		LitRule.MaxAverageCorruption = 0.3f; // only when corruption is contained

		FNightConsequenceRule CorruptionRule;
		CorruptionRule.NightType = ENightConsequenceType::Corruption;
		CorruptionRule.Weight = 5.f;
		CorruptionRule.MinAverageLight = 0.f;
		CorruptionRule.MaxAverageLight = 0.4f;     // only when the sanctuary is dark
		CorruptionRule.MinAverageCorruption = 0.4f; // only when corruption has taken hold
		CorruptionRule.MaxAverageCorruption = 1.f;

		Catalog->Rules.Add(LitRule);
		Catalog->Rules.Add(CorruptionRule);
		return Catalog;
	}

	UNightConsequenceManager* MakeManager(UNightConsequenceCatalog* Catalog)
	{
		UNightConsequenceManager* Manager = NewObject<UNightConsequenceManager>();
		Manager->NightCatalog = Catalog;
		Manager->bForceTutorialOnFirstNight = false; // exercise the scoring path, not the shortcut
		return Manager;
	}
}

// The seeded sanctuary snapshot must flow through the REAL snapshot builder into the night brain and
// change the selected night type: a lit/contained sanctuary and a dark/corrupted one pick differently.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFirstNightSnapshotDrivesSelectionTest,
	"Gloamstead.FirstNight.SeededSnapshotDrivesNightSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFirstNightSnapshotDrivesSelectionTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	UNightConsequenceManager* Manager = MakeManager(MakeSnapshotSensitiveCatalog());

	// Lit, contained sanctuary -> the snapshot builder reports high light / low corruption.
	PCG->Test_SeedPointStates(MakeUniformPointStates(/*Count*/ 6, /*Light*/ 0.8f, /*Corruption*/ 0.1f, /*bRestored*/ true));
	const FNightSanctuarySnapshot LitSnapshot = PCG->BuildSanctuarySnapshot();
	TestEqual(TEXT("lit seed -> snapshot average light"), LitSnapshot.AverageLightLevel, 0.8f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("lit seed -> snapshot average corruption"), LitSnapshot.AverageCorruptionLevel, 0.1f, KINDA_SMALL_NUMBER);
	const ENightConsequenceType LitNight = Manager->Test_SelectNightType(LitSnapshot);
	TestTrue(TEXT("lit sanctuary does not summon a Corruption night"), LitNight != ENightConsequenceType::Corruption);
	TestTrue(TEXT("lit sanctuary selects the lit-gated Omen night"), LitNight == ENightConsequenceType::Omen);

	// Dark, corrupted sanctuary -> the snapshot builder reports low light / high corruption.
	PCG->Test_SeedPointStates(MakeUniformPointStates(/*Count*/ 6, /*Light*/ 0.1f, /*Corruption*/ 0.7f, /*bRestored*/ false));
	const FNightSanctuarySnapshot DarkSnapshot = PCG->BuildSanctuarySnapshot();
	TestEqual(TEXT("dark seed -> snapshot average light"), DarkSnapshot.AverageLightLevel, 0.1f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("dark seed -> snapshot average corruption"), DarkSnapshot.AverageCorruptionLevel, 0.7f, KINDA_SMALL_NUMBER);
	const ENightConsequenceType DarkNight = Manager->Test_SelectNightType(DarkSnapshot);
	TestTrue(TEXT("neglected sanctuary summons a Corruption night"), DarkNight == ENightConsequenceType::Corruption);

	// The seeded snapshot genuinely changed the outcome (the integration claim).
	TestTrue(TEXT("seeded snapshot changes the selected night type"), LitNight != DarkNight);
	return true;
}

// The full first-night loop on seeded data: the brain selects a night from the seeded snapshot, the
// director runs its beats for that night (encroachment cue scaled by the seeded light), the loop
// reaches Dawn, and the Heart's dawn reflection consumes and clears the cycle's satisfied warnings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFirstNightSeededLoopToDawnTest,
	"Gloamstead.FirstNight.SeededLoopRunsToDawnReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFirstNightSeededLoopToDawnTest::RunTest(const FString& /*Parameters*/)
{
	// --- Systems under test (worldless; wired via the documented seams) ---
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	UGloamsteadDayNightSubsystem* DayNight = NewObject<UGloamsteadDayNightSubsystem>();
	UNightConsequenceRuntime* Runtime = NewObject<UNightConsequenceRuntime>();
	UNightConsequenceManager* Manager = MakeManager(MakeSnapshotSensitiveCatalog());

	// Seed a neglected sanctuary: dark + corrupted -> the brain should pick Corruption.
	const float SeededLight = 0.12f;
	PCG->Test_SeedPointStates(MakeUniformPointStates(/*Count*/ 5, SeededLight, /*Corruption*/ 0.6f, /*bRestored*/ false));

	const FNightSanctuarySnapshot Snapshot = PCG->BuildSanctuarySnapshot();
	const ENightConsequenceType SelectedNight = Manager->Test_SelectNightType(Snapshot);
	TestTrue(TEXT("seeded neglected sanctuary selects Corruption"), SelectedNight == ENightConsequenceType::Corruption);

	// --- Director driving the presentation beats for the selected night ---
	AGloamsteadFirstNightDirector* Director = NewObject<AGloamsteadFirstNightDirector>();
	Director->Test_BindTo(PCG, DayNight, Runtime);

	// Day intro: warning + lantern target.
	Director->Test_BeginIntro();
	TestTrue(TEXT("loop begins at the Day intro beat"), Director->GetCurrentBeat() == EFirstNightBeat::Intro);
	TestEqual(TEXT("warning presented once"), Director->Test_WarningCount, 1);
	TestEqual(TEXT("lantern target presented once"), Director->Test_LanternTargetCount, 1);

	// Dusk gate holds until the lantern is restored.
	Director->RequestAdvanceToDusk();
	TestTrue(TEXT("dusk gate holds before restoration"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// Restore the lantern -> the director opens the gate by unlocking the first rest; the player's rest
	// at the Heart is what actually advances the phase authority to Dusk.
	FRestorationEventPayload Restore;
	Restore.RitualType = ERitualType::LanternPost;
	Restore.WorldLocation = FVector(300.f, -120.f, 0.f);
	Director->HandleStructureRestored(Restore);
	TestEqual(TEXT("lantern restored beat fired once"), Director->Test_LanternRestoredCount, 1);
	TestTrue(TEXT("lantern restoration opens the dusk gate"), DayNight->CanRestNow());
	TestTrue(TEXT("still Day until the player rests"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
	TestTrue(TEXT("player rests at the Heart"), DayNight->RequestRest());
	TestTrue(TEXT("rest advances to Dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);

	// Dusk cue on the phase change.
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Day, EGloamsteadDayPhase::Dusk);
	TestTrue(TEXT("beat is Dusk"), Director->GetCurrentBeat() == EFirstNightBeat::Dusk);
	TestEqual(TEXT("dusk cue presented once"), Director->Test_DuskCueCount, 1);

	// Dusk -> Night is cadence-owned by DayNight.
	DayNight->AdvanceToNextPhase();
	TestTrue(TEXT("advanced to Night"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night);

	// The night runtime announces the SELECTED night (the brain's snapshot-driven choice); the director
	// begins the encroachment beat, scaled by the seeded sanctuary light.
	Director->HandleNightStarted(SelectedNight);
	TestTrue(TEXT("observed night type matches the seeded selection"), Director->GetObservedNightType() == SelectedNight);
	TestTrue(TEXT("beat is Night"), Director->GetCurrentBeat() == EFirstNightBeat::Night);
	TestEqual(TEXT("encroachment presented once"), Director->Test_EncroachmentCount, 1);
	// ComputeLanternInfluence reads the seeded sanctuary light off the same PCG state.
	TestEqual(TEXT("lantern influence reflects the seeded sanctuary light"),
		PCG->GetSanctuaryAverageLightLevel(), SeededLight, KINDA_SMALL_NUMBER);

	// Night completion advances to Dawn and fires the payoff; the loop is complete.
	DayNight->AdvanceToNextPhase();
	TestTrue(TEXT("advanced to Dawn"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dawn);
	Director->HandlePhaseChanged(EGloamsteadDayPhase::Night, EGloamsteadDayPhase::Dawn);
	TestTrue(TEXT("beat is Complete"), Director->GetCurrentBeat() == EFirstNightBeat::Complete);
	TestEqual(TEXT("dawn payoff presented once"), Director->Test_DawnPayoffCount, 1);
	TestTrue(TEXT("the completed tutorial no longer owns recurring cycles"), Director->IsTutorialDetached());

	// --- Dawn reflection: the Heart consumes and clears the cycle's satisfied warnings ---
	AVeilHeart* Heart = NewObject<AVeilHeart>();
	Heart->WarningCatalog = nullptr; // no catalog -> any non-None tag is recorded directly

	FRestorationEventPayload Understood;
	Understood.WarningTagSatisfied = FName(TEXT("path_remembers_light"));
	Heart->EvaluateRestorationAgainstWarnings(Understood);
	TestEqual(TEXT("the cycle recorded one satisfied warning"), Heart->GetSatisfiedWarningTagCount(), 1);

	Heart->ProcessDawnReflection();
	TestEqual(TEXT("dawn reflection clears the cycle's satisfied warnings"), Heart->GetSatisfiedWarningTagCount(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
