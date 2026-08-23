#include "Validation/VeilHeartWarningCatalogValidator.h"

#include "AssetRegistry/AssetData.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Misc/DataValidation.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
	bool GetCanonicalGardenPlan(FExperienceCyclePlan& OutPlan)
	{
		UExperienceCycleCatalog* CanonicalCatalog = NewObject<UExperienceCycleCatalog>(GetTransientPackage());
		PopulateDefaultExperienceCyclePlans(*CanonicalCatalog);
		for (const FExperienceCyclePlan& Plan : CanonicalCatalog->AuthoredPlans)
		{
			if (Plan.PlanId == FName(TEXT("Cycle2_Garden")))
			{
				OutPlan = Plan;
				return true;
			}
		}
		return false;
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

	FExperienceCyclePlan GardenPlan;
	if (!GetCanonicalGardenPlan(GardenPlan))
	{
		AssetFails(InAsset, FText::FromString(TEXT("Canonical Cycle2_Garden plan is unavailable for warning validation.")));
		return EDataValidationResult::Invalid;
	}

	bool bInvalid = false;
	auto Fail = [this, InAsset, &bInvalid](const FString& Message)
	{
		bInvalid = true;
		AssetFails(InAsset, FText::FromString(Message));
	};

	TSet<FName> SeenWarningIds;
	int32 GardenRotCount = 0;
	for (const FVeilHeartWarningFragment& Warning : Catalog->Warnings)
	{
		if (Warning.WarningId == NAME_None)
		{
			Fail(TEXT("Veil Heart warning catalog contains an empty WarningId."));
			continue;
		}
		if (SeenWarningIds.Contains(Warning.WarningId))
		{
			Fail(FString::Printf(TEXT("Veil Heart warning catalog duplicates WarningId '%s'."), *Warning.WarningId.ToString()));
			continue;
		}
		SeenWarningIds.Add(Warning.WarningId);

		if (Warning.WarningId == GardenPlan.WarningId)
		{
			++GardenRotCount;
			FString ContractError;
			if (!Warning.MatchesExactPlanContract(GardenPlan, &ContractError))
			{
				Fail(FString::Printf(TEXT("GardenRot warning contract is invalid: %s."), *ContractError));
			}
		}
	}

	if (GardenRotCount != 1)
	{
		Fail(TEXT("Veil Heart warning catalog must contain exactly one GardenRot warning."));
	}

	if (bInvalid)
	{
		return EDataValidationResult::Invalid;
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadGardenRotValidatorRejectsDuplicateSupportTest,
	"Gloamstead.Editor.Validation.GardenRotRejectsDuplicateSupportFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadGardenRotValidatorRejectsDuplicateSupportTest::RunTest(const FString& /*Parameters*/)
{
	FExperienceCyclePlan GardenPlan;
	if (!TestTrue(TEXT("canonical GardenRot plan is available to the validator"), GetCanonicalGardenPlan(GardenPlan)))
	{
		return false;
	}

	UVeilHeartWarningCatalog* Catalog = NewObject<UVeilHeartWarningCatalog>();
	FVeilHeartWarningFragment GardenWarning;
	GardenWarning.WarningId = GardenPlan.WarningId;
	GardenWarning.Fragment = FText::FromString(TEXT("What grows in darkness must be tended before the bell tolls."));
	GardenWarning.AssociatedNightType = GardenPlan.NightType;
	GardenWarning.SatisfiableTags = GardenPlan.RequiredRestorationTags;
	GardenWarning.SemanticSubject = GardenPlan.SemanticSubject;
	GardenWarning.RequiredRitualType = GardenPlan.RequiredRitualType;
	GardenWarning.InterpretationReceiptId = GardenPlan.InterpretationReceiptId;
	for (int32 Index = 0; Index < GardenPlan.RequiredSupportIds.Num(); ++Index)
	{
		FVeilHeartWarningSupportChannel& Channel = GardenWarning.SupportChannels.AddDefaulted_GetRef();
		Channel.SupportId = GardenPlan.RequiredSupportIds[Index];
		Channel.EvidenceText = FText::FromString(TEXT("Readable evidence."));
		Channel.ChannelType = GardenPlan.RequiredSupportChannelTypes[Index];
	}
	GardenWarning.SupportChannels[1].SupportId = GardenWarning.SupportChannels[0].SupportId;
	Catalog->Warnings.Add(GardenWarning);

	UVeilHeartWarningCatalogValidator* Validator = NewObject<UVeilHeartWarningCatalogValidator>();
	FDataValidationContext Context;
	const EDataValidationResult Result = Validator->ValidateLoadedAsset(FAssetData(Catalog), Catalog, Context);
	TestEqual(TEXT("editor validator rejects duplicate GardenRot supports"), Result, EDataValidationResult::Invalid);
	TestTrue(TEXT("editor validator reports the contract failure"), Context.GetNumErrors() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadGardenRotValidatorRejectsWrongMediumTest,
	"Gloamstead.Editor.Validation.GardenRotRejectsWrongMediumFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadGardenRotValidatorRejectsWrongMediumTest::RunTest(const FString& /*Parameters*/)
{
	FExperienceCyclePlan GardenPlan;
	if (!TestTrue(TEXT("canonical GardenRot plan is available to the validator"), GetCanonicalGardenPlan(GardenPlan)))
	{
		return false;
	}

	UVeilHeartWarningCatalog* Catalog = NewObject<UVeilHeartWarningCatalog>();
	FVeilHeartWarningFragment GardenWarning;
	GardenWarning.WarningId = GardenPlan.WarningId;
	GardenWarning.Fragment = FText::FromString(TEXT("What grows in darkness must be tended before the bell tolls."));
	GardenWarning.AssociatedNightType = GardenPlan.NightType;
	GardenWarning.SatisfiableTags = GardenPlan.RequiredRestorationTags;
	GardenWarning.SemanticSubject = GardenPlan.SemanticSubject;
	GardenWarning.RequiredRitualType = GardenPlan.RequiredRitualType;
	GardenWarning.InterpretationReceiptId = GardenPlan.InterpretationReceiptId;
	for (int32 Index = 0; Index < GardenPlan.RequiredSupportIds.Num(); ++Index)
	{
		FVeilHeartWarningSupportChannel& Channel = GardenWarning.SupportChannels.AddDefaulted_GetRef();
		Channel.SupportId = GardenPlan.RequiredSupportIds[Index];
		Channel.EvidenceText = FText::FromString(TEXT("Readable evidence."));
		Channel.ChannelType = GardenPlan.RequiredSupportChannelTypes[Index];
	}
	GardenWarning.SupportChannels[2].ChannelType = TEXT("Environmental");
	Catalog->Warnings.Add(GardenWarning);

	UVeilHeartWarningCatalogValidator* Validator = NewObject<UVeilHeartWarningCatalogValidator>();
	FDataValidationContext Context;
	const EDataValidationResult Result = Validator->ValidateLoadedAsset(FAssetData(Catalog), Catalog, Context);
	TestEqual(TEXT("editor validator rejects a wrong-medium GardenRot support"), Result, EDataValidationResult::Invalid);
	TestTrue(TEXT("editor validator reports the wrong-medium contract failure"), Context.GetNumErrors() > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
