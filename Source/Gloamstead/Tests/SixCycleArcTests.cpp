// The whole authored experience, played once, end to end, in ONE live world.
//
// Every other cycle proof in this suite verifies a slot in isolation (Gloamstead.Experience.Plan.SlotOne
// ..SlotSix each check one authored row; Gloamstead.PlayableCycle.* drives Cycle I and the front half of
// Cycle II). Nothing walked 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> ending continuously, which is exactly the claim
// a shippable game has to be able to make: the arc the player is offered can actually be finished.
//
// This test is deliberately a PLAYTHROUGH rather than a state-machine poke. Each cycle is driven through
// the same seams a player has:
//   - the Day arms and the Heart SPEAKS the cycle's exact authored warning (no plan is asserted from the
//     catalog alone; it has to reach a registered presenter),
//   - the cycle's evidence is learned from live placed AGloamsteadEvidenceSource actors,
//   - the restoration is committed through URitualPlacementComponent's private placement-authority tail,
//     which is the only route that can mint an interpretation receipt,
//   - the phases turn through IGloamInteractable::Execute_Interact on the Heart.
// No test-only shortcut is used to open a gate. Where a seam is test-only (PCG point seeding, the
// placement commit wrapper) it is the same one the isolated cycle proofs use, and it still runs the
// production code underneath.
#include "Misc/AutomationTest.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "Actors/GloamsteadEvidenceSource.h"
#include "Components/RitualPlacementComponent.h"
#include "Components/GloamsteadSurveySubjectComponent.h"
#include "Presentation/GloamsteadSkyPresenter.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Interfaces/GloamInteractable.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/RitualTypes.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/Guid.h"

#if WITH_DEV_AUTOMATION_TESTS

