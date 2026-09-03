#include "Validation/VeilHeartWarningCatalogValidator.h"

#include "AssetRegistry/AssetData.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Misc/DataValidation.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
	/**
	 * Finds the canonical authored plan a warning row claims to answer.
	 *
	 * This used to look only for GardenRot, which meant every warning authored after Cycle II was
	 * validated by nothing at all: the identity was the gate, so a new night's row could ship with
	 * sparse evidence, a wrong medium, or a half-authored reading set and the editor would pass it.
	 * Matching on (WarningId, NightType) makes the check cover whatever the catalog currently
	 * authors, including cycles that do not exist yet.
	 */
	bool FindCanonicalPlanForWarning(
		FName WarningId,
		ENightConsequenceType NightType,
		FExperienceCyclePlan& OutPlan)
	{
		UExperienceCycleCatalog* CanonicalCatalog = NewObject<UExperienceCycleCatalog>(GetTransientPackage());
		PopulateDefaultExperienceCyclePlans(*CanonicalCatalog);
		for (const FExperienceCyclePlan& Plan : CanonicalCatalog->AuthoredPlans)
		{
			if (Plan.IsAuthoredPlan() && Plan.WarningId == WarningId && Plan.NightType == NightType)
			{
				OutPlan = Plan;
				return true;
			}
		}
		return false;
	}

	/** The one exemption: the opening tutorial predates the fair-crypticism evidence contract. */
	bool IsTutorialExemptPlan(const FExperienceCyclePlan& Plan)
	{
		return Plan.NightType == ENightConsequenceType::Tutorial
			&& Plan.WarningId == FName(TEXT("TutorialLostPath"));
	}
}

bool UVeilHeartWarningCatalogValidator::CanValidateAsset_Implementation(
	const FAssetData& /*InAssetData*/,
	UObject* InAsset,
	FDataValidationContext& /*InContext*/) const
{
	return InAsset && InAsset->IsA<UVeilHeartWarningCatalog>();
}

