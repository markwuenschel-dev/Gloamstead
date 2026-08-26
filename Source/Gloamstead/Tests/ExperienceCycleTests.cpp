// Authored Cycle II plan invariants: early slots are contracts, never score-selected narratives.
#include "Misc/AutomationTest.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Engine/GameInstance.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/VeilHeart.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UExperienceCycleCatalog* MakeAuthoredCatalog()
	{
		UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>();
		PopulateDefaultExperienceCyclePlans(*Catalog);
		return Catalog;
	}

	UGloamsteadExperienceCycleSubsystem* MakeSubsystem(UExperienceCycleCatalog* Catalog)
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		UGloamsteadExperienceCycleSubsystem* Subsystem = NewObject<UGloamsteadExperienceCycleSubsystem>(GameInstance);
		Subsystem->Test_SetCatalog(Catalog);
		return Subsystem;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanSlotOneIsTutorialTest,
	"Gloamstead.Experience.Plan.SlotOneIsTutorial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanSlotOneIsTutorialTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());

	TestTrue(TEXT("the first authored plan resolves"), Subsystem->EnsureUpcomingPlan());
	const FExperienceCyclePlan& Plan = Subsystem->GetActivePlan();
	TestTrue(TEXT("the first plan is authored"), Plan.IsAuthoredPlan());
	TestEqual(TEXT("slot one is selected"), Plan.Slot, 1);
	TestEqual(TEXT("slot one uses the tutorial night"), Plan.NightType, ENightConsequenceType::Tutorial);
	TestEqual(TEXT("slot one keeps its stable id"), Plan.PlanId, FName(TEXT("Cycle1_Tutorial")));
	TestEqual(TEXT("slot one uses the shipped TutorialLostPath warning identity"), Plan.WarningId, FName(TEXT("TutorialLostPath")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanSlotTwoIsExactCorruptionTest,
	"Gloamstead.Experience.Plan.SlotTwoIsExactCorruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanSlotTwoIsExactCorruptionTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 1;
	TestTrue(TEXT("unarmed completed tutorial state restores"), Subsystem->RestorePersistentState(State));

	TestTrue(TEXT("the second authored plan resolves"), Subsystem->EnsureUpcomingPlan());
	const FExperienceCyclePlan& Plan = Subsystem->GetActivePlan();
	TestTrue(TEXT("the second plan is authored"), Plan.IsAuthoredPlan());
	TestEqual(TEXT("slot two is selected"), Plan.Slot, 2);
	TestEqual(TEXT("slot two uses Corruption"), Plan.NightType, ENightConsequenceType::Corruption);
	TestEqual(TEXT("slot two uses the exact warning"), Plan.WarningId, FName(TEXT("GardenRot")));
	TestEqual(TEXT("slot two uses the exact garden subject"), Plan.SemanticSubject, FName(TEXT("Cycle2_Garden")));
	TestEqual(TEXT("slot two keeps its stable id"), Plan.PlanId, FName(TEXT("Cycle2_Garden")));
	TestEqual(TEXT("slot two requires the canonical GardenBed ritual"), Plan.RequiredRitualType, ERitualType::GardenBed);
	TestEqual(TEXT("slot two requires two distinct readable supports"), Plan.MinimumDistinctSupportCount, 2);
	TestEqual(TEXT("slot two has exactly three authored support ids"), Plan.RequiredSupportIds.Num(), 3);
	TestTrue(TEXT("slot two names the withered-vines support"), Plan.RequiredSupportIds.Contains(FName(TEXT("GardenRot.WitheredVines"))));
	TestEqual(TEXT("slot two names one medium for each authored support"), Plan.RequiredSupportChannelTypes.Num(), 3);
	TestEqual(TEXT("the vines are readable in the environment"), Plan.RequiredSupportChannelTypes[0], FName(TEXT("Environmental")));
	TestEqual(TEXT("the cold soil reacts as an object"), Plan.RequiredSupportChannelTypes[1], FName(TEXT("ObjectReaction")));
	TestEqual(TEXT("the moths are an audio clue"), Plan.RequiredSupportChannelTypes[2], FName(TEXT("Audio")));
	TestEqual(TEXT("slot two records the exact interpretation receipt id"), Plan.InterpretationReceiptId, FName(TEXT("GardenRot.Interpreted")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanDawnOutcomeAdvancesCycleTest,
	"Gloamstead.Experience.Plan.DawnOutcomeAdvancesCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanDawnOutcomeAdvancesCycleTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 1;
	TestTrue(TEXT("completed tutorial state restores"), Subsystem->RestorePersistentState(State));
	TestTrue(TEXT("Cycle II is armed before its dawn"), Subsystem->EnsureUpcomingPlan());

	FNightRuntimeOutcome Outcome;
	Outcome.NightType = ENightConsequenceType::Corruption;
	Outcome.Result = ENightOutcomeResult::Failure;
	Outcome.ResultTag = TEXT("GardenScar");
	TestTrue(TEXT("the active Cycle II plan records its dawn outcome"), Subsystem->RecordActivePlanOutcome(Outcome));

	const FExperienceCyclePersistentState Captured = Subsystem->CapturePersistentState();
	TestEqual(TEXT("dawn completes the exact active slot"), Captured.CompletedCycleSlot, 2);
	TestEqual(TEXT("dawn records the exact active plan id"), Captured.LastPlanId, FName(TEXT("Cycle2_Garden")));
	TestEqual(TEXT("dawn records the durable result tag"), Captured.LastOutcomeResultTag, FName(TEXT("GardenScar")));
	TestTrue(TEXT("a failure result is retained as a scar"), Captured.ScarTags.Contains(FName(TEXT("GardenScar"))));
	TestEqual(TEXT("dawn clears the armed plan for the next Day"), Captured.ArmedPlanId, NAME_None);
	TestTrue(TEXT("dawn clears the active authored plan"), Subsystem->GetActivePlan().IsInvalid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanSlotThreeIsBrokenRoadTest,
	"Gloamstead.Experience.Plan.SlotThreeIsExactRetrieval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanSlotThreeIsBrokenRoadTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 2;
	TestTrue(TEXT("completed Cycle II state restores"), Subsystem->RestorePersistentState(State));

	TestTrue(TEXT("the third authored plan resolves"), Subsystem->EnsureUpcomingPlan());
	const FExperienceCyclePlan& Plan = Subsystem->GetActivePlan();
	TestTrue(TEXT("the third plan is authored"), Plan.IsAuthoredPlan());
	TestEqual(TEXT("slot three is selected"), Plan.Slot, 3);
	TestEqual(TEXT("slot three uses Retrieval"), Plan.NightType, ENightConsequenceType::Retrieval);
	TestEqual(TEXT("slot three owns the broken-road warning"), Plan.WarningId, FName(TEXT("RoadUnbound")));
	TestEqual(TEXT("slot three names its own place, not the garden"), Plan.SemanticSubject, FName(TEXT("Cycle3_Road")));
	TestEqual(TEXT("slot three keeps its stable id"), Plan.PlanId, FName(TEXT("Cycle3_Road")));
	TestEqual(TEXT("slot three asks for the PathPoint ritual"), Plan.RequiredRitualType, ERitualType::PathPoint);
	TestEqual(TEXT("slot three requires two distinct readable supports"), Plan.MinimumDistinctSupportCount, 2);
	TestEqual(TEXT("slot three records a road-specific interpretation receipt"), Plan.InterpretationReceiptId, FName(TEXT("RoadUnbound.Interpreted")));
	TestEqual(TEXT("slot three offers three authored support ids"), Plan.RequiredSupportIds.Num(), 3);
	TestTrue(TEXT("slot three names the broken-flagstones clue"), Plan.RequiredSupportIds.Contains(FName(TEXT("RoadUnbound.BrokenFlagstones"))));
	TestTrue(TEXT("slot three names the leaning-waymark clue"), Plan.RequiredSupportIds.Contains(FName(TEXT("RoadUnbound.LeaningWaymark"))));
	TestTrue(TEXT("slot three names the dragging-step clue"), Plan.RequiredSupportIds.Contains(FName(TEXT("RoadUnbound.DraggingStep"))));

	// The second clause: loops guard, dead ends invite hands.
	TestTrue(TEXT("slot three offers a second reading"), Plan.OffersSecondReading());
	TestTrue(TEXT("its reading set is coherent"), Plan.HasCoherentSecondReadings());
	const FExperienceCycleSecondReading* Loop = Plan.FindSecondReading(FName(TEXT("RoadUnbound.CloseTheLoop")));
	const FExperienceCycleSecondReading* DeadEnd = Plan.FindSecondReading(FName(TEXT("RoadUnbound.ReachTheOuterGate")));
	if (TestNotNull(TEXT("closing the loop is authored"), Loop)
		&& TestNotNull(TEXT("running the road to the outer gate is authored"), DeadEnd))
	{
		TestEqual(TEXT("closing the loop is the sharper read"), Loop->Grade, EExperienceReadingGrade::Insight);
		TestEqual(TEXT("the outer-gate spur is the overread"), DeadEnd->Grade, EExperienceReadingGrade::Overreach);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanSlotFourIsOverlookMirrorTest,
	"Gloamstead.Experience.Plan.SlotFourIsExactPossession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanSlotFourIsOverlookMirrorTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 3;
	TestTrue(TEXT("completed Cycle III state restores"), Subsystem->RestorePersistentState(State));

	TestTrue(TEXT("the fourth authored plan resolves"), Subsystem->EnsureUpcomingPlan());
	const FExperienceCyclePlan& Plan = Subsystem->GetActivePlan();
	TestTrue(TEXT("the fourth plan is authored"), Plan.IsAuthoredPlan());
	TestEqual(TEXT("slot four is selected"), Plan.Slot, 4);
	TestEqual(TEXT("slot four uses SilencePossession"), Plan.NightType, ENightConsequenceType::SilencePossession);
	TestEqual(TEXT("slot four owns the stolen-light warning"), Plan.WarningId, FName(TEXT("StolenLight")));
	TestEqual(TEXT("slot four climbs to the overlook"), Plan.SemanticSubject, FName(TEXT("Cycle4_Overlook")));
	TestEqual(TEXT("slot four keeps its stable id"), Plan.PlanId, FName(TEXT("Cycle4_Mirror")));
	TestEqual(TEXT("slot four asks for the MirrorPillar ritual"), Plan.RequiredRitualType, ERitualType::MirrorPillar);
	TestEqual(TEXT("slot four requires two readable supports"), Plan.MinimumDistinctSupportCount, 2);
	TestEqual(TEXT("slot four records a stolen-light receipt"), Plan.InterpretationReceiptId, FName(TEXT("StolenLight.Interpreted")));
	TestEqual(TEXT("slot four offers three authored support ids"), Plan.RequiredSupportIds.Num(), 3);

	// The second clause: face stolen light, never show it the Heart.
	const FExperienceCycleSecondReading* FaceLantern = Plan.FindSecondReading(FName(TEXT("StolenLight.FaceTheLantern")));
	const FExperienceCycleSecondReading* FaceHeart = Plan.FindSecondReading(FName(TEXT("StolenLight.FaceTheHeart")));
	if (TestNotNull(TEXT("facing the lantern is authored"), FaceLantern)
		&& TestNotNull(TEXT("facing the Heart is authored"), FaceHeart))
	{
		TestEqual(TEXT("facing the stolen light is the sharper read"), FaceLantern->Grade, EExperienceReadingGrade::Insight);
		TestEqual(TEXT("facing the Heart is the overread"), FaceHeart->Grade, EExperienceReadingGrade::Overreach);
		TestEqual(TEXT("the sharper read exposes the tether"), FaceLantern->ConsequenceTag, FName(TEXT("Boon.TetherExposed")));
		TestEqual(TEXT("the overread reveals the centre"), FaceHeart->ConsequenceTag, FName(TEXT("Scar.HeartRevealed")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanExactWarningAndNightPrepTest,
	"Gloamstead.Experience.Plan.ExactWarningAndNightPrep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanExactWarningAndNightPrepTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 1;
	TestTrue(TEXT("completed tutorial state restores"), Subsystem->RestorePersistentState(State));
	TestTrue(TEXT("Cycle II arms before any generic selection"), Subsystem->EnsureUpcomingPlan());
	const FExperienceCyclePlan Plan = Subsystem->GetActivePlan();

	UVeilHeartWarningCatalog* WarningCatalog = NewObject<UVeilHeartWarningCatalog>();
	FVeilHeartWarningFragment GardenWarning;
	GardenWarning.WarningId = TEXT("GardenRot");
	GardenWarning.AssociatedNightType = ENightConsequenceType::Corruption;
	GardenWarning.Fragment = FText::FromString(TEXT("What grows in darkness must be tended before the bell tolls."));
	GardenWarning.SatisfiableTags = Plan.RequiredRestorationTags;
	GardenWarning.SemanticSubject = Plan.SemanticSubject;
	GardenWarning.RequiredRitualType = Plan.RequiredRitualType;
	GardenWarning.InterpretationReceiptId = Plan.InterpretationReceiptId;
	for (int32 Index = 0; Index < Plan.RequiredSupportIds.Num(); ++Index)
	{
		FVeilHeartWarningSupportChannel& Channel = GardenWarning.SupportChannels.AddDefaulted_GetRef();
		Channel.SupportId = Plan.RequiredSupportIds[Index];
		Channel.ChannelType = Plan.RequiredSupportChannelTypes[Index];
		Channel.EvidenceText = FText::FromString(TEXT("Readable authored evidence."));
	}
	WarningCatalog->Warnings.Add(GardenWarning);
	AVeilHeart* Heart = NewObject<AVeilHeart>();
	Heart->WarningCatalog = WarningCatalog;
	AGloamsteadFirstNightDirector* Presenter = NewObject<AGloamsteadFirstNightDirector>();
	Heart->OnWarningEmittedDelegate.AddDynamic(Presenter, &AGloamsteadFirstNightDirector::HandleHeartWarning);
	TestFalse(TEXT("an incidental warning observer is not a registered player-facing presenter"), Heart->HasValidWarningPresenter());
	TestFalse(TEXT("an incidental warning observer cannot emit an exact player-facing warning"), Heart->EmitWarningById(Plan.WarningId, Plan.NightType));
	TestTrue(TEXT("a live warning binding can register as the designated presenter"),
		Heart->RegisterWarningPresenter(Presenter, GET_FUNCTION_NAME_CHECKED(AGloamsteadFirstNightDirector, HandleHeartWarning)));
	TestTrue(TEXT("a registered presenter must retain its exact live warning binding"), Heart->HasValidWarningPresenter());
	TestTrue(TEXT("the exact Cycle II warning emits"), Heart->EmitWarningById(Plan.WarningId, Plan.NightType));
	TestEqual(TEXT("the exact Cycle II warning identity is emitted"), Heart->GetLastEmittedWarningId(), Plan.WarningId);
	TestFalse(TEXT("an absent warning id emits no substitute"), Heart->EmitWarningById(TEXT("AbsentWarning"), Plan.NightType));
	TestEqual(TEXT("an absent warning id preserves the prior exact emission"), Heart->GetLastEmittedWarningId(), Plan.WarningId);
	TestFalse(TEXT("a mismatched expected type emits no substitute"), Heart->EmitWarningById(Plan.WarningId, ENightConsequenceType::Tutorial));

	WarningCatalog->Warnings.Add(GardenWarning);
	TestFalse(TEXT("a duplicate warning id emits no substitute"), Heart->EmitWarningById(Plan.WarningId, Plan.NightType));

	FVeilHeartWarningFragment RetrievalWarning = GardenWarning;
	RetrievalWarning.AssociatedNightType = ENightConsequenceType::Retrieval;
	RetrievalWarning.InterpretationReceiptId = TEXT("GardenRot.Retrieved");
	WarningCatalog->Warnings.Add(RetrievalWarning);
	TestTrue(TEXT("the same warning identity is selectable for its distinct Retrieval night type"),
		Heart->HasExactWarningById(Plan.WarningId, ENightConsequenceType::Retrieval));

	Heart->OnWarningEmittedDelegate.RemoveDynamic(Presenter, &AGloamsteadFirstNightDirector::HandleHeartWarning);
	TestFalse(TEXT("a registered presenter loses readiness when its warning binding is removed"), Heart->HasValidWarningPresenter());
	Heart->UnregisterWarningPresenter(Presenter);

	UVeilHeartWarningCatalog* WrongMediumCatalog = NewObject<UVeilHeartWarningCatalog>();
	FVeilHeartWarningFragment WrongMediumWarning = GardenWarning;
	WrongMediumWarning.SupportChannels[2].ChannelType = TEXT("Environmental");
	WrongMediumCatalog->Warnings.Add(WrongMediumWarning);
	AVeilHeart* WrongMediumHeart = NewObject<AVeilHeart>();
	WrongMediumHeart->WarningCatalog = WrongMediumCatalog;
	WrongMediumHeart->Test_SetActivePlan(Plan);
	WrongMediumHeart->OnWarningEmittedDelegate.AddDynamic(Presenter, &AGloamsteadFirstNightDirector::HandleHeartWarning);
	TestTrue(TEXT("the presenter can register with the wrong-medium fixture"),
		WrongMediumHeart->RegisterWarningPresenter(Presenter, GET_FUNCTION_NAME_CHECKED(AGloamsteadFirstNightDirector, HandleHeartWarning)));
	TestFalse(TEXT("GardenRot refuses Day presentation when its media contract is incomplete"),
		WrongMediumHeart->EmitWarningById(Plan.WarningId, Plan.NightType));

	UNightConsequenceManager* Manager = NewObject<UNightConsequenceManager>();
	TestTrue(TEXT("the manager accepts the exact armed authored plan"), Manager->PrepareNightConsequencesForPlan(Plan));
	TestEqual(TEXT("the manager retains the exact authored type"), Manager->GetLastSelectedNightType(), ENightConsequenceType::Corruption);

	FExperienceCyclePlan InvalidPlan = FExperienceCyclePlan::MakeInvalid(2);
	TestFalse(TEXT("the manager rejects an invalid plan without generic fallback"), Manager->PrepareNightConsequencesForPlan(InvalidPlan));
	TestEqual(TEXT("a rejected plan leaves no runnable type"), Manager->GetLastSelectedNightType(), ENightConsequenceType::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanSlotFiveIsBellBargainTest,
	"Gloamstead.Experience.Plan.SlotFiveIsMirrorBargain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanSlotFiveIsBellBargainTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 4;
	TestTrue(TEXT("completed possession state restores"), Subsystem->RestorePersistentState(State));

	TestTrue(TEXT("the fifth authored plan resolves"), Subsystem->EnsureUpcomingPlan());
	const FExperienceCyclePlan& Plan = Subsystem->GetActivePlan();
	TestTrue(TEXT("the fifth plan is authored"), Plan.IsAuthoredPlan());
	TestEqual(TEXT("slot five is selected"), Plan.Slot, 5);
	TestEqual(TEXT("slot five uses Bargain"), Plan.NightType, ENightConsequenceType::Bargain);
	TestEqual(TEXT("slot five owns the bell-bargain warning"), Plan.WarningId, FName(TEXT("BellBargain")));
	TestEqual(TEXT("slot five crosses to the bell shrine"), Plan.SemanticSubject, FName(TEXT("Cycle5_BellShrine")));
	TestEqual(TEXT("slot five keeps its stable id"), Plan.PlanId, FName(TEXT("Cycle5_Bell")));
	TestEqual(TEXT("slot five asks for the BellShrine ritual"), Plan.RequiredRitualType, ERitualType::BellShrine);
	TestEqual(TEXT("slot five requires two readable supports"), Plan.MinimumDistinctSupportCount, 2);
	TestTrue(TEXT("slot five names the worn inscription"), Plan.RequiredSupportIds.Contains(FName(TEXT("BellBargain.WornInscription"))));
	TestTrue(TEXT("slot five names the cracked clapper"), Plan.RequiredSupportIds.Contains(FName(TEXT("BellBargain.CrackedClapper"))));
	TestTrue(TEXT("slot five names the answering toll"), Plan.RequiredSupportIds.Contains(FName(TEXT("BellBargain.AnsweringToll"))));
	TestEqual(TEXT("slot five records the exact interpretation receipt"), Plan.InterpretationReceiptId, FName(TEXT("BellBargain.Interpreted")));

	// The second clause: one answer frees, three answers invite company.
	const FExperienceCycleSecondReading* Once = Plan.FindSecondReading(FName(TEXT("BellBargain.RingOnce")));
	const FExperienceCycleSecondReading* Thrice = Plan.FindSecondReading(FName(TEXT("BellBargain.RingThrice")));
	if (TestNotNull(TEXT("ringing once is authored"), Once)
		&& TestNotNull(TEXT("ringing three times is authored"), Thrice))
	{
		TestEqual(TEXT("one answer is the sharper read"), Once->Grade, EExperienceReadingGrade::Insight);
		TestEqual(TEXT("three answers is the overread"), Thrice->Grade, EExperienceReadingGrade::Overreach);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanSlotSixIsWholeSanctuaryTest,
	"Gloamstead.Experience.Plan.SlotSixIsWholeSanctuarySiege",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanSlotSixIsWholeSanctuaryTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 5;
	TestTrue(TEXT("completed bell state restores"), Subsystem->RestorePersistentState(State));

	TestTrue(TEXT("the sixth authored plan resolves"), Subsystem->EnsureUpcomingPlan());
	const FExperienceCyclePlan& Plan = Subsystem->GetActivePlan();
	TestTrue(TEXT("the sixth plan is authored"), Plan.IsAuthoredPlan());
	TestEqual(TEXT("slot six is selected"), Plan.Slot, 6);
	TestEqual(TEXT("slot six is the true siege"), Plan.NightType, ENightConsequenceType::TrueSiege);
	TestEqual(TEXT("slot six owns the three-lights warning"), Plan.WarningId, FName(TEXT("ThreeLights")));
	TestEqual(TEXT("slot six reads the whole sanctuary"), Plan.SemanticSubject, FName(TEXT("Cycle6_Sanctuary")));
	TestEqual(TEXT("slot six keeps its stable id"), Plan.PlanId, FName(TEXT("Cycle6_Siege")));
	TestEqual(TEXT("slot six binds rather than builds"), Plan.RequiredRitualType, ERitualType::AnchorStone);
	TestEqual(TEXT("slot six records the exact interpretation receipt"), Plan.InterpretationReceiptId, FName(TEXT("ThreeLights.Interpreted")));

	// The second clause: a closed ring holds, a crown breaks.
	const FExperienceCycleSecondReading* Ring = Plan.FindSecondReading(FName(TEXT("ThreeLights.ClosedRing")));
	const FExperienceCycleSecondReading* Crown = Plan.FindSecondReading(FName(TEXT("ThreeLights.CrownOnHeart")));
	if (TestNotNull(TEXT("the closed ring is authored"), Ring)
		&& TestNotNull(TEXT("the crown is authored"), Crown))
	{
		TestEqual(TEXT("the closed ring is the sharper read"), Ring->Grade, EExperienceReadingGrade::Insight);
		TestEqual(TEXT("crowning the Heart is the overread"), Crown->Grade, EExperienceReadingGrade::Overreach);
		TestEqual(TEXT("the ring holds"), Ring->ConsequenceTag, FName(TEXT("Boon.RingHeld")));
		TestEqual(TEXT("the crown breaks"), Crown->ConsequenceTag, FName(TEXT("Scar.CrownBroken")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanEveryCycleFromTwoOffersReadingsTest,
	"Gloamstead.Experience.Plan.EveryCycleAfterTheTutorialOffersACoherentSecondReading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanEveryCycleFromTwoOffersReadingsTest::RunTest(const FString& /*Parameters*/)
{
	// The structural promise of the whole arc, asserted once over the catalog rather than five times
	// by hand: Cycle I asks only for the minimum, and every cycle after it carries a complete
	// Insight / Plain / Overreach set with distinct durable tags.
	UExperienceCycleCatalog* Catalog = MakeAuthoredCatalog();
	TestEqual(TEXT("the authored arc is six cycles long"), Catalog->AuthoredPlans.Num(), 6);

	TSet<FName> AllConsequenceTags;
	for (const FExperienceCyclePlan& Plan : Catalog->AuthoredPlans)
	{
		FString ReadingError;
		if (!TestTrue(*FString::Printf(TEXT("%s has a coherent reading set (%s)"),
				*Plan.PlanId.ToString(), *ReadingError), Plan.HasCoherentSecondReadings(&ReadingError)))
		{
			continue;
		}

		if (Plan.Slot == 1)
		{
			TestFalse(TEXT("the tutorial asks only for the minimum"), Plan.OffersSecondReading());
			continue;
		}

		TestTrue(*FString::Printf(TEXT("%s offers a second reading"), *Plan.PlanId.ToString()),
			Plan.OffersSecondReading());

		int32 InsightCount = 0;
		int32 OverreachCount = 0;
		for (const FExperienceCycleSecondReading& Reading : Plan.SecondReadings)
		{
			if (Reading.Grade == EExperienceReadingGrade::Insight) { ++InsightCount; }
			if (Reading.Grade == EExperienceReadingGrade::Overreach) { ++OverreachCount; }
			if (Reading.ConsequenceTag != NAME_None)
			{
				// A tag shared across two cycles would make a boon or scar ambiguous the moment the
				// persisted set is read back, so uniqueness is checked across the whole arc.
				TestFalse(*FString::Printf(TEXT("%s is a unique durable tag"), *Reading.ConsequenceTag.ToString()),
					AllConsequenceTags.Contains(Reading.ConsequenceTag));
				AllConsequenceTags.Add(Reading.ConsequenceTag);
			}
		}
		TestEqual(*FString::Printf(TEXT("%s has exactly one sharper read"), *Plan.PlanId.ToString()), InsightCount, 1);
		TestEqual(*FString::Printf(TEXT("%s has exactly one overread"), *Plan.PlanId.ToString()), OverreachCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanRequiredSlotsFailClosedTest,
	"Gloamstead.Experience.Plan.RequiredSlotsFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanRequiredSlotsFailClosedTest::RunTest(const FString& /*Parameters*/)
{
	// Every refusal below now says why. Declaring it keeps this proof about the fail-closed BEHAVIOUR while
	// still asserting the diagnostic reaches the log.
	AddExpectedError(
		TEXT("has no authored row satisfying the canonical contract"),
		EAutomationExpectedErrorFlags::Contains,
		0);

	UExperienceCycleCatalog* Catalog = MakeAuthoredCatalog();
	Catalog->AuthoredPlans.RemoveAll([](const FExperienceCyclePlan& Plan) { return Plan.Slot == 2; });
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(Catalog);
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 1;
	TestTrue(TEXT("unarmed completed tutorial state restores"), Subsystem->RestorePersistentState(State));

	TestFalse(TEXT("missing required slot fails rather than selecting another plan"), Subsystem->EnsureUpcomingPlan());
	TestTrue(TEXT("failure produces an invalid plan result"), Subsystem->GetActivePlan().IsInvalid());
	TestEqual(TEXT("failure does not substitute a warning"), Subsystem->GetActivePlan().WarningId, NAME_None);

	Catalog = MakeAuthoredCatalog();
	const FExperienceCyclePlan DuplicateSlotTwoPlan = Catalog->AuthoredPlans[1];
	Catalog->AuthoredPlans.Add(DuplicateSlotTwoPlan);
	Subsystem = MakeSubsystem(Catalog);
	TestTrue(TEXT("duplicate catalog state restores"), Subsystem->RestorePersistentState(State));
	TestFalse(TEXT("duplicate required slot fails closed"), Subsystem->EnsureUpcomingPlan());
	TestTrue(TEXT("duplicate failure is invalid"), Subsystem->GetActivePlan().IsInvalid());

	Catalog = MakeAuthoredCatalog();
	Catalog->AuthoredPlans[1].WarningId = TEXT("SubstitutedWarning");
	Subsystem = MakeSubsystem(Catalog);
	TestTrue(TEXT("mismatched catalog state restores"), Subsystem->RestorePersistentState(State));
	TestFalse(TEXT("mismatched required slot fails closed"), Subsystem->EnsureUpcomingPlan());
	TestTrue(TEXT("mismatched failure is invalid"), Subsystem->GetActivePlan().IsInvalid());

	Catalog = MakeAuthoredCatalog();
	Catalog->AuthoredPlans[1].RequiredSupportIds.RemoveAt(0);
	Subsystem = MakeSubsystem(Catalog);
	TestTrue(TEXT("sparse support catalog state restores"), Subsystem->RestorePersistentState(State));
	TestFalse(TEXT("sparse authored support contract fails closed"), Subsystem->EnsureUpcomingPlan());
	TestTrue(TEXT("sparse support failure is invalid"), Subsystem->GetActivePlan().IsInvalid());

	Catalog = MakeAuthoredCatalog();
	Catalog->AuthoredPlans[1].RequiredSupportChannelTypes[2] = TEXT("Environmental");
	Subsystem = MakeSubsystem(Catalog);
	TestTrue(TEXT("wrong-medium catalog state restores before admission"), Subsystem->RestorePersistentState(State));
	TestFalse(TEXT("wrong-medium authored support contract fails closed"), Subsystem->EnsureUpcomingPlan());
	TestTrue(TEXT("wrong-medium support failure is invalid"), Subsystem->GetActivePlan().IsInvalid());

	Catalog = MakeAuthoredCatalog();
	Catalog->AuthoredPlans[1].InterpretationReceiptId = TEXT("SubstitutedReceipt");
	Subsystem = MakeSubsystem(Catalog);
	TestTrue(TEXT("mismatched receipt catalog state restores"), Subsystem->RestorePersistentState(State));
	TestFalse(TEXT("mismatched receipt contract fails closed"), Subsystem->EnsureUpcomingPlan());
	TestTrue(TEXT("mismatched receipt failure is invalid"), Subsystem->GetActivePlan().IsInvalid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanRestoresCanonicalIdTest,
	"Gloamstead.Experience.Plan.RestoresCanonicalId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanRestoresCanonicalIdTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 1;
	State.ArmedPlanId = TEXT("Cycle2_Garden");

	TestTrue(TEXT("a canonical armed plan restores"), Subsystem->RestorePersistentState(State));
	const FExperienceCyclePlan& RestoredPlan = Subsystem->GetActivePlan();
	TestTrue(TEXT("restored plan remains authored"), RestoredPlan.IsAuthoredPlan());
	TestEqual(TEXT("restored plan retains canonical id"), RestoredPlan.PlanId, State.ArmedPlanId);
	TestEqual(TEXT("restored plan retains exact subject"), RestoredPlan.SemanticSubject, FName(TEXT("Cycle2_Garden")));
	TestEqual(TEXT("captured state retains the armed id"), Subsystem->CapturePersistentState().ArmedPlanId, State.ArmedPlanId);

	State.ArmedPlanId = TEXT("Cycle2_Substitute");
	TestFalse(TEXT("unknown persisted plan id is rejected"), Subsystem->RestorePersistentState(State));
	TestTrue(TEXT("unknown persisted plan produces invalid result"), Subsystem->GetActivePlan().IsInvalid());
	TestEqual(TEXT("unknown persisted id is cleared"), Subsystem->CapturePersistentState().ArmedPlanId, NAME_None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanGenericHandoffIsExplicitTest,
	"Gloamstead.Experience.Plan.GenericHandoffIsExplicit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanGenericHandoffIsExplicitTest::RunTest(const FString& /*Parameters*/)
{
	UExperienceCycleCatalog* Catalog = MakeAuthoredCatalog();
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(Catalog);
	FExperienceCyclePersistentState State;
	// Read the ceiling from the catalog rather than writing it down. The literal 5 here is what made
	// authoring a sixth cycle turn this test red for no reason a reader could act on.
	State.CompletedCycleSlot = Catalog->AuthoredPlans.Num();
	TestTrue(TEXT("post-authored state restores"), Subsystem->RestorePersistentState(State));

	TestFalse(TEXT("no later authored plan is claimed"), Subsystem->EnsureUpcomingPlan());
	const FExperienceCyclePlan& Plan = Subsystem->GetActivePlan();
	TestTrue(TEXT("later slot is an explicit generic handoff"), Plan.IsGenericHandoff());
	TestEqual(TEXT("handoff owns no authored plan id"), Plan.PlanId, NAME_None);
	TestEqual(TEXT("handoff owns no authored warning"), Plan.WarningId, NAME_None);

	// The handoff must be READABLE, not merely explicit. It previously had no production reader at all,
	// so the end of the authored experience surfaced as an unhandled boundary: rest refused forever and a
	// permanently unresponsive Heart. IsExperienceComplete() is what an ending presents from.
	TestTrue(TEXT("the subsystem reports the authored experience as complete"),
		Subsystem->IsExperienceComplete());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperienceSlotCeilingIsAuthoredTest,
	"Gloamstead.Experience.Plan.SlotCeilingFollowsTheAuthoredCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperienceSlotCeilingIsAuthoredTest::RunTest(const FString& /*Parameters*/)
{
	// The ceiling was the literal 4, written in three places. Authoring a fifth cycle would have been
	// silently ignored: the catalog would grow and the experience would still end after the fourth night.
	// A catalog with a fifth authored plan must therefore reach slot 5.
	UExperienceCycleCatalog* Catalog = MakeAuthoredCatalog();
	const int32 AuthoredBefore = Catalog->AuthoredPlans.Num();

	FExperienceCyclePlan Fifth;
	Fifth.Slot = AuthoredBefore + 1;
	Fifth.PlanId = TEXT("Cycle5_Mirror");
	Fifth.WarningId = TEXT("HiddenReflection");
	Fifth.NightType = ENightConsequenceType::Mirror;
	Fifth.SemanticSubject = TEXT("Cycle5_Mirror");
	Fifth.RequiredRestorationTags = { TEXT("MirrorPillar") };
	Fifth.RequiredRitualType = ERitualType::MirrorPillar;
	Fifth.VisualStateKey = TEXT("restoration_level");
	Fifth.OutcomeSummaryKey = TEXT("Cycle5_Mirror");
	Fifth.Resolution = EExperiencePlanResolution::Authored;
	Catalog->AuthoredPlans.Add(Fifth);

	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(Catalog);
	TestEqual(TEXT("the subsystem reports the authored slot count, not a literal"),
		Subsystem->GetAuthoredSlotCount(), AuthoredBefore + 1);

	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = AuthoredBefore;
	TestTrue(TEXT("state after the previously-final cycle restores"), Subsystem->RestorePersistentState(State));

	// Content alone must NOT be able to introduce a night. MatchesRequiredContract has a case per authored
	// slot and `default: return false`, so the asset is transport while C++ stays the authority on what a
	// cycle IS - an edited or forged catalog cannot invent one. The row is therefore refused.
	AddExpectedError(
		TEXT("has no authored row satisfying the canonical contract"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(TEXT("an authored row with no canonical contract is refused"), Subsystem->EnsureUpcomingPlan());
	TestFalse(TEXT("a refused row is not mistaken for the end of the experience"),
		Subsystem->IsExperienceComplete());
	TestEqual(TEXT("the refused slot arms no plan"), Subsystem->GetActivePlan().PlanId, NAME_None);

	// And the genuine end of the experience is still reached once the catalog is exhausted - now measured
	// against the authored count rather than the literal 4 it used to be compared to.
	UGloamsteadExperienceCycleSubsystem* AtEnd = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState EndState;
	EndState.CompletedCycleSlot = AtEnd->GetAuthoredSlotCount();
	TestTrue(TEXT("state at the authored ceiling restores"), AtEnd->RestorePersistentState(EndState));
	TestFalse(TEXT("no plan follows the final authored cycle"), AtEnd->EnsureUpcomingPlan());
	TestTrue(TEXT("exhausting the authored catalog completes the experience"), AtEnd->IsExperienceComplete());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
