#include "Data/GloamsteadRitualSiteCatalog.h"

bool FGloamsteadRitualSiteDeclaration::IsCompleteDeclaration(TArray<FString>& OutProblems) const
{
	OutProblems.Reset();

	const FString Where = SemanticSubject.IsNone()
		? FString(TEXT("<unnamed site>"))
		: SemanticSubject.ToString();

	if (SemanticSubject.IsNone())
	{
		OutProblems.Add(TEXT("an authored ritual site declares no SemanticSubject - an anonymous site cannot be a night's target"));
	}
	if (RecommendedForWarning.IsNone())
	{
		OutProblems.Add(FString::Printf(
			TEXT("ritual site '%s' declares no RecommendedForWarning - PointMatchesExperiencePlan would refuse it"), *Where));
	}
	if (RitualType == ERitualType::Invalid)
	{
		OutProblems.Add(FString::Printf(
			TEXT("ritual site '%s' declares no RitualType"), *Where));
	}
	if (RestorationTag.IsNone())
	{
		OutProblems.Add(FString::Printf(
			TEXT("ritual site '%s' declares no RestorationTag - the authored plan requires exactly one"), *Where));
	}
	if (MinimumAnchorDistance >= BindRadius)
	{
		OutProblems.Add(FString::Printf(
			TEXT("ritual site '%s' has MinimumAnchorDistance (%.0f) >= BindRadius (%.0f), so no point can ever satisfy it"),
			*Where, MinimumAnchorDistance, BindRadius));
	}

	return OutProblems.Num() == 0;
}