EDataValidationResult UVeilHeartWarningCatalogValidator::ValidateLoadedAsset_Implementation(
	const FAssetData& /*InAssetData*/,
	UObject* InAsset,
	FDataValidationContext& /*InContext*/)
{
	const UVeilHeartWarningCatalog* Catalog = Cast<UVeilHeartWarningCatalog>(InAsset);
	if (!Catalog)
	{
		return EDataValidationResult::NotValidated;
	}

	bool bInvalid = false;
	auto Fail = [this, InAsset, &bInvalid](const FString& Message)
	{
		bInvalid = true;
		AssetFails(InAsset, FText::FromString(Message));
	};

	TSet<FString> SeenWarningKeys;
	TSet<FName> AnsweredPlanIds;
	for (const FVeilHeartWarningFragment& Warning : Catalog->Warnings)
	{
		if (Warning.WarningId == NAME_None)
		{
			Fail(TEXT("Veil Heart warning catalog contains an empty WarningId."));
			continue;
		}
		const FString WarningKey = FString::Printf(TEXT("%s|%d"),
			*Warning.WarningId.ToString(), static_cast<int32>(Warning.AssociatedNightType));
		if (SeenWarningKeys.Contains(WarningKey))
		{
			Fail(FString::Printf(TEXT("Veil Heart warning catalog duplicates warning identity '%s' for night type %d."),
				*Warning.WarningId.ToString(), static_cast<int32>(Warning.AssociatedNightType)));
			continue;
		}
		SeenWarningKeys.Add(WarningKey);

		// A row that answers no authored plan is allowed: the catalog may hold flavour warnings for
		// night types the experience does not currently author. A row that DOES claim an authored
		// plan is held to that plan's full contract.
		FExperienceCyclePlan AnsweredPlan;
		if (!FindCanonicalPlanForWarning(Warning.WarningId, Warning.AssociatedNightType, AnsweredPlan))
		{
			continue;
		}

		AnsweredPlanIds.Add(AnsweredPlan.PlanId);

		if (IsTutorialExemptPlan(AnsweredPlan))
		{
			continue;
		}

		FString ContractError;
		if (!Warning.MatchesExactPlanContract(AnsweredPlan, &ContractError))
		{
			Fail(FString::Printf(TEXT("Warning '%s' for %s does not satisfy plan %s: %s."),
				*Warning.WarningId.ToString(),
				*GetNightConsequenceTypeDisplayName(Warning.AssociatedNightType),
				*AnsweredPlan.PlanId.ToString(),
				*ContractError));
		}
	}

	// Coverage, in the plan -> warning direction. A plan with no row can never present its warning,
	// which means rest is refused and that night can never begin - so it is a catalog defect even
	// though nothing in the catalog itself looks wrong.
	{
		UExperienceCycleCatalog* CanonicalCatalog = NewObject<UExperienceCycleCatalog>(GetTransientPackage());
		PopulateDefaultExperienceCyclePlans(*CanonicalCatalog);
		for (const FExperienceCyclePlan& Plan : CanonicalCatalog->AuthoredPlans)
		{
			if (Plan.IsAuthoredPlan() && !AnsweredPlanIds.Contains(Plan.PlanId))
			{
				Fail(FString::Printf(
					TEXT("Veil Heart warning catalog has no row for plan %s (%s / %s), so that night can never begin."),
					*Plan.PlanId.ToString(),
					*Plan.WarningId.ToString(),
					*GetNightConsequenceTypeDisplayName(Plan.NightType)));
			}
		}
	}

	if (bInvalid)
	{
		return EDataValidationResult::Invalid;
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Builds a catalog that satisfies every authored plan, so a test can break exactly one thing.
	 *
	 * The old fixtures built a single GardenRot row and asserted "invalid" - which stayed green after
	 * the validator gained plan coverage, because a one-row catalog is now invalid for a completely
	 * different reason than the one the test names. Starting from a valid catalog is what keeps these
	 * tests about the defect they claim to be about.
	 */
	UVeilHeartWarningCatalog* BuildFullyValidCatalog(TArray<FExperienceCyclePlan>& OutPlans)
	{
		UExperienceCycleCatalog* CanonicalCatalog = NewObject<UExperienceCycleCatalog>(GetTransientPackage());
		PopulateDefaultExperienceCyclePlans(*CanonicalCatalog);
		OutPlans = CanonicalCatalog->AuthoredPlans;

		UVeilHeartWarningCatalog* Catalog = NewObject<UVeilHeartWarningCatalog>();
		for (const FExperienceCyclePlan& Plan : OutPlans)
		{
			FVeilHeartWarningFragment Warning;
			Warning.WarningId = Plan.WarningId;
			Warning.Fragment = FText::FromString(TEXT("A short sensory fragment."));
			Warning.AssociatedNightType = Plan.NightType;
			Warning.SatisfiableTags = Plan.RequiredRestorationTags;
			Warning.SemanticSubject = Plan.SemanticSubject;
			Warning.RequiredRitualType = Plan.RequiredRitualType;
			Warning.InterpretationReceiptId = Plan.InterpretationReceiptId;
			for (int32 Index = 0; Index < Plan.RequiredSupportIds.Num(); ++Index)
			{
				FVeilHeartWarningSupportChannel& Channel = Warning.SupportChannels.AddDefaulted_GetRef();
				Channel.SupportId = Plan.RequiredSupportIds[Index];
				Channel.EvidenceText = FText::FromString(TEXT("Readable evidence."));
				Channel.ChannelType = Plan.RequiredSupportChannelTypes[Index];
			}
			Catalog->Warnings.Add(MoveTemp(Warning));
		}
		return Catalog;
	}

	/** Index of the first row carrying real support channels, i.e. the first non-tutorial row. */
	int32 FindFirstEvidenceBackedRow(const UVeilHeartWarningCatalog& Catalog)
	{
		for (int32 Index = 0; Index < Catalog.Warnings.Num(); ++Index)
		{
			if (Catalog.Warnings[Index].SupportChannels.Num() >= 3)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	EDataValidationResult RunValidator(UVeilHeartWarningCatalog* Catalog, int32& OutErrorCount)
	{
		UVeilHeartWarningCatalogValidator* Validator = NewObject<UVeilHeartWarningCatalogValidator>();
		FDataValidationContext Context;
		const EDataValidationResult Result = Validator->ValidateLoadedAsset(FAssetData(Catalog), Catalog, Context);
		OutErrorCount = Context.GetNumErrors();
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadWarningValidatorAcceptsCompleteCatalogTest,
	"Gloamstead.Editor.Validation.AcceptsACatalogCoveringEveryAuthoredPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadWarningValidatorAcceptsCompleteCatalogTest::RunTest(const FString& /*Parameters*/)
{
	TArray<FExperienceCyclePlan> Plans;
	UVeilHeartWarningCatalog* Catalog = BuildFullyValidCatalog(Plans);

	int32 ErrorCount = 0;
	const EDataValidationResult Result = RunValidator(Catalog, ErrorCount);
	TestEqual(TEXT("a catalog covering every authored plan validates"), Result, EDataValidationResult::Valid);
	TestEqual(TEXT("a complete catalog reports no defects"), ErrorCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadWarningValidatorRejectsMissingPlanRowTest,
	"Gloamstead.Editor.Validation.RejectsACatalogMissingAnAuthoredPlanRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadWarningValidatorRejectsMissingPlanRowTest::RunTest(const FString& /*Parameters*/)
{
	TArray<FExperienceCyclePlan> Plans;
	UVeilHeartWarningCatalog* Catalog = BuildFullyValidCatalog(Plans);
	if (!TestTrue(TEXT("the authored catalog has rows to remove"), Catalog->Warnings.Num() > 1))
	{
		return false;
	}

	// A plan with no warning row can never present, which means rest is refused and that night can
	// never begin. Nothing in the catalog looks wrong; only coverage catches it.
	Catalog->Warnings.RemoveAt(Catalog->Warnings.Num() - 1);

	int32 ErrorCount = 0;
	const EDataValidationResult Result = RunValidator(Catalog, ErrorCount);
	TestEqual(TEXT("a catalog missing an authored plan row is rejected"), Result, EDataValidationResult::Invalid);
	TestTrue(TEXT("the validator names the uncovered plan"), ErrorCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadWarningValidatorRejectsDuplicateSupportTest,
	"Gloamstead.Editor.Validation.RejectsDuplicateSupportChannels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadWarningValidatorRejectsDuplicateSupportTest::RunTest(const FString& /*Parameters*/)
{
	TArray<FExperienceCyclePlan> Plans;
	UVeilHeartWarningCatalog* Catalog = BuildFullyValidCatalog(Plans);
	const int32 RowIndex = FindFirstEvidenceBackedRow(*Catalog);
	if (!TestTrue(TEXT("the catalog has an evidence-backed row to corrupt"), RowIndex != INDEX_NONE))
	{
		return false;
	}

	Catalog->Warnings[RowIndex].SupportChannels[1].SupportId =
		Catalog->Warnings[RowIndex].SupportChannels[0].SupportId;

	int32 ErrorCount = 0;
	const EDataValidationResult Result = RunValidator(Catalog, ErrorCount);
	TestEqual(TEXT("editor validator rejects duplicate supports"), Result, EDataValidationResult::Invalid);
	TestTrue(TEXT("editor validator reports the contract failure"), ErrorCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadWarningValidatorRejectsWrongMediumTest,
	"Gloamstead.Editor.Validation.RejectsAWrongMediumSupportChannel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadWarningValidatorRejectsWrongMediumTest::RunTest(const FString& /*Parameters*/)
{
	TArray<FExperienceCyclePlan> Plans;
	UVeilHeartWarningCatalog* Catalog = BuildFullyValidCatalog(Plans);
	const int32 RowIndex = FindFirstEvidenceBackedRow(*Catalog);
	if (!TestTrue(TEXT("the catalog has an evidence-backed row to corrupt"), RowIndex != INDEX_NONE))
	{
		return false;
	}

	// Index 2 is authored as Audio for every evidence-backed plan; retyping it as Environmental
	// leaves three channels but only two distinct media, which is the failure fair crypticism cares
	// about - a clue the player cannot find a second, different way.
	Catalog->Warnings[RowIndex].SupportChannels[2].ChannelType = TEXT("Environmental");

	int32 ErrorCount = 0;
	const EDataValidationResult Result = RunValidator(Catalog, ErrorCount);
	TestEqual(TEXT("editor validator rejects a wrong-medium support"), Result, EDataValidationResult::Invalid);
	TestTrue(TEXT("editor validator reports the wrong-medium contract failure"), ErrorCount > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