// A NAMED namespace on purpose. This module builds through UBT adaptive unity, and helpers in an
// anonymous namespace have already collided across merged translation units here (C2264).
namespace GloamsteadArcFixtures
{
	/**
	 * UWorld::CreateWorld test worlds are brought up by hand. Actors spawned after that setup do not
	 * receive BeginPlay in every editor automation configuration, so drive the documented lifecycle
	 * dispatch once and only once. This keeps the proof on real actor bindings rather than on manually
	 * invoked gameplay callbacks.
	 */
	static void EnsureArcActorBegunPlay(AActor* Actor)
	{
		if (IsValid(Actor) && !Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
	}

	/**
	 * FTimerManager intentionally permits one tick per GFrameCounter, and automation runs this whole
	 * body inside a single editor frame. Pump two synthetic frames: the first promotes a deferred timer,
	 * the second executes it. This stays on UWorld's normal timer/actor paths.
	 */
	static void PumpArcWorld(UWorld* World, float DeltaSeconds)
	{
		if (!World)
		{
			return;
		}
		for (int32 Frame = 0; Frame < 2; ++Frame)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, DeltaSeconds);
		}
	}

	/** The authored arc, in order. Mirrors PopulateDefaultExperienceCyclePlans (ExperienceCycleTypes.cpp:195-410). */
	static const TCHAR* const ArcPlanIds[6] = {
		TEXT("Cycle1_Tutorial"),
		TEXT("Cycle2_Garden"),
		TEXT("Cycle3_Road"),
		TEXT("Cycle4_Mirror"),
		TEXT("Cycle5_Bell"),
		TEXT("Cycle6_Siege")
	};

	static const TCHAR* const ArcWarningIds[6] = {
		TEXT("TutorialLostPath"),
		TEXT("GardenRot"),
		TEXT("RoadUnbound"),
		TEXT("StolenLight"),
		TEXT("BellBargain"),
		TEXT("ThreeLights")
	};

	/** One sanctuary point per authored cycle, plus slack. Cycle N mends point N+1. */
	static constexpr int32 ArcPointCount = 8;
	static int32 RestorationPointForSlot(int32 Slot) { return Slot + 1; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamWholeAuthoredArcTest,
	"Gloamstead.Arc.TheWholeAuthoredExperienceCanBePlayedToItsEnding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamWholeAuthoredArcTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamsteadArcFixtures;

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
		// The arc must never touch a player's real save.
		DayNight->SetDawnAutosaveEnabled(false);

		// Deliberately NOT injecting a fixture catalog. This proof is about the SHIPPED authored
		// experience, so both the experience catalog and the Heart's warning catalog are the ones a
		// player build loads. (Their contract coverage is separately proved by
		// Gloamstead.Experience.Plan.ShippedCatalogIsAuthoredAndMatchesFallback and
		// Gloamstead.Experience.Warning.ShippedCatalogCoversEveryAuthoredPlan.) The slot count is
		// asserted below, once the first Day has actually armed a plan: the catalog loads lazily inside
		// EnsureUpcomingPlan, so reading it before that would only prove the accessor is lazy.

		// A small sanctuary with one mendable place per cycle.
		TArray<FVector> Locations;
		for (int32 Index = 0; Index < ArcPointCount; ++Index)
		{
			Locations.Add(FVector(Index * 250.0f, 0.0f, 0.0f));
		}
		PCG->Test_SeedPoints(Locations);
		TArray<FRitualPointState> States;
		for (int32 Index = 0; Index < ArcPointCount; ++Index)
		{
			FRitualPointState State;
			State.LightLevel = 0.40f;
			State.CorruptionLevel = (Index == 0) ? 0.60f : 0.15f;
			State.bIsRestored = false;
			States.Add(State);
		}
		PCG->Test_SeedPointStates(States);

		AVeilHeart* Heart = World->SpawnActor<AVeilHeart>();
		TestNotNull(TEXT("the Heart exists in the live world"), Heart);
		if (!Heart)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			GameInstance->Shutdown();
			return false;
		}
		EnsureArcActorBegunPlay(Heart);
		TestTrue(TEXT("the shipped warning catalog loaded for the live Heart"), Heart->IsInterpretationCatalogReady());

		// The placement authority's evidence publisher needs one declared survey subject, exactly as the
		// isolated Cycle II proof arranges it; without it every confirmation logs a GSS publication error.
		UGloamsteadSurveySubjectComponent* EvidenceSubject = NewObject<UGloamsteadSurveySubjectComponent>(Heart);
		TestNotNull(TEXT("a survey subject component exists for placement evidence"), EvidenceSubject);
		if (EvidenceSubject)
		{
			EvidenceSubject->SubjectId = TEXT("courtyard.lantern.first");
			TestTrue(TEXT("the live world declares the placement evidence subject"), EvidenceSubject->RegisterWithRegistry());
		}

		// The only route that can mint an interpretation receipt. Blueprint-exposed PCG mutation cannot.
		URitualPlacementComponent* Placement = NewObject<URitualPlacementComponent>(Heart);
		TestNotNull(TEXT("a placement authority exists for the arc's restorations"), Placement);

		// The sky is the generic warning presenter for every cycle after the tutorial director detaches.
		AGloamsteadSkyPresenter* SkyPresenter = World->SpawnActor<AGloamsteadSkyPresenter>();
		TestNotNull(TEXT("the global phase/warning presenter exists"), SkyPresenter);
		EnsureArcActorBegunPlay(SkyPresenter);

		AGloamsteadFirstNightDirector* FirstNightDirector = World->SpawnActor<AGloamsteadFirstNightDirector>();
		TestNotNull(TEXT("the first-night director exists for Cycle I"), FirstNightDirector);
		EnsureArcActorBegunPlay(FirstNightDirector);

		// --- shared per-cycle drivers -------------------------------------------------------------

		// Learn the cycle's evidence from live placed sources and mend its place through the placement
		// authority. This is the whole fair-crypticism contract: two distinct authored support channels
		// in their authored media, then one restoration on a point whose PCG metadata matches the plan.
		auto EarnInterpretationForPlan = [this, World, PCG, Placement, Heart](const FExperienceCyclePlan& Plan) -> bool
		{
			const int32 PointIndex = RestorationPointForSlot(Plan.Slot);
			const FString CycleLabel = Plan.PlanId.ToString();

			if (!TestTrue(*FString::Printf(TEXT("%s declares its full evidence contract"), *CycleLabel),
				Plan.RequiredSupportIds.Num() >= 2
				&& Plan.RequiredSupportChannelTypes.Num() == Plan.RequiredSupportIds.Num()
				&& Plan.MinimumDistinctSupportCount >= 2
				&& Plan.RequiredRestorationTags.Num() == 1))
			{
				return false;
			}

			// Give the place the authored identity the plan asks for, through the same production writer
			// an authored ritual site uses.
			if (!TestTrue(*FString::Printf(TEXT("%s point carries the authored contract metadata"), *CycleLabel),
				PCG->Test_SetPointContractMetadata(PointIndex, Plan.WarningId, Plan.SemanticSubject,
					Plan.RequiredRitualType, Plan.RequiredRestorationTags[0])))
			{
				return false;
			}

			for (int32 SupportIndex = 0; SupportIndex < Plan.MinimumDistinctSupportCount; ++SupportIndex)
			{
				AGloamsteadEvidenceSource* Source = World->SpawnActor<AGloamsteadEvidenceSource>();
				if (!TestNotNull(*FString::Printf(TEXT("%s evidence source %d spawns"), *CycleLabel, SupportIndex), Source))
				{
					return false;
				}
				Source->WarningId = Plan.WarningId;
				Source->SupportId = Plan.RequiredSupportIds[SupportIndex];
				Source->ChannelType = Plan.RequiredSupportChannelTypes[SupportIndex];
				TestTrue(*FString::Printf(TEXT("%s support '%s' is learned from its placed source"),
					*CycleLabel, *Source->SupportId.ToString()), Source->ReportEncounter(nullptr));
			}

			AActor* RestoredActor = World->SpawnActor<AActor>();
			if (!TestNotNull(*FString::Printf(TEXT("%s restored actor spawns"), *CycleLabel), RestoredActor))
			{
				return false;
			}

			FRestorationEventPayload Payload;
			Payload.PointIndex = PointIndex;
			Payload.RestoredActor = RestoredActor;
			Payload.WarningId = Plan.WarningId;
			Payload.SemanticSubject = Plan.SemanticSubject;
			Payload.RitualType = Plan.RequiredRitualType;
			Payload.WarningTagSatisfied = Plan.RequiredRestorationTags[0];
			Payload.WorldLocation = FVector(PointIndex * 250.0f, 0.0f, 0.0f);

			const bool bCommitted = Placement->Test_CommitRestorationWithEvidence(
				PCG, PointIndex, Payload, FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
			TestTrue(*FString::Printf(TEXT("%s restoration commits through the placement authority"), *CycleLabel), bCommitted);
			TestTrue(*FString::Printf(TEXT("%s mints its exact interpretation receipt"), *CycleLabel),
				Heart->HasExactInterpretationForPlan(Plan));
			return bCommitted && Heart->HasExactInterpretationForPlan(Plan);
		};

		// Day -> Dusk -> Night -> Dawn, entirely at the Heart, then prove the night actually resolved.
		auto DriveNightToDawn = [this, World, DayNight, Runtime, Heart](const FExperienceCyclePlan& Plan)
		{
			const FString CycleLabel = Plan.PlanId.ToString();

			TestTrue(*FString::Printf(TEXT("%s: the Heart offers rest on its Day"), *CycleLabel),
				IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			IGloamInteractable::Execute_Interact(Heart, nullptr);
			TestEqual(*FString::Printf(TEXT("%s: resting at the Heart brings Dusk"), *CycleLabel),
				DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Dusk);

			TestTrue(*FString::Printf(TEXT("%s: Dusk waits for the player to bring the night"), *CycleLabel),
				DayNight->CanBeginNightNow());
			TestTrue(*FString::Printf(TEXT("%s: the Heart answers at Dusk"), *CycleLabel),
				IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			IGloamInteractable::Execute_Interact(Heart, nullptr);
			TestEqual(*FString::Printf(TEXT("%s: the player brings the Night"), *CycleLabel),
				DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Night);

			// Night presentation defers the runtime start by one frame; pump it as a real frame would.
			PumpArcWorld(World, 0.05f);

			// A night whose objective resolves inside that first beat is already at Dawn. Only force the
			// deadline transition when the night is genuinely still running.
			if (DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night)
			{
				DayNight->AdvanceToNextPhase();
			}
			TestEqual(*FString::Printf(TEXT("%s: the night ends at Dawn"), *CycleLabel),
				DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Dawn);
			PumpArcWorld(World, 0.05f);

			const FNightRuntimeOutcome Outcome = Runtime->GetLastOutcome();
			TestTrue(*FString::Printf(TEXT("%s: the night produced a real outcome"), *CycleLabel),
				Outcome.Result != ENightOutcomeResult::None);
			TestEqual(*FString::Printf(TEXT("%s: the outcome names this cycle's authored night type"), *CycleLabel),
				Outcome.NightType, Plan.NightType);
			TestEqual(*FString::Printf(TEXT("%s: the Heart reflected on that same outcome at dawn"), *CycleLabel),
				Heart->GetLastNightOutcome().Result, Outcome.Result);
		};

		// --- Cycle I: the lantern tutorial opens the arc -------------------------------------------

		TestEqual(TEXT("the arc opens in Day"), DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Day);
		{
			// Bootstrap arms the Tutorial plan, but its warning stays closed behind the lantern gate
			// (GloamsteadDayNightSubsystem.cpp:333-341), so preparation is expected to report false here.
			TestFalse(TEXT("pre-lantern preparation keeps the Tutorial warning closed"), DayNight->PrepareUpcomingCycle());
			TestEqual(TEXT("the authored experience defines six cycles"), Experience->GetAuthoredSlotCount(), 6);
			const FExperienceCyclePlan* TutorialPlan = DayNight->GetUpcomingPlan();
			TestNotNull(TEXT("Cycle I arms an authored plan"), TutorialPlan);
			if (TutorialPlan)
			{
				TestEqual(TEXT("Cycle I is Cycle1_Tutorial"), TutorialPlan->PlanId, FName(ArcPlanIds[0]));
			}
		}
		TestFalse(TEXT("the Heart offers no rest before the lantern lesson"),
			IGloamInteractable::Execute_CanInteract(Heart, nullptr));
		TestEqual(TEXT("no warning is spoken before the lantern lesson"), Heart->GetLastEmittedWarningId(), NAME_None);

		{
			FRestorationEventPayload LanternRestore;
			LanternRestore.RitualType = ERitualType::LanternPost;
			LanternRestore.WorldLocation = FVector(125.0f, 0.0f, 0.0f);
			FirstNightDirector->HandleStructureRestored(LanternRestore);
		}
		TestTrue(TEXT("the lantern lesson opens the first rest"), DayNight->IsFirstRestUnlocked());

		int32 CompletedNights = 0;
		for (int32 Slot = 1; Slot <= 6; ++Slot)
		{
			const int32 Index = Slot - 1;
			const FString CycleLabel = ArcPlanIds[Index];

			const FExperienceCyclePlan* ArmedPlan = DayNight->GetUpcomingPlan();
			if (!TestNotNull(*FString::Printf(TEXT("%s: the Day arms an authored plan"), *CycleLabel), ArmedPlan))
			{
				break;
			}
			// Copy: the subsystem's active plan is replaced as the cycle turns.
			const FExperienceCyclePlan Plan = *ArmedPlan;

			// 1. The Day arms the expected plan, in order.
			TestEqual(*FString::Printf(TEXT("%s: the Day arms the expected plan id"), *CycleLabel),
				Plan.PlanId, FName(ArcPlanIds[Index]));
			TestEqual(*FString::Printf(TEXT("%s: the armed plan is in the expected arc slot"), *CycleLabel),
				Plan.Slot, Slot);

			// 2. The cycle's exact warning is actually spoken by the Heart.
			TestEqual(*FString::Printf(TEXT("%s: the Heart speaks this cycle's exact warning"), *CycleLabel),
				Heart->GetLastEmittedWarningId(), FName(ArcWarningIds[Index]));
			TestEqual(*FString::Printf(TEXT("%s: the spoken warning is the plan's warning"), *CycleLabel),
				Heart->GetLastEmittedWarningId(), Plan.WarningId);

			// 3. The cycle's required restoration is committed and its receipt minted, so rest opens.
			//    Cycle I deliberately authors no evidence contract or receipt: night one teaches only
			//    "what I restore matters" (ExperienceCycleTypes.cpp:199-216). Its gate is the lantern.
			if (Slot == 1)
			{
				TestTrue(*FString::Printf(TEXT("%s: the tutorial authors no receipt to earn"), *CycleLabel),
					Plan.InterpretationReceiptId == NAME_None);
			}
			else
			{
				TestFalse(*FString::Printf(TEXT("%s: no receipt stands before the evidence is learned"), *CycleLabel),
					Heart->HasExactInterpretationForPlan(Plan));
				EarnInterpretationForPlan(Plan);
			}
			TestTrue(*FString::Printf(TEXT("%s: rest is open at the Heart"), *CycleLabel), DayNight->CanRestNow());

			// 4/5. The whole turn, and a real night outcome.
			DriveNightToDawn(Plan);

			// The exact consequence the plan named — never a score-selected substitute.
			TestEqual(*FString::Printf(TEXT("%s: the manager prepared this plan's exact night type"), *CycleLabel),
				Manager->GetLastSelectedNightType(), Plan.NightType);

			const bool bFinalCycle = (Slot == 6);
			if (!bFinalCycle)
			{
				// Wake at the Heart. Dawn -> Day is the only place the night counter turns.
				TestTrue(*FString::Printf(TEXT("%s: the Heart offers the dawn"), *CycleLabel),
					IGloamInteractable::Execute_CanInteract(Heart, nullptr));
				IGloamInteractable::Execute_Interact(Heart, nullptr);
				TestEqual(*FString::Printf(TEXT("%s: greeting the dawn opens the next Day"), *CycleLabel),
					DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Day);
				++CompletedNights;
				TestEqual(*FString::Printf(TEXT("%s: the night counter turned"), *CycleLabel),
					DayNight->GetNightCount(), CompletedNights);
				TestFalse(*FString::Printf(TEXT("%s: the arc is not over yet"), *CycleLabel),
					Experience->IsExperienceComplete());
				PumpArcWorld(World, 0.05f);
			}
			else
			{
				// 6. The sixth dawn IS the ending: HandleEnterDawn records the last authored outcome and
				//    then finds no seventh plan, so the experience resolves to the generic handoff
				//    (GloamsteadExperienceCycleSubsystem.cpp:235-246).
				TestTrue(TEXT("the authored experience is complete after the sixth dawn"),
					Experience->IsExperienceComplete());
				TestNull(TEXT("no seventh authored plan is armed"), DayNight->GetUpcomingPlan());

				// 7. And the Heart does NOT soft-lock. CanRestNow() is false forever from here, so the
				//    Heart deliberately keeps answering rather than going silent
				//    (VeilHeart.cpp:50-66).
				TestTrue(TEXT("the completed Heart still answers the player"),
					IGloamInteractable::Execute_CanInteract(Heart, nullptr));
				TestEqual(TEXT("the completed Heart offers the ending, not a rest"),
					IGloamInteractable::Execute_GetInteractionPrompt(Heart).ToString(),
					FString(TEXT("Sit with what the sanctuary remembers")));
				IGloamInteractable::Execute_Interact(Heart, nullptr);
				TestEqual(TEXT("the ending does not turn the arc into a seventh Day"),
					DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Dawn);

				// The sixth night still has its Dawn -> Day wrap; the phase authority owns it and the
				// counter turns there. The Heart simply chooses the ending over another night.
				TestTrue(TEXT("the sixth dawn can still wrap to Day"), DayNight->RequestRest());
				TestEqual(TEXT("the sixth dawn wrapped to Day"),
					DayNight->GetCurrentPhase(), EGloamsteadDayPhase::Day);
				++CompletedNights;
				TestEqual(TEXT("all six nights were counted"), DayNight->GetNightCount(), CompletedNights);
				TestTrue(TEXT("the experience stays complete after the last wrap"),
					Experience->IsExperienceComplete());
				TestTrue(TEXT("the Heart still answers after the last wrap"),
					IGloamInteractable::Execute_CanInteract(Heart, nullptr));
			}
		}

		TestEqual(TEXT("the whole authored arc ran six nights"), CompletedNights, 6);
		TestEqual(TEXT("the world's night counter agrees"), DayNight->GetNightCount(), 6);
		TestTrue(TEXT("the authored experience finished"), Experience->IsExperienceComplete());
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
