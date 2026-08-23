// Playable Cycle I (Wave 5A) — proof that the day->dusk->night->dawn loop is player-driven and legible.
//
// Three levels of proof:
//  1. Pure feedback formatters (headless) — the on-screen surface is legible and distinguishes outcomes.
//  2. RequestRest phase logic (worldless) — the Heart's rest advances only the resting phases.
//  3. A LIVE game world — the phase handlers, night runtime (BeginNight/EndNight), dawn reflection, the
//     and the Heart's rest/wake interaction all run through real dynamic-multicast dispatch (which does NOT
//     fire on worldless NewObject'd actors), spawning a real AVeilHeart and exercising the interaction interface.
//     The interior Dusk/Night transitions are pumped as DayNight cadence would drive them in-game. The test
//     uses its own explicit slot and disables dawn autosave so it never touches a player's real save.
#include "Misc/AutomationTest.h"
#include "Systems/GloamsteadCycleFeedbackSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "Presentation/GloamsteadSkyPresenter.h"
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
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UVeilHeartWarningCatalog* MakeCycleWarningCatalog()
	{
		UVeilHeartWarningCatalog* Catalog = NewObject<UVeilHeartWarningCatalog>();
		FVeilHeartWarningFragment Tutorial;
		// Asset-faithful fixture: the shipped warning catalog's Tutorial fragment
		// is TutorialLostPath, not a synthetic Tutorial row.
		Tutorial.WarningId = TEXT("TutorialLostPath");
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
	const FString Slot = TEXT("GloamsteadPlayableCycleRemediation");
	const FString LanternGateSlot = TEXT("GloamsteadLanternGateReloadRemediation");
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(LanternGateSlot, 0);

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
		DayNight->SetDawnAutosaveEnabled(false);
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
			// The sky owns global phase presentation and becomes the generic warning
			// presenter only after the first-night director has detached at Dawn.
			AGloamsteadSkyPresenter* SkyPresenter = World->SpawnActor<AGloamsteadSkyPresenter>();
			TestNotNull(TEXT("SkyPresenter spawned for the post-tutorial handoff"), SkyPresenter);
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

			// A pre-lantern bootstrap may arm the tutorial plan, but its exact warning
			// must remain silent. The armed ID is not proof that the lantern gate ran.
			TestFalse(TEXT("a Heart without a registered warning presenter is not presentation-ready"), Heart->HasValidWarningPresenter());
			TestFalse(TEXT("pre-lantern preparation keeps the Tutorial warning closed"), DayNight->PrepareUpcomingCycle());
			const FExperienceCyclePlan* TutorialPlan = DayNight->GetUpcomingPlan();
			TestNotNull(TEXT("first rest arms a canonical Tutorial plan"), TutorialPlan);
			if (TutorialPlan)
			{
				TestEqual(TEXT("first rest arms the canonical Tutorial plan"), TutorialPlan->PlanId, FName(TEXT("Cycle1_Tutorial")));
				TestEqual(TEXT("first rest arms the shipped TutorialLostPath warning"), TutorialPlan->WarningId, FName(TEXT("TutorialLostPath")));
			}
			TestEqual(TEXT("pre-lantern preparation emits no Tutorial warning"), Heart->GetLastEmittedWarningId(), NAME_None);
			TestTrue(TEXT("pre-lantern Day snapshot saves to its isolated slot"), DayNight->SaveProgressionToSlot(LanternGateSlot));

			// A ready production presenter before reload must not turn the saved armed
			// plan into proof that the lantern tutorial completed.
			AGloamsteadFirstNightDirector* FirstNightDirector = World->SpawnActor<AGloamsteadFirstNightDirector>();
			TestNotNull(TEXT("the existing first-night director binds before reload"), FirstNightDirector);
			TestTrue(TEXT("the first-night director binds as the registered warning presenter"), Heart->HasValidWarningPresenter());
			FExperienceCyclePersistentState ClearedCycle;
			TestTrue(TEXT("test clears the pre-lantern in-memory plan before isolated reload"), Experience->RestorePersistentState(ClearedCycle));
			TestTrue(TEXT("v2 pre-lantern Day snapshot reloads from its isolated slot"), DayNight->LoadProgressionFromSlot(LanternGateSlot));
			TestFalse(TEXT("pre-lantern Cycle I reload keeps the live tutorial director attached"),
				FirstNightDirector && FirstNightDirector->IsTutorialDetached());
			TestTrue(TEXT("pre-lantern Cycle I reload retains the tutorial Heart presenter"), Heart->HasValidWarningPresenter());
			TestFalse(TEXT("saved Tutorial plan cannot infer the lantern gate"), DayNight->IsFirstRestUnlocked());
			TestEqual(TEXT("reload stays in Day before the lantern tutorial event"), DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Day);
			const FExperienceCyclePlan* ReloadedTutorialPlan = DayNight->GetUpcomingPlan();
			TestNotNull(TEXT("isolated reload retains the persisted Cycle I Tutorial plan before unlock"), ReloadedTutorialPlan);
			if (ReloadedTutorialPlan)
			{
				TestEqual(TEXT("isolated reload retains Cycle1_Tutorial before unlock"), ReloadedTutorialPlan->PlanId, FName(TEXT("Cycle1_Tutorial")));
				TestEqual(TEXT("isolated reload retains TutorialLostPath before unlock"), ReloadedTutorialPlan->WarningId, FName(TEXT("TutorialLostPath")));
			}
			TestEqual(TEXT("reload still emits no Tutorial warning before the lantern tutorial event"), Heart->GetLastEmittedWarningId(), NAME_None);
			TestFalse(TEXT("RequestRest remains closed after pre-lantern reload"), DayNight->RequestRest());
			TestEqual(TEXT("rejected pre-lantern rest leaves the phase in Day"), DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Day);

			FRestorationEventPayload LanternRestore;
			LanternRestore.RitualType = ERitualType::LanternPost;
			LanternRestore.WorldLocation = FVector(125.0f, 0.0f, 0.0f);
			if (FirstNightDirector)
			{
				FirstNightDirector->HandleStructureRestored(LanternRestore);
			}
			TestTrue(TEXT("only the explicit lantern event opens the first-rest gate"), DayNight->IsFirstRestUnlocked());
			TestTrue(TEXT("the director records the explicit lantern lesson"),
				FirstNightDirector && FirstNightDirector->IsLanternRestored());
			TestEqual(TEXT("explicit lantern event presents the retained Tutorial warning"), Heart->GetLastEmittedWarningId(), FName(TEXT("TutorialLostPath")));
			TestTrue(TEXT("the Heart offers rest after the explicit lantern event"), IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			TestTrue(TEXT("RequestRest takes the normal guarded first-rest route"), DayNight->RequestRest());
			TestTrue(TEXT("guarded first rest advances to Dusk"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
			TestTrue(TEXT("DayNight schedules the first Dusk-to-Night cadence"), DayNight->Test_IsDuskToNightCadenceScheduled());

			// Run the first night through the exact plan path. Dusk must broadcast Tutorial without score selection.
			TestEqual(TEXT("manager received the exact Tutorial plan type"), Manager->GetLastSelectedNightType(), ENightConsequenceType::Tutorial);
			TestEqual(TEXT("runtime received the manager's exact delegate type"), Runtime->GetPlannedNightType(), ENightConsequenceType::Tutorial);
			TestFalse(TEXT("the Heart is inert at Dusk"), IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			DayNight->AdvanceToNextPhase(); // Dusk -> Night (BeginNight)
			TestTrue(TEXT("advanced to Night"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night);
			TestFalse(TEXT("entering Night clears the completed Dusk cadence"), DayNight->Test_IsDuskToNightCadenceScheduled());
			TestTrue(TEXT("DayNight schedules the Night-to-Dawn cadence"), DayNight->Test_IsNightToDawnCadenceScheduled());
			DayNight->AdvanceToNextPhase(); // Night -> Dawn (EndNight + reflection + autosave)
			TestTrue(TEXT("advanced to Dawn"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dawn);
			TestFalse(TEXT("entering Dawn clears the Night cadence"), DayNight->Test_IsNightToDawnCadenceScheduled());
			TestTrue(TEXT("Cycle I dawn permanently detaches the tutorial director"),
				FirstNightDirector && FirstNightDirector->IsTutorialDetached());
			World->Tick(LEVELTICK_All, 0.05f);
			TestTrue(TEXT("SkyPresenter takes the registered warning role after tutorial teardown"), Heart->HasValidWarningPresenter());

			// The night resolved to a real outcome, and dawn reflection consumed it on the Heart.
			const FNightRuntimeOutcome Outcome = Runtime->GetLastOutcome();
			TestTrue(TEXT("the night produced an outcome"), Outcome.Result != ENightOutcomeResult::None);
			TestTrue(TEXT("the Heart recorded the dawn outcome"), Heart->GetLastNightOutcome().Result == Outcome.Result);

			// The player wakes at the Heart (first dawn) -> Day, and the night is counted — all via the interface.
			TestTrue(TEXT("the Heart offers rest at Dawn"), IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			IGloamInteractable::Execute_Interact(Heart, nullptr);
			TestTrue(TEXT("waking advanced to Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestTrue(TEXT("a night has passed"), DayNight->GetNightCount() >= 1);
			if (SkyPresenter)
			{
				TestEqual(TEXT("generic post-tutorial presenter receives Cycle II's exact warning"),
					SkyPresenter->Test_GetLastPresentedWarningId(), FName(TEXT("GardenRot")));
			}
			const FExperienceCyclePlan* CycleTwoBeforeSave = DayNight->GetUpcomingPlan();
			TestNotNull(TEXT("the new Day arms Cycle II before rest"), CycleTwoBeforeSave);
			if (CycleTwoBeforeSave)
			{
				TestEqual(TEXT("the exact Cycle II id is armed"), CycleTwoBeforeSave->PlanId, FName(TEXT("Cycle2_Garden")));
				TestEqual(TEXT("the exact Cycle II warning is armed"), CycleTwoBeforeSave->WarningId, FName(TEXT("GardenRot")));
			}

			// Save a non-default sanctuary snapshot in Day together with the exact Cycle II plan.
			TArray<FRitualPointState> SavedPCGStates = PCG->Test_PeekPointStates();
			SavedPCGStates[0].LightLevel = 0.93f;
			SavedPCGStates[0].CorruptionLevel = 0.07f;
			SavedPCGStates[0].bIsRestored = true;
			PCG->Test_SeedPointStates(SavedPCGStates);
			TestTrue(TEXT("full progression save retains non-default PCG and armed Cycle II plan"), DayNight->SaveProgressionToSlot(Slot));

			// A duplicate is not a canonical Heart election. The otherwise ready
			// presenter must not cause DayNight to pick either actor arbitrarily.
			AVeilHeart* DuplicateHeart = World->SpawnActor<AVeilHeart>();
			TestNotNull(TEXT("a duplicate Heart exists to prove ambiguity stays closed"), DuplicateHeart);
			if (DuplicateHeart)
			{
				DuplicateHeart->WarningCatalog = MakeCycleWarningCatalog();
			}
			DayNight->SetPhase(EGloamsteadDayPhase::Dusk);
			TestFalse(TEXT("an in-progress Dusk cannot overwrite the prior safe Day progression snapshot"), DayNight->SaveProgressionToSlot(Slot));
			FExperienceCyclePersistentState EmptyCycle;
			TestTrue(TEXT("test can clear the in-memory cycle before reload"), Experience->RestorePersistentState(EmptyCycle));
			TArray<FRitualPointState> MutatedPCGStates = SavedPCGStates;
			MutatedPCGStates[0].LightLevel = 0.11f;
			MutatedPCGStates[0].CorruptionLevel = 0.88f;
			MutatedPCGStates[0].bIsRestored = false;
			PCG->Test_SeedPointStates(MutatedPCGStates);
			TestTrue(TEXT("valid v2 load succeeds while warning presentation is pending"), DayNight->LoadProgressionFromSlot(Slot));
			TestTrue(TEXT("load restores the saved Day phase"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestTrue(TEXT("load retains a pending exact warning while multiple Hearts are ambiguous"), DayNight->IsWarningPresentationPending());
			TestFalse(TEXT("rest stays gated while multiple Hearts are ambiguous"), DayNight->CanRestNow());
			DayNight->AdvanceToNextPhase();
			TestTrue(TEXT("direct Day advance stays blocked while presentation is pending"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			const TArray<FRitualPointState>& RestoredPCGStates = PCG->Test_PeekPointStates();
			TestEqual(TEXT("load restores the saved PCG point count"), RestoredPCGStates.Num(), SavedPCGStates.Num());
			if (RestoredPCGStates.IsValidIndex(0))
			{
				TestTrue(TEXT("load restores saved PCG light"), FMath::IsNearlyEqual(RestoredPCGStates[0].LightLevel, 0.93f));
				TestTrue(TEXT("load restores saved PCG corruption"), FMath::IsNearlyEqual(RestoredPCGStates[0].CorruptionLevel, 0.07f));
				TestTrue(TEXT("load restores saved PCG restoration flag"), RestoredPCGStates[0].bIsRestored);
			}
			const FExperienceCyclePlan* CycleTwoAfterLoad = DayNight->GetUpcomingPlan();
			TestNotNull(TEXT("loaded Cycle II plan is available before rest"), CycleTwoAfterLoad);
			if (CycleTwoAfterLoad)
			{
				TestEqual(TEXT("loaded plan keeps Cycle II id"), CycleTwoAfterLoad->PlanId, FName(TEXT("Cycle2_Garden")));
				TestEqual(TEXT("loaded plan keeps GardenRot warning"), CycleTwoAfterLoad->WarningId, FName(TEXT("GardenRot")));
			}

			if (DuplicateHeart)
			{
				DuplicateHeart->Destroy();
			}
			World->Tick(LEVELTICK_All, 0.3f);
			TestFalse(TEXT("removing the duplicate lets the single canonical Heart present"), DayNight->IsWarningPresentationPending());
			TestTrue(TEXT("the single canonical Heart re-opens rest after exact presentation"), DayNight->CanRestNow());

			// Bootstrap may instead load while the one canonical Heart exists but the
			// generic post-tutorial presenter has not attached yet. That valid payload
			// must stay pending until the same explicit production presenter attaches.
			if (SkyPresenter)
			{
				Heart->OnWarningEmittedDelegate.RemoveDynamic(SkyPresenter, &AGloamsteadSkyPresenter::HandleHeartWarning);
				Heart->UnregisterWarningPresenter(SkyPresenter);
			}
			TestFalse(TEXT("test detaches the registered presenter before a singular resumed load"), Heart->HasValidWarningPresenter());
			DayNight->SetPhase(EGloamsteadDayPhase::Dusk);
			TestTrue(TEXT("valid v2 load succeeds with one Heart but no presenter"), DayNight->LoadProgressionFromSlot(Slot));
			TestTrue(TEXT("one canonical Heart without a presenter remains pending"), DayNight->IsWarningPresentationPending());
			TestFalse(TEXT("one canonical Heart without a presenter still cannot offer rest"), DayNight->CanRestNow());
			DayNight->AdvanceToNextPhase();
			TestTrue(TEXT("direct Day advance stays blocked while the singular presenter is late"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			if (SkyPresenter)
			{
				Heart->OnWarningEmittedDelegate.AddDynamic(SkyPresenter, &AGloamsteadSkyPresenter::HandleHeartWarning);
				TestTrue(TEXT("the rebound warning delegate re-registers as the player-facing presenter"),
					Heart->RegisterWarningPresenter(SkyPresenter, GET_FUNCTION_NAME_CHECKED(AGloamsteadSkyPresenter, HandleHeartWarning)));
			}
			TestTrue(TEXT("the generic post-tutorial presenter can rebind"), Heart->HasValidWarningPresenter());
			World->Tick(LEVELTICK_All, 0.3f);
			TestFalse(TEXT("listener-late Cycle II presentation clears pending state"), DayNight->IsWarningPresentationPending());
			TestEqual(TEXT("the canonical Heart emits the retained exact GardenRot warning"), Heart->GetLastEmittedWarningId(), FName(TEXT("GardenRot")));
			TestTrue(TEXT("rest becomes eligible only after the exact pending warning presents"), DayNight->CanRestNow());

			// From day two, rest preserves the armed ID and broadcasts only the exact Corruption type.
			TestTrue(TEXT("the Heart offers rest on the recurring day"), IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			IGloamInteractable::Execute_Interact(Heart, nullptr);
			TestTrue(TEXT("resting advanced to Dusk on the recurring loop"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
			TestTrue(TEXT("Cycle II Dusk cadence belongs to DayNight after tutorial teardown"), DayNight->Test_IsDuskToNightCadenceScheduled());
			TestTrue(TEXT("Cycle II still cannot revive the detached tutorial actor"),
				FirstNightDirector && FirstNightDirector->IsTutorialDetached());
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
			TestTrue(TEXT("legacy fixture writes to the isolated slot"), UGameplayStatics::SaveGameToSlot(LegacySave, Slot, 0));
			TestTrue(TEXT("legacy restore is attempted from an in-progress Dusk phase"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
			TestFalse(TEXT("legacy load is explicitly rejected for authored progression"), DayNight->LoadProgressionFromSlot(Slot));
			TestTrue(TEXT("legacy load remains invalid rather than replaying Cycle II"), Experience->GetActivePlan().IsInvalid());
			TestTrue(TEXT("legacy rejection atomically reconciles phase to Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestEqual(TEXT("legacy rejection clears the prior night counter"), DayNight->GetNightCount(), 0);
			TestFalse(TEXT("legacy rejection closes the first-rest eligibility gate"), DayNight->IsFirstRestUnlocked());
			TestFalse(TEXT("legacy rejection clears pending warning presentation"), DayNight->IsWarningPresentationPending());
			TestFalse(TEXT("legacy rejection cannot offer rest without a new safe plan"), DayNight->CanRestNow());

			UGloamsteadSaveGame* MalformedSave = Cast<UGloamsteadSaveGame>(UGameplayStatics::CreateSaveGameObject(UGloamsteadSaveGame::StaticClass()));
			PCG->CaptureToSaveGame(MalformedSave);
			FExperienceCyclePersistentState MalformedState;
			MalformedState.CompletedCycleSlot = 1;
			MalformedState.ArmedPlanId = TEXT("Cycle2_Garden");
			MalformedState.bFirstRestCompleted = true;
			MalformedState.SavedPhaseOrdinal = 99;
			MalformedSave->SetExperienceCycleState(MalformedState);
			TestTrue(TEXT("malformed v2 fixture writes to the isolated slot"), UGameplayStatics::SaveGameToSlot(MalformedSave, Slot, 0));
			DayNight->SetPhase(EGloamsteadDayPhase::Night);
			TestTrue(TEXT("malformed phase restore is attempted from an in-progress Night"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night);
			TestFalse(TEXT("malformed saved phase is rejected"), DayNight->LoadProgressionFromSlot(Slot));
			TestTrue(TEXT("malformed phase rejection atomically reconciles to Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestEqual(TEXT("malformed phase rejection clears the night counter"), DayNight->GetNightCount(), 0);
			TestFalse(TEXT("malformed phase rejection clears pending warning presentation"), DayNight->IsWarningPresentationPending());
			TestFalse(TEXT("malformed phase rejection closes rest eligibility"), DayNight->CanRestNow());
			TestTrue(TEXT("malformed phase rejection clears the cycle plan"), Experience->GetActivePlan().IsInvalid());

			UGloamsteadSaveGame* UnsupportedSave = Cast<UGloamsteadSaveGame>(UGameplayStatics::CreateSaveGameObject(UGloamsteadSaveGame::StaticClass()));
			PCG->CaptureToSaveGame(UnsupportedSave);
			UnsupportedSave->SaveVersion = UGloamsteadSaveGame::CurrentSaveVersion + 1;
			TestTrue(TEXT("unsupported v3 fixture writes to the isolated slot"), UGameplayStatics::SaveGameToSlot(UnsupportedSave, Slot, 0));
			DayNight->SetPhase(EGloamsteadDayPhase::Dusk);
			TestFalse(TEXT("unsupported progression payload is rejected"), DayNight->LoadProgressionFromSlot(Slot));
			TestTrue(TEXT("unsupported payload rejection atomically reconciles to Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestEqual(TEXT("unsupported payload rejection clears the night counter"), DayNight->GetNightCount(), 0);
			TestFalse(TEXT("unsupported payload rejection clears pending warning presentation"), DayNight->IsWarningPresentationPending());
			TestFalse(TEXT("unsupported payload rejection closes rest eligibility"), DayNight->CanRestNow());

			// Runtime-resume state is not persisted. Injected legacy-v2 in-progress
			// snapshots keep their PCG aftermath but must clear the armed plan rather
			// than re-present/replay the pressure from a fabricated Day.
			TArray<FRitualPointState> InjectedPressureStates = PCG->Test_PeekPointStates();
			if (InjectedPressureStates.IsValidIndex(0))
			{
				InjectedPressureStates[0].LightLevel = 0.16f;
				InjectedPressureStates[0].CorruptionLevel = 0.84f;
				InjectedPressureStates[0].bIsRestored = false;
				PCG->Test_SeedPointStates(InjectedPressureStates);
			}
			UGloamsteadSaveGame* DuskSave = Cast<UGloamsteadSaveGame>(UGameplayStatics::CreateSaveGameObject(UGloamsteadSaveGame::StaticClass()));
			PCG->CaptureToSaveGame(DuskSave);
			FExperienceCyclePersistentState DuskState;
			DuskState.CompletedCycleSlot = 1;
			DuskState.ArmedPlanId = TEXT("Cycle2_Garden");
			DuskState.bFirstRestCompleted = true;
			DuskState.SavedPhaseOrdinal = static_cast<int32>(EGloamsteadDayPhase::Dusk);
			DuskSave->SetExperienceCycleState(DuskState);
			TestTrue(TEXT("Dusk fixture writes to the isolated slot"), UGameplayStatics::SaveGameToSlot(DuskSave, Slot, 0));
			TestFalse(TEXT("test clears the selected runtime consequence before injected Dusk reconciliation"),
				Manager->PrepareNightConsequencesForPlan(FExperienceCyclePlan::MakeInvalid(2)));
			const FName WarningBeforeDuskLoad = Heart->GetLastEmittedWarningId();
			DayNight->SetPhase(EGloamsteadDayPhase::Night);
			TestTrue(TEXT("injected v2 Dusk restores PCG then reconciles to Day without replay"), DayNight->LoadProgressionFromSlot(Slot));
			TestTrue(TEXT("saved Dusk resumes at safe Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestFalse(TEXT("reconciled Dusk leaves no pending re-presentation"), DayNight->IsWarningPresentationPending());
			TestTrue(TEXT("reconciled Dusk clears the active authored plan"), Experience->GetActivePlan().IsInvalid());
			TestEqual(TEXT("reconciled Dusk clears the persisted armed plan"), Experience->CapturePersistentState().ArmedPlanId, NAME_None);
			TestFalse(TEXT("reconciled Dusk closes rest instead of replaying the consequence"), DayNight->CanRestNow());
			DayNight->AdvanceToNextPhase();
			TestTrue(TEXT("reconciled Dusk cannot re-arm or advance into a second consequence"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestFalse(TEXT("reconciled Dusk cannot persist a fresh Day wrapper around prior pressure"), DayNight->SaveProgressionToSlot(Slot));
			TestEqual(TEXT("reconciled Dusk does not select a new consequence"), Manager->GetLastSelectedNightType(), ENightConsequenceType::Invalid);
			TestEqual(TEXT("reconciled Dusk does not re-emit the warning"), Heart->GetLastEmittedWarningId(), WarningBeforeDuskLoad);
			const TArray<FRitualPointState>& DuskRestoredStates = PCG->Test_PeekPointStates();
			if (DuskRestoredStates.IsValidIndex(0))
			{
				TestTrue(TEXT("reconciled Dusk preserves its already-applied PCG pressure"), FMath::IsNearlyEqual(DuskRestoredStates[0].CorruptionLevel, 0.84f));
			}

			UGloamsteadSaveGame* NightSave = Cast<UGloamsteadSaveGame>(UGameplayStatics::CreateSaveGameObject(UGloamsteadSaveGame::StaticClass()));
			PCG->CaptureToSaveGame(NightSave);
			FExperienceCyclePersistentState NightState = DuskState;
			NightState.SavedPhaseOrdinal = static_cast<int32>(EGloamsteadDayPhase::Night);
			NightSave->SetExperienceCycleState(NightState);
			TestTrue(TEXT("Night fixture writes to the isolated slot"), UGameplayStatics::SaveGameToSlot(NightSave, Slot, 0));
			TestFalse(TEXT("test keeps the runtime consequence cleared before injected Night reconciliation"),
				Manager->PrepareNightConsequencesForPlan(FExperienceCyclePlan::MakeInvalid(2)));
			const FName WarningBeforeNightLoad = Heart->GetLastEmittedWarningId();
			DayNight->SetPhase(EGloamsteadDayPhase::Dusk);
			TestTrue(TEXT("injected v2 Night restores PCG then reconciles to Day without replay"), DayNight->LoadProgressionFromSlot(Slot));
			TestTrue(TEXT("saved Night resumes at safe Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestFalse(TEXT("reconciled Night leaves no pending re-presentation"), DayNight->IsWarningPresentationPending());
			TestTrue(TEXT("reconciled Night clears the active authored plan"), Experience->GetActivePlan().IsInvalid());
			TestEqual(TEXT("reconciled Night clears the persisted armed plan"), Experience->CapturePersistentState().ArmedPlanId, NAME_None);
			TestFalse(TEXT("reconciled Night closes rest instead of replaying the consequence"), DayNight->CanRestNow());
			DayNight->AdvanceToNextPhase();
			TestTrue(TEXT("reconciled Night cannot re-arm or advance into a second consequence"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
			TestEqual(TEXT("reconciled Night does not select a new consequence"), Manager->GetLastSelectedNightType(), ENightConsequenceType::Invalid);
			TestEqual(TEXT("reconciled Night does not re-emit the warning"), Heart->GetLastEmittedWarningId(), WarningBeforeNightLoad);
		}
	}

	// Teardown the world.
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	GameInstance->Shutdown();

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(LanternGateSlot, 0);
	return true;
}

// A valid later-cycle Day save replaces the opening lesson's world state. The
// live regression proves that the tutorial presenter releases before GardenRot
// can become rest-eligible, and that an active old night is aborted before its
// pressure cadence can touch the restored PCG baseline.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamPlayableCycleResumeQuiescenceWorldTest,
	"Gloamstead.PlayableCycle.ResumeQuiescesTutorialAndRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamPlayableCycleResumeQuiescenceWorldTest::RunTest(const FString& /*Parameters*/)
{
	const FString SafeDaySlot = TEXT("GloamsteadPlayableCycleResumeQuiescence");
	UGameplayStatics::DeleteGameInSlot(SafeDaySlot, 0);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("resume-quiescence live world created"), World))
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
	UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>();
	const bool bSubsystems = DayNight && Experience && PCG && Runtime;
	TestTrue(TEXT("resume-quiescence subsystems present"), bSubsystems);

	if (bSubsystems)
	{
		DayNight->SetDawnAutosaveEnabled(false);
		DayNight->NightDurationSeconds = 60.0f;
		Runtime->PressureStepSeconds = 0.05f;

		TArray<FRitualPointState> SafeDayStates;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FRitualPointState State;
			State.LightLevel = 0.35f + static_cast<float>(Index) * 0.1f;
			State.CorruptionLevel = (Index == 0) ? 0.65f : 0.15f;
			State.bIsRestored = false;
			SafeDayStates.Add(State);
		}
		PCG->Test_SeedPointStates(SafeDayStates);

		AVeilHeart* Heart = World->SpawnActor<AVeilHeart>();
		AGloamsteadSkyPresenter* SkyPresenter = World->SpawnActor<AGloamsteadSkyPresenter>();
		TestNotNull(TEXT("resume-quiescence Heart spawned"), Heart);
		TestNotNull(TEXT("resume-quiescence SkyPresenter spawned"), SkyPresenter);
		if (Heart)
		{
			Heart->WarningCatalog = MakeCycleWarningCatalog();
		}

		AGloamsteadFirstNightDirector* FirstNightDirector = World->SpawnActor<AGloamsteadFirstNightDirector>();
		TestNotNull(TEXT("resume-quiescence director begins active"), FirstNightDirector);
		TestTrue(TEXT("opening director owns the tutorial presenter before the later save"), Heart && Heart->HasValidWarningPresenter());
		TestFalse(TEXT("opening director is active before the later save"),
			FirstNightDirector && FirstNightDirector->IsTutorialDetached());

		UGloamsteadSaveGame* SafeDaySave = Cast<UGloamsteadSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UGloamsteadSaveGame::StaticClass()));
		TestNotNull(TEXT("safe later-cycle Day save allocated"), SafeDaySave);
		if (SafeDaySave)
		{
			PCG->CaptureToSaveGame(SafeDaySave);
			FExperienceCyclePersistentState SafeCycleState;
			SafeCycleState.CompletedCycleSlot = 1;
			SafeCycleState.ArmedPlanId = FName(TEXT("Cycle2_Garden"));
			SafeCycleState.LastPlanId = FName(TEXT("Cycle1_Tutorial"));
			SafeCycleState.LastOutcomeResultTag = FName(TEXT("TutorialSheltered"));
			SafeCycleState.bFirstRestCompleted = true;
			SafeCycleState.SavedPhaseOrdinal = static_cast<int32>(EGloamsteadDayPhase::Day);
			SafeDaySave->SetExperienceCycleState(SafeCycleState);
			TestTrue(TEXT("safe authored Cycle II Day fixture writes"), UGameplayStatics::SaveGameToSlot(SafeDaySave, SafeDaySlot, 0));
		}

		TestTrue(TEXT("later-cycle Day load succeeds while Cycle I director is active"), DayNight->LoadProgressionFromSlot(SafeDaySlot));
		TestTrue(TEXT("later-cycle Day load detaches the stale tutorial director before warning retry"),
			FirstNightDirector && FirstNightDirector->IsTutorialDetached());
		TestFalse(TEXT("detached tutorial director releases the Heart presenter slot"), Heart && Heart->HasValidWarningPresenter());
		TestFalse(TEXT("detached tutorial director no longer receives Heart warning callbacks"),
			Heart && FirstNightDirector && Heart->OnWarningEmittedDelegate.IsAlreadyBound(FirstNightDirector, &AGloamsteadFirstNightDirector::HandleHeartWarning));
		TestTrue(TEXT("Cycle II warning stays pending until the generic presenter attaches"), DayNight->IsWarningPresentationPending());
		TestFalse(TEXT("rest stays closed while the former tutorial presenter is the only UI path"), DayNight->CanRestNow());

		// The placed generic presenter observes the detached director on its next
		// world tick, registers, and lets the queued exact warning land.
		World->Tick(LEVELTICK_All, 0.30f);
		World->Tick(LEVELTICK_All, 0.30f);
		TestTrue(TEXT("SkyPresenter takes the released Heart presenter role"), Heart && Heart->HasValidWarningPresenter());
		TestFalse(TEXT("generic presenter clears the retained Cycle II warning pending state"), DayNight->IsWarningPresentationPending());
		TestEqual(TEXT("generic handoff exposes exactly GardenRot"), Heart ? Heart->GetLastEmittedWarningId() : NAME_None, FName(TEXT("GardenRot")));
		if (SkyPresenter)
		{
			TestEqual(TEXT("SkyPresenter receives exactly GardenRot after resume"), SkyPresenter->Test_GetLastPresentedWarningId(), FName(TEXT("GardenRot")));
		}
		TestTrue(TEXT("rest opens only after the generic GardenRot presentation"), DayNight->CanRestNow());

		// Establish a real Corruption runtime, then replace it with the same safe
		// Day snapshot. This must not become EndNight or an early-dawn callback.
		TestTrue(TEXT("Cycle II rest enters Dusk before live runtime abort proof"), DayNight->RequestRest());
		TestTrue(TEXT("Cycle II rest reached Dusk before live runtime abort proof"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk);
		DayNight->AdvanceToNextPhase();
		TestTrue(TEXT("Cycle II cadence enters Night before live runtime abort proof"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night);
		TestTrue(TEXT("old Corruption runtime is active before restore"), Runtime->IsNightActive());
		TestNotNull(TEXT("old Corruption runtime owns an active strategy before restore"), Runtime->Test_GetActiveStrategy());
		TestTrue(TEXT("old Corruption runtime scheduled pressure before restore"), Runtime->Test_IsPressureCadenceScheduled());
		TestTrue(TEXT("old Corruption runtime spawned its pressure actor before restore"), Runtime->Test_HasActivePressureActor());
		const int32 DawnRequestsBeforeRestore = DayNight->Test_GetCadenceDawnRequestCount();

		TestTrue(TEXT("safe Day reload aborts the active old runtime"), DayNight->LoadProgressionFromSlot(SafeDaySlot));
		TestTrue(TEXT("safe reload returns the phase authority to Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
		TestFalse(TEXT("old runtime is inactive after restore abort"), Runtime->IsNightActive());
		TestNull(TEXT("old runtime strategy is cleared without ResolveNight"), Runtime->Test_GetActiveStrategy());
		TestFalse(TEXT("old pressure cadence is cleared before restored PCG can tick"), Runtime->Test_IsPressureCadenceScheduled());
		TestFalse(TEXT("old pressure actor is destroyed before restored PCG can tick"), Runtime->Test_HasActivePressureActor());
		TestEqual(TEXT("restore abort leaves no old night outcome to record"), Runtime->GetLastOutcome().Result, ENightOutcomeResult::None);
		TestFalse(TEXT("restore abort removes the stale early-dawn callback"),
			Runtime->OnNightShouldEnd.IsAlreadyBound(DayNight, &UGloamsteadDayNightSubsystem::HandleNightShouldEnd));

		const TArray<FRitualPointState>& RestoredBeforeTick = PCG->Test_PeekPointStates();
		TestEqual(TEXT("safe Day reload restores the saved PCG point count"), RestoredBeforeTick.Num(), SafeDayStates.Num());
		if (RestoredBeforeTick.IsValidIndex(0) && SafeDayStates.IsValidIndex(0))
		{
			TestTrue(TEXT("safe Day reload restores corruption before stale timer opportunity"),
				FMath::IsNearlyEqual(RestoredBeforeTick[0].CorruptionLevel, SafeDayStates[0].CorruptionLevel));
			TestTrue(TEXT("safe Day reload restores light before stale timer opportunity"),
				FMath::IsNearlyEqual(RestoredBeforeTick[0].LightLevel, SafeDayStates[0].LightLevel));
		}

		World->Tick(LEVELTICK_All, 0.10f);
		TestTrue(TEXT("a stale runtime tick cannot force restored Day into Dawn"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
		TestEqual(TEXT("a stale runtime tick cannot request a new Dawn"), DayNight->Test_GetCadenceDawnRequestCount(), DawnRequestsBeforeRestore);
		const TArray<FRitualPointState>& RestoredAfterTick = PCG->Test_PeekPointStates();
		if (RestoredAfterTick.IsValidIndex(0) && SafeDayStates.IsValidIndex(0))
		{
			TestTrue(TEXT("a stale runtime tick cannot mutate restored corruption"),
				FMath::IsNearlyEqual(RestoredAfterTick[0].CorruptionLevel, SafeDayStates[0].CorruptionLevel));
			TestTrue(TEXT("a stale runtime tick cannot mutate restored light"),
				FMath::IsNearlyEqual(RestoredAfterTick[0].LightLevel, SafeDayStates[0].LightLevel));
		}
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	GameInstance->Shutdown();
	UGameplayStatics::DeleteGameInSlot(SafeDaySlot, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
