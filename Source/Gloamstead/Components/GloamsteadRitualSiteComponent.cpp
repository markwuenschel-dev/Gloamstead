#include "Components/GloamsteadRitualSiteComponent.h"

#include "GameFramework/Actor.h"

UGloamsteadRitualSiteComponent::UGloamsteadRitualSiteComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UGloamsteadRitualSiteComponent::IsCompleteDeclaration(TArray<FString>& OutProblems) const
{
	OutProblems.Reset();

	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("<no owner>");

	if (SemanticSubject.IsNone())
	{
		OutProblems.Add(FString::Printf(
			TEXT("ritual site on '%s' declares no SemanticSubject - an anonymous site cannot be a night's target"),
			*OwnerName));
	}
	if (RecommendedForWarning.IsNone())
	{
		OutProblems.Add(FString::Printf(
			TEXT("ritual site '%s' on '%s' declares no RecommendedForWarning - PointMatchesExperiencePlan would refuse it"),
			*SemanticSubject.ToString(), *OwnerName));
	}
	if (RitualType == ERitualType::Invalid)
	{
		OutProblems.Add(FString::Printf(
			TEXT("ritual site '%s' on '%s' declares no RitualType - there is no way to tell which generated points are eligible"),
			*SemanticSubject.ToString(), *OwnerName));
	}
	if (RestorationTag.IsNone())
	{
		OutProblems.Add(FString::Printf(
			TEXT("ritual site '%s' on '%s' declares no RestorationTag - the authored plan requires exactly one"),
			*SemanticSubject.ToString(), *OwnerName));
	}

	return OutProblems.Num() == 0;
}
