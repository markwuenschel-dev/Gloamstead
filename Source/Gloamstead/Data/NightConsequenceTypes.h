#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/RitualTypes.h"
#include "NightConsequenceTypes.generated.h"

/** MVP night types aligned with docs/systems/03_night_consequence_system.md */
UENUM(BlueprintType)
enum class ENightConsequenceType : uint8
{
	Invalid            = 0 UMETA(Hidden),
	Tutorial           = 1,
	Corruption         = 2,
	Omen               = 3,
	Retrieval          = 4,
	SilencePossession  = 5,
	Mirror             = 6,
	Bargain            = 7,
	Fracture           = 8,
	TrueSiege          = 9,
};

/** Sanctuary aggregates used when scoring night rules at dusk */
USTRUCT(BlueprintType)
struct FNightSanctuarySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	float AverageLightLevel = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	float AverageCorruptionLevel = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	int32 RestoredPointCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	int32 LanternPostRestored = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	int32 GardenBedRestored = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	int32 PathPointRestored = 0;
};

/** Designer-tunable rule row for catalog scoring */
USTRUCT(BlueprintType)
struct FNightConsequenceRule
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night")
	ENightConsequenceType NightType = ENightConsequenceType::Invalid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night", meta = (ClampMin = "0"))
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float MinAverageLight = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxAverageLight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float MinAverageCorruption = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxAverageCorruption = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night")
	TArray<ERitualType> FavoredRitualTypes;

	/** Used when NightType is Omen; broadcast at night start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night")
	FName OmenClueTag = NAME_None;
};

UCLASS(BlueprintType)
class GLOAMSTEAD_API UNightConsequenceCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night")
	TArray<FNightConsequenceRule> Rules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night")
	ENightConsequenceType FallbackNightType = ENightConsequenceType::Corruption;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night")
	bool bForceTutorialOnFirstNight = true;
};

GLOAMSTEAD_API FString GetNightConsequenceTypeDisplayName(ENightConsequenceType Type);

/** Fills MVP Tutorial / Corruption / Omen rules for dev PIE without a Content asset. */
GLOAMSTEAD_API void PopulateMVPNightConsequenceRules(UNightConsequenceCatalog& Catalog);