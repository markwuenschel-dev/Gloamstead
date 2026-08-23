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
	bool GetCanonicalGardenPlan(
		FExperienceCyclePlan& OutPlan,
		ENightConsequenceType DesiredNightType = ENightConsequenceType::Corruption)
	{
		UExperienceCycleCatalog* CanonicalCatalog = NewObject<UExperienceCycleCatalog>(GetTransientPackage());
		PopulateDefaultExperienceCyclePlans(*CanonicalCatalog);
		for (const FExperienceCyclePlan& Plan : CanonicalCatalog->AuthoredPlans)
		{
			if (Plan.WarningId == FName(TEXT("GardenRot")) && Plan.NightType == DesiredNightType)
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

	bool bInvalid = false;
	auto Fail = [this, InAsset, &bInvalid](const FString& Message)
	{
		bInvalid = true;
		AssetFails(InAsset, FText::FromString(Message));
	};

	TSet<FString> SeenWarningKeys;
	bool bFoundCorruptionGarden = false;
	bool bFoundRetrievalGarden = false;
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

		if (Warning.WarningId == FName(TEXT("GardenRot")))
		{
			FExperienceCyclePlan GardenPlan;
			if (!GetCanonicalGardenPlan(GardenPlan, Warning.AssociatedNightType))
			{
				Fail(FString::Printf(TEXT("Canonical GardenRot plan is unavailable for night type %d."),
					static_cast<int32>(Warning.AssociatedNightType)));
				continue;
			}
			FString ContractError;
			if (!Warning.MatchesExactPlanContract(GardenPlan, &ContractError))
			{
				Fail(FString::Printf(TEXT("GardenRot warning contract is invalid: %s."), *ContractError));
			}
			if (Warning.AssociatedNightType == ENightConsequenceType::Corruption)
			{
				bFoundCorruptionGarden = true;
			}
			else if (Warning.AssociatedNightType == ENightConsequenceType::Retrieval)
			{
				bFoundRetrievalGarden = true;
			}
		}
	}

	if (!bFoundCorruptionGarden || !bFoundRetrievalGarden)
	{
		Fail(TEXT("Veil Heart warning catalog must contain canonical Corruption and Retrieval GardenRot warnings."));
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
