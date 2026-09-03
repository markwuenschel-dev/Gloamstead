// The shipped experience catalog must exist, be well-formed, and agree with the in-code fallback.
//
// UGloamsteadExperienceCycleSubsystem::EnsureCatalog loads /Game/Data/DA_ExperienceCycleCatalog and only
// falls back to PopulateDefaultExperienceCyclePlans when that asset is absent. Both produce a catalog the
// rest of the game treats identically, so nothing at runtime reveals which one won - which is exactly how
// code-as-production-content survives unnoticed.
//
// This test pins three things the game depends on and no other test covers:
//   1. the authored asset SHIPS at all (its absence is a silent downgrade to the dev fallback);
//   2. its slots are unique and contiguous from 1, because EnsureUpcomingPlan walks CompletedCycleSlot + 1
//      and a gap ends the experience at the gap rather than at the last authored cycle;
//   3. it does not DRIFT from the in-code plans, so the fallback stays a faithful development stand-in
//      rather than a second, quietly different definition of the game.
#include "Misc/AutomationTest.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamShippedExperienceCatalogTest,
	"Gloamstead.Experience.Plan.ShippedCatalogIsAuthoredAndMatchesFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamShippedExperienceCatalogTest::RunTest(const FString& /*Parameters*/)
{
	UExperienceCycleCatalog* Shipped = Cast<UExperienceCycleCatalog>(StaticLoadObject(
		UExperienceCycleCatalog::StaticClass(), nullptr,
		TEXT("/Game/Data/DA_ExperienceCycleCatalog.DA_ExperienceCycleCatalog")));

	if (!Shipped)
	{
		AddError(TEXT("/Game/Data/DA_ExperienceCycleCatalog did not load. The game would silently fall back to the in-code development plans. Author them in specs/data/vs-polish-starter.json and re-import with agent_collab/scripts/Invoke-GloamsteadDataAssetImport.ps1."));
		return false;
	}

	UExperienceCycleCatalog* Fallback = NewObject<UExperienceCycleCatalog>();
	PopulateDefaultExperienceCyclePlans(*Fallback);

	TestEqual(
		TEXT("the shipped catalog has the same number of authored plans as the in-code fallback"),
		Shipped->AuthoredPlans.Num(),
		Fallback->AuthoredPlans.Num());

	// Slot integrity is independent of the fallback: even a catalog that matched a broken fallback
	// exactly would still soft-lock the experience at the first gap.
	for (int32 Index = 0; Index < Shipped->AuthoredPlans.Num(); ++Index)
	{
		const FExperienceCyclePlan& Plan = Shipped->AuthoredPlans[Index];
		TestEqual(
			*FString::Printf(TEXT("shipped plan %d (%s) occupies slot %d"), Index, *Plan.PlanId.ToString(), Index + 1),
			Plan.Slot,
			Index + 1);
		TestTrue(
			*FString::Printf(TEXT("shipped plan %s is an authored plan"), *Plan.PlanId.ToString()),
			Plan.IsAuthoredPlan());
	}

	for (const FExperienceCyclePlan& Expected : Fallback->AuthoredPlans)
	{
		const FExperienceCyclePlan* Actual = Shipped->AuthoredPlans.FindByPredicate(
			[&Expected](const FExperienceCyclePlan& Candidate) { return Candidate.Slot == Expected.Slot; });

		if (!Actual)
		{
			AddError(FString::Printf(
				TEXT("the shipped catalog has no plan in slot %d; the in-code fallback defines %s there"),
				Expected.Slot, *Expected.PlanId.ToString()));
			continue;
		}

		const FString Where = FString::Printf(TEXT("slot %d (%s)"), Expected.Slot, *Expected.PlanId.ToString());
		TestEqual(*FString::Printf(TEXT("%s PlanId"), *Where), Actual->PlanId, Expected.PlanId);
		TestEqual(*FString::Printf(TEXT("%s WarningId"), *Where), Actual->WarningId, Expected.WarningId);
		TestEqual(
			*FString::Printf(TEXT("%s NightType"), *Where),
			GetNightConsequenceTypeDisplayName(Actual->NightType),
			GetNightConsequenceTypeDisplayName(Expected.NightType));
		TestEqual(*FString::Printf(TEXT("%s SemanticSubject"), *Where), Actual->SemanticSubject, Expected.SemanticSubject);
		TestEqual(*FString::Printf(TEXT("%s RequiredRitualType"), *Where),
			static_cast<int32>(Actual->RequiredRitualType), static_cast<int32>(Expected.RequiredRitualType));
		TestEqual(*FString::Printf(TEXT("%s RequiredRestorationTags count"), *Where),
			Actual->RequiredRestorationTags.Num(), Expected.RequiredRestorationTags.Num());
		TestEqual(*FString::Printf(TEXT("%s RequiredSupportIds count"), *Where),
			Actual->RequiredSupportIds.Num(), Expected.RequiredSupportIds.Num());
		TestEqual(*FString::Printf(TEXT("%s RequiredSupportChannelTypes count"), *Where),
			Actual->RequiredSupportChannelTypes.Num(), Expected.RequiredSupportChannelTypes.Num());
		TestEqual(*FString::Printf(TEXT("%s MinimumDistinctSupportCount"), *Where),
			Actual->MinimumDistinctSupportCount, Expected.MinimumDistinctSupportCount);
		TestEqual(*FString::Printf(TEXT("%s InterpretationReceiptId"), *Where),
			Actual->InterpretationReceiptId, Expected.InterpretationReceiptId);
		TestEqual(*FString::Printf(TEXT("%s OutcomeSummaryKey"), *Where),
			Actual->OutcomeSummaryKey, Expected.OutcomeSummaryKey);

		// The second clause is content too, and it is content the night acts on. A shipped plan whose
		// readings had drifted from the code would let a player commit a reading the runtime grades
		// differently - the single worst failure this mechanic can have.
		TestEqual(*FString::Printf(TEXT("%s SecondReadings count"), *Where),
			Actual->SecondReadings.Num(), Expected.SecondReadings.Num());
		for (const FExperienceCycleSecondReading& ExpectedReading : Expected.SecondReadings)
		{
			const FExperienceCycleSecondReading* ActualReading = Actual->FindSecondReading(ExpectedReading.ReadingId);
			if (!ActualReading)
			{
				AddError(FString::Printf(TEXT("%s is missing second reading %s"),
					*Where, *ExpectedReading.ReadingId.ToString()));
				continue;
			}
			TestEqual(*FString::Printf(TEXT("%s %s grade"), *Where, *ExpectedReading.ReadingId.ToString()),
				GetExperienceReadingGradeDisplayName(ActualReading->Grade),
				GetExperienceReadingGradeDisplayName(ExpectedReading.Grade));
			TestEqual(*FString::Printf(TEXT("%s %s consequence tag"), *Where, *ExpectedReading.ReadingId.ToString()),
				ActualReading->ConsequenceTag, ExpectedReading.ConsequenceTag);
			TestEqual(*FString::Printf(TEXT("%s %s choice prompt"), *Where, *ExpectedReading.ReadingId.ToString()),
				ActualReading->ChoicePrompt.ToString(), ExpectedReading.ChoicePrompt.ToString());
			TestEqual(*FString::Printf(TEXT("%s %s outcome summary"), *Where, *ExpectedReading.ReadingId.ToString()),
				ActualReading->OutcomeSummary.ToString(), ExpectedReading.OutcomeSummary.ToString());
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
