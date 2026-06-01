#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/NightConsequenceTypes.h"
#include "VeilHeartWarningTypes.generated.h"

USTRUCT(BlueprintType)
struct FVeilHeartWarningFragment
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	FName WarningId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	FText Fragment;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	ENightConsequenceType AssociatedNightType = ENightConsequenceType::Invalid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	TArray<FName> SatisfiableTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning", meta = (ClampMin = "0"))
	int32 ClarityTier = 0;
};

UCLASS(BlueprintType)
class GLOAMSTEAD_API UVeilHeartWarningCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	TArray<FVeilHeartWarningFragment> Warnings;
};