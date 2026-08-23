// Playable Cycle I (Wave 5A) — proof that the day->dusk->night->dawn loop is player-driven and legible.
//
// Three levels of proof:
//  1. Pure feedback formatters (headless) — the on-screen surface is legible and distinguishes outcomes.
//  2. RequestRest phase logic (worldless) — the Heart's rest advances only the resting phases.
//  3. A LIVE game world — the phase handlers, night runtime (BeginNight/EndNight), dawn reflection, the
//     dawn autosave, and the Heart's rest/wake interaction all run through real dynamic-multicast dispatch
//     (which does NOT fire on worldless NewObject'd actors), spawning a real AVeilHeart and exercising the
//     interaction interface. The interior Dusk/Night transitions are pumped as the director's timers would
//     drive them in-game. The player's real autosave slot is rooted, backed up, and restored so the test
//     never clobbers a live save.
#include "Misc/AutomationTest.h"
#include "Systems/GloamsteadCycleFeedbackSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Save/GloamsteadSaveGame.h"
#include "Interfaces/GloamInteractable.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UVeilHeartWarningCatalog* MakeCycleWarningCatalog()
	{
		UVeilHeartWarningCatalog* Catalog = NewObject<UVeilHeartWarningCatalog>();
		FVeilHeartWarningFragment Tutorial;
		Tutorial.WarningId = TEXT("Tutorial");
		Tutorial.AssociatedNightType = ENightConsequenceType::Tutorial;
		Catalog->Warnings.Add(Tutorial);

		FVeilHeartWarningFragment Garden;
		Garden.WarningId = TEXT("GardenRot");
		Garden.AssociatedNightType = ENightConsequenceType::Corruption;
		Catalog->Warnings.Add(Garden);
		return Catalog;
	}
}

// 1. The on-screen feedback layer is legible and distinguishes the three outcomes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamCycleFeedbackFormatTest,
	"Gloamstead.PlayableCycle.FeedbackFormatsAreLegible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamCycleFeedbackFormatTest::RunTest(const FString& /*Parameters*/)
{
	using U = UGloamsteadCycleFeedbackSubsystem;

	// Each phase yields a non-empty, distinct line.
	const FString Day = U::FormatPhase(EGloamsteadDayPhase::Day);
	const FString Dusk = U::FormatPhase(EGloamsteadDayPhase::Dusk);
	const FString Night = U::FormatPhase(EGloamsteadDayPhase::Night);
	const FString Dawn = U::FormatPhase(EGloamsteadDayPhase::Dawn);
	TestTrue(TEXT("day line non-empty"), !Day.IsEmpty());
	TestTrue(TEXT("dusk line non-empty"), !Dusk.IsEmpty());
	TestTrue(TEXT("night line non-empty"), !Night.IsEmpty());
	TestTrue(TEXT("dawn line non-empty"), !Dawn.IsEmpty());
	TestNotEqual(TEXT("day != night line"), Day, Night);

	// Night start names the night type.
	const FString Start = U::FormatNightStart(ENightConsequenceType::Corruption);
	TestTrue(TEXT("night start names the type"), Start.Contains(GetNightConsequenceTypeDisplayName(ENightConsequenceType::Corruption)));

	// The three outcomes read distinctly and carry distinct colours.
	FNightRuntimeOutcome Win;  Win.Result = ENightOutcomeResult::Success; Win.NightType = ENightConsequenceType::Corruption; Win.ResultTag = FName(TEXT("CorruptionCleansed"));
	FNightRuntimeOutcome Mid;  Mid.Result = ENightOutcomeResult::Partial; Mid.NightType = ENightConsequenceType::Corruption; Mid.ResultTag = FName(TEXT("CorruptionLingers"));
	FNightRuntimeOutcome Loss; Loss.Result = ENightOutcomeResult::Failure; Loss.NightType = ENightConsequenceType::Corruption; Loss.ResultTag = FName(TEXT("CorruptionScar"));

	const FString WinText = U::FormatOutcome(Win);
	const FString MidText = U::FormatOutcome(Mid);
	const FString LossText = U::FormatOutcome(Loss);
	TestNotEqual(TEXT("success != partial text"), WinText, MidText);
	TestNotEqual(TEXT("partial != failure text"), MidText, LossText);
	TestTrue(TEXT("success text carries its tag"), WinText.Contains(TEXT("CorruptionCleansed")));

	TestTrue(TEXT("success is green"), U::OutcomeColor(ENightOutcomeResult::Success) == FColor::Green);
	TestTrue(TEXT("partial is yellow"), U::OutcomeColor(ENightOutcomeResult::Partial) == FColor::Yellow);
	TestTrue(TEXT("failure is red"), U::OutcomeColor(ENightOutcomeResult::Failure) == FColor::Red);
	return true;
}

// 2. RequestRest advances only the resting phases (Day->Dusk, Dawn->Day); inert during Dusk/Night.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamDayNightRestAdvanceTest,
	"Gloamstead.PlayableCycle.RestAdvancesRestingPhases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamDayNightRestAdvanceTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadDayNightSubsystem* DayNight = NewObject<UGloamsteadDayNightSubsystem>();

	// The first day belongs to the scripted first-night director: rest must NOT bypass the lantern tutorial.
	TestTrue(TEXT("starts in Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
	TestFalse(TEXT("cannot rest on the first day (tutorial gate)"), DayNight->CanRestNow());
	TestFalse(TEXT("rest is inert on the first day"), DayNight->RequestRest());
	TestTrue(TEXT("still Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);

	// Run the first night as the director/timers would; rest is inert through Dusk and Night.
	DayNight->SetPhase(EGloamsteadDayPhase::Dusk);
	TestFalse(TEXT("cannot rest at Dusk"), DayNight->CanRestNow());
	DayNight->SetPhase(EGloamsteadDayPhase::Night);
	TestFalse(TEXT("cannot rest at Night"), DayNight->CanRestNow());
	DayNight->SetPhase(EGloamsteadDayPhase::Dawn);

	// Dawn is always wake-able (including the FIRST dawn — nothing else advances Dawn->Day in-game).
	TestTrue(TEXT("can wake at Dawn"), DayNight->CanRestNow());
	TestTrue(TEXT("waking advances from Dawn"), DayNight->RequestRest());
	TestTrue(TEXT("Dawn -> Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
	TestEqual(TEXT("a night was counted on the Dawn->Day wrap"), DayNight->GetNightCount(), 1);

	// From day two on (director dormant), rest is the player-driven advance into the next night.
	TestTrue(TEXT("can rest on a later day"), DayNight->CanRestNow());
	TestTrue(TEXT("rest advances from a later Day"), DayNight->RequestRest());
	TestTrue(TEXT("Day -> Dusk on the recurring loop"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
	return true;
}

// 3. LIVE world: the player rests at the Heart and the whole cycle runs through real delegate dispatch.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamPlayableCycleWorldTest,
	"Gloamstead.PlayableCycle.RestToDawnInLiveWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamPlayableCycleWorldTest::RunTest(const FString& /*Parameters*/)
{
	const FString Slot = UGloamsteadPCGSubsystem::DefaultSaveSlot;

	// Protect the player's real autosave: back it up (the dawn autosave below will overwrite this slot).
	// TStrongObjectPtr roots the loaded object so a GC during world teardown can't collect it and leave the
	// restore writing garbage into the real save slot.
	TStrongObjectPtr<USaveGame> Backup(UGameplayStatics::DoesSaveGameExist(Slot, 0)
		? UGameplayStatics::LoadGameFromSlot(Slot, 0) : nullptr);

	// A real game world so dynamic-multicast delegates actually dispatch.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("live world created"), World))
	{
		return false;
	}
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->Init();
	World->SetGameInstance(GameInstance);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.OwningGameInstance = GameInstance;
	WorldContext.SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>();
	UGloamsteadExperienceCycleSubsystem* Experience = GameInstance->GetSubsystem<UGloamsteadExperienceCycleSubsystem>();
	UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	UNightConsequenceManager* Manager = World->GetSubsystem<UNightConsequenceManager>();
	UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>();
	const bool bSubsystems = DayNight && Experience && PCG && Manager && Runtime;
	TestTrue(TEXT("world subsystems present"), bSubsystems);

	if (bSubsystems)
	{
		// Seed a small sanctuary so snapshots/selection have real state.
		TArray<FRitualPointState> States;
		for (int32 i = 0; i < 4; ++i)
		{
			FRitualPointState S; S.LightLevel = 0.4f; S.CorruptionLevel = (i == 0) ? 0.6f : 0.1f; S.bIsRestored = false;
			States.Add(S);
		}
		PCG->Test_SeedPointStates(States);

		// The Heart is the player's rest point, placed in the live world.
		AVeilHeart* Heart = World->SpawnActor<AVeilHeart>();
		TestNotNull(TEXT("Heart spawned in the world"), Heart);

		if (Heart)
		{
			Heart->WarningCatalog = MakeCycleWarningCatalog();
			// Regression guard: the interaction system focuses its target via an object-type overlap
			// (UGloamInteractionComponent::UpdateFocus), so the Heart MUST carry query collision to be
			// reachable. The interface assertions below call Execute_CanInteract/Interact directly, which
			// bypasses that overlap — and once hid a Heart with zero collision, leaving rest/greet-dawn
			// unreachable for a real player (Dawn->Day soft-lock). Prove the Heart is overlap-discoverable.
			{
				TArray<FOverlapResult> Overlaps;
				World->OverlapMultiByObjectType(
					Overlaps, Heart->GetActorLocation(), FQuat::Identity,
					FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
					FCollisionShape::MakeSphere(50.0f));
				bool bHeartDiscoverable = false;
				for (const FOverlapResult& Result : Overlaps)
				{
					if (Result.GetActor() == Heart) { bHeartDiscoverable = true; break; }
				}
				TestTrue(TEXT("the Heart is discoverable by the interaction overlap (has query collision)"), bHeartDiscoverable);
			}

			TestTrue(TEXT("cycle starts in Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			// The first day belongs to the scripted director; the Heart does not offer rest before its lantern gate.
			TestFalse(TEXT("no rest on the first day (tutorial gate)"), IGloamInteractable::Execute_CanInteract(Heart, nullptr));

			// This call is the route the untouched first-night director takes after
			// the lantern is restored. It arms the exact Tutorial plan and its Day warning.
			DayNight->UnlockFirstRest();
			const FExperienceCyclePlan* TutorialPlan = DayNight->GetUpcomingPlan();
			TestNotNull(TEXT("first rest arms a canonical Tutorial plan"), TutorialPlan);
			if (TutorialPlan)
			{
				TestEqual(TEXT("first rest arms the canonical Tutorial plan"), TutorialPlan->PlanId, FName(TEXT("Cycle1_Tutorial")));
			}
			TestTrue(TEXT("the Heart offers rest only after the lantern gate"), IGloamInteractable::Execute_CanInteract(Heart, nullptr));

			// Run the first night through the exact plan path. Dusk must broadcast Tutorial without score selection.
			IGloamInteractable::Execute_Interact(Heart, nullptr);
			TestTrue(TEXT("advanced to Dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
			TestEqual(TEXT("manager received the exact Tutorial plan type"), Manager->GetLastSelectedNightType(), ENightConsequenceType::Tutorial);
			TestEqual(TEXT("runtime received the manager's exact delegate type"), Runtime->GetPlannedNightType(), ENightConsequenceType::Tutorial);
			TestFalse(TEXT("the Heart is inert at Dusk"), IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			DayNight->AdvanceToNextPhase(); // Dusk -> Night (BeginNight)
			TestTrue(TEXT("advanced to Night"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night);
			DayNight->AdvanceToNextPhase(); // Night -> Dawn (EndNight + reflection + autosave)
			TestTrue(TEXT("advanced to Dawn"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dawn);

			// The night resolved to a real outcome, and dawn reflection consumed it on the Heart.
			const FNightRuntimeOutcome Outcome = Runtime->GetLastOutcome();
			TestTrue(TEXT("the night produced an outcome"), Outcome.Result != ENightOutcomeResult::None);
			TestTrue(TEXT("the Heart recorded the dawn outcome"), Heart->GetLastNightOutcome().Result == Outcome.Result);

			// The dawn autosave wrote the single sanctuary progression slot.
			TestTrue(TEXT("dawn autosave wrote the slot"), UGameplayStatics::DoesSaveGameExist(Slot, 0));

			// The player wakes at the Heart (first dawn) -> Day, and the night is counted — all via the interface.
			TestTrue(TEXT("the Heart offers rest at Dawn"), IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			IGloamInteractable::Execute_Interact(Heart, nullptr);
			TestTrue(TEXT("waking advanced to Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestTrue(TEXT("a night has passed"), DayNight->GetNightCount() >= 1);
			const FExperienceCyclePlan* CycleTwoBeforeSave = DayNight->GetUpcomingPlan();
			TestNotNull(TEXT("the new Day arms Cycle II before rest"), CycleTwoBeforeSave);
			if (CycleTwoBeforeSave)
			{
				TestEqual(TEXT("the exact Cycle II id is armed"), CycleTwoBeforeSave->PlanId, FName(TEXT("Cycle2_Garden")));
				TestEqual(TEXT("the exact Cycle II warning is armed"), CycleTwoBeforeSave->WarningId, FName(TEXT("GardenRot")));
			}

			TestTrue(TEXT("full progression save retains the armed Cycle II plan"), DayNight->SaveProgressionToSlot());
			FExperienceCyclePersistentState EmptyCycle;
			TestTrue(TEXT("test can clear the in-memory cycle before reload"), Experience->RestorePersistentState(EmptyCycle));
			TestTrue(TEXT("full progression load restores Cycle II rather than replaying Tutorial"), DayNight->LoadProgressionFromSlot());
			const FExperienceCyclePlan* CycleTwoAfterLoad = DayNight->GetUpcomingPlan();
			TestNotNull(TEXT("loaded Cycle II plan is available before rest"), CycleTwoAfterLoad);
			if (CycleTwoAfterLoad)
			{
				TestEqual(TEXT("loaded plan keeps Cycle II id"), CycleTwoAfterLoad->PlanId, FName(TEXT("Cycle2_Garden")));
				TestEqual(TEXT("loaded plan keeps GardenRot warning"), CycleTwoAfterLoad->WarningId, FName(TEXT("GardenRot")));
			}

			// From day two, rest preserves the armed ID and broadcasts only the exact Corruption type.
			TestTrue(TEXT("the Heart offers rest on the recurring day"), IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			IGloamInteractable::Execute_Interact(Heart, nullptr);
			TestTrue(TEXT("resting advanced to Dusk on the recurring loop"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
			const FExperienceCyclePlan* CycleTwoAtDusk = DayNight->GetUpcomingPlan();
			TestNotNull(TEXT("rest retains an authored Cycle II plan at Dusk"), CycleTwoAtDusk);
			if (CycleTwoAtDusk)
			{
				TestEqual(TEXT("rest did not mutate the Cycle II id"), CycleTwoAtDusk->PlanId, FName(TEXT("Cycle2_Garden")));
			}
			TestEqual(TEXT("Dusk did not choose a generic catalog type"), Manager->GetLastSelectedNightType(), ENightConsequenceType::Corruption);
			TestEqual(TEXT("the runtime received exact Corruption through the existing delegate"), Runtime->GetPlannedNightType(), ENightConsequenceType::Corruption);

			UGloamsteadSaveGame* LegacySave = Cast<UGloamsteadSaveGame>(UGameplayStatics::CreateSaveGameObject(UGloamsteadSaveGame::StaticClass()));
			PCG->CaptureToSaveGame(LegacySave);
			LegacySave->SaveVersion = 1;
			FExperienceCyclePersistentState LegacyState;
			LegacyState.CompletedCycleSlot = 1;
			LegacyState.ArmedPlanId = TEXT("Cycle2_Garden");
			LegacyState.bFirstRestCompleted = true;
			LegacyState.SavedPhaseOrdinal = static_cast<int32>(EGloamsteadDayPhase::Day);
			LegacySave->SetExperienceCycleState(LegacyState);
			TestTrue(TEXT("legacy fixture writes to the default slot"), UGameplayStatics::SaveGameToSlot(LegacySave, Slot, 0));
			TestFalse(TEXT("legacy load is explicitly rejected for authored progression"), DayNight->LoadProgressionFromSlot());
			TestTrue(TEXT("legacy load remains invalid rather than replaying Cycle II"), Experience->GetActivePlan().IsInvalid());
		}
	}

	// Teardown the world.
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	GameInstance->Shutdown();

	// Restore the player's real save slot exactly as we found it.
	if (Backup)
	{
		UGameplayStatics::SaveGameToSlot(Backup.Get(), Slot, 0);
	}
	else if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
