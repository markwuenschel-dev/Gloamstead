// Authored Cycle II plan invariants: early slots are contracts, never score-selected narratives.
#include "Misc/AutomationTest.h"
#include "Data/ExperienceCycleTypes.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"

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
		UGloamsteadExperienceCycleSubsystem* Subsystem = NewObject<UGloamsteadExperienceCycleSubsystem>();
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
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperiencePlanRequiredSlotsFailClosedTest,
	"Gloamstead.Experience.Plan.RequiredSlotsFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePlanRequiredSlotsFailClosedTest::RunTest(const FString& /*Parameters*/)
{
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
	Catalog->AuthoredPlans.Add(Catalog->AuthoredPlans[1]);
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
	State.CompletedCycleSlot = 2;
	TestTrue(TEXT("post-authored state restores"), Subsystem->RestorePersistentState(State));

	TestFalse(TEXT("no later authored plan is claimed"), Subsystem->EnsureUpcomingPlan());
	const FExperienceCyclePlan& Plan = Subsystem->GetActivePlan();
	TestTrue(TEXT("later slot is an explicit generic handoff"), Plan.IsGenericHandoff());
	TestEqual(TEXT("handoff owns no authored plan id"), Plan.PlanId, NAME_None);
	TestEqual(TEXT("handoff owns no authored warning"), Plan.WarningId, NAME_None);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
