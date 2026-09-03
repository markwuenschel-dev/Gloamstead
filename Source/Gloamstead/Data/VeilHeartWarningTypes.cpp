#include "Data/VeilHeartWarningTypes.h"

#include "Data/NightConsequenceTypes.h"

bool ValidateWarningCatalogCoversAuthoredPlans(
	const UVeilHeartWarningCatalog& Catalog,
	TArray<FString>& OutProblems)
{
	OutProblems.Reset();

	// The authored plans are the contract's demand side. Item 5 of the production goal replaces this
	// code-populated source with an authored DA_ExperienceCycleCatalog; when it does, this validator
	// should read the same asset the runtime reads so both sides of the contract are authored content.
	UExperienceCycleCatalog* Plans = NewObject<UExperienceCycleCatalog>();
	PopulateDefaultExperienceCyclePlans(*Plans);

	if (Plans->AuthoredPlans.Num() == 0)
	{
		OutProblems.Add(TEXT("there are no authored experience-cycle plans to satisfy - the experience catalog is empty"));
		return false;
	}

	for (const FExperienceCyclePlan& Plan : Plans->AuthoredPlans)
	{
		if (!Plan.IsAuthoredPlan())
		{
			continue;
		}

		int32 MatchCount = 0;
		for (const FVeilHeartWarningFragment& Warning : Catalog.Warnings)
		{
			if (Warning.WarningId == Plan.WarningId && Warning.AssociatedNightType == Plan.NightType)
			{
				++MatchCount;
			}
		}

		if (MatchCount == 1)
		{
			continue;
		}

		const FString Pair = FString::Printf(
			TEXT("%s/%s"),
			*Plan.WarningId.ToString(),
			*GetNightConsequenceTypeDisplayName(Plan.NightType));

		if (MatchCount == 0)
		{
			OutProblems.Add(FString::Printf(
				TEXT("slot %d (%s) has NO warning row for %s - that night can never present its warning, so it can never begin"),
				Plan.Slot,
				*Plan.PlanId.ToString(),
				*Pair));
		}
		else
		{
			OutProblems.Add(FString::Printf(
				TEXT("slot %d (%s) has %d warning rows for %s - an ambiguous match is refused exactly like a missing one"),
				Plan.Slot,
				*Plan.PlanId.ToString(),
				MatchCount,
				*Pair));
		}
	}

	return OutProblems.Num() == 0;
}
