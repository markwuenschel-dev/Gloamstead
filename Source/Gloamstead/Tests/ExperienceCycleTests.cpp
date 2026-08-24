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
	FGloamExperiencePlanSlotThreeIsExactRetrievalTest,
	"Gloamstead.Experience.Plan.SlotThreeIsExactRetrieval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanSlotThreeIsExactRetrievalTest::RunTest(const FString& /*Parameters*/)
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
	TestEqual(TEXT("slot three reuses the garden warning identity"), Plan.WarningId, FName(TEXT("GardenRot")));
	TestEqual(TEXT("slot three binds to the restored garden subject"), Plan.SemanticSubject, FName(TEXT("Cycle2_Garden")));
	TestEqual(TEXT("slot three keeps its stable id"), Plan.PlanId, FName(TEXT("Cycle3_Retrieval")));
	TestEqual(TEXT("slot three requires the canonical GardenBed ritual"), Plan.RequiredRitualType, ERitualType::GardenBed);
	TestEqual(TEXT("slot three requires two distinct readable supports"), Plan.MinimumDistinctSupportCount, 2);
	TestEqual(TEXT("slot three records a retrieval-specific interpretation receipt"), Plan.InterpretationReceiptId, FName(TEXT("GardenRot.Retrieved")));
	TestEqual(TEXT("slot three keeps the garden support set"), Plan.RequiredSupportIds.Num(), 3);
	TestTrue(TEXT("slot three retains the environmental vines clue"), Plan.RequiredSupportIds.Contains(FName(TEXT("GardenRot.WitheredVines"))));
	TestTrue(TEXT("slot three retains the object-reaction soil clue"), Plan.RequiredSupportIds.Contains(FName(TEXT("GardenRot.ColdSoil"))));
	TestTrue(TEXT("slot three retains the audio moth clue"), Plan.RequiredSupportIds.Contains(FName(TEXT("GardenRot.BellMoths"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanSlotFourIsExactPossessionTest,
	"Gloamstead.Experience.Plan.SlotFourIsExactPossession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanSlotFourIsExactPossessionTest::RunTest(const FString& /*Parameters*/)
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
	TestEqual(TEXT("slot four re-reads the garden warning"), Plan.WarningId, FName(TEXT("GardenRot")));
	TestEqual(TEXT("slot four binds to the restored garden subject"), Plan.SemanticSubject, FName(TEXT("Cycle2_Garden")));
	TestEqual(TEXT("slot four keeps its stable id"), Plan.PlanId, FName(TEXT("Cycle4_Possession")));
	TestEqual(TEXT("slot four requires the canonical GardenBed ritual"), Plan.RequiredRitualType, ERitualType::GardenBed);
	TestEqual(TEXT("slot four requires two readable supports"), Plan.MinimumDistinctSupportCount, 2);
	TestEqual(TEXT("slot four records a possession-specific receipt"), Plan.InterpretationReceiptId, FName(TEXT("GardenRot.Possessed")));
	TestEqual(TEXT("slot four keeps the garden support set"), Plan.RequiredSupportIds.Num(), 3);
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
	UGloamsteadExperienceCycleSubsystem* Subsystem = MakeSubsystem(MakeAuthoredCatalog());
	FExperienceCyclePersistentState State;
	State.CompletedCycleSlot = 4;
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
