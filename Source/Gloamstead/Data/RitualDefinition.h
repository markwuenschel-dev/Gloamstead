#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/RitualTypes.h"
#include "RitualDefinition.generated.h"

/**
 * Base class for Ritual Definition Data Assets.
 * Create concrete assets (e.g. DA_Ritual_LanternPost, DA_Ritual_MirrorPillar, DA_Ritual_BellShrine) that inherit from this.
 * 
 * This allows tuning LightContribution, CorruptionClearance, etc. per ritual type
 * without hardcoding values in C++ or Blueprints.
 *
 * New types for Phase 2+:
 * - MirrorPillar: reflects warnings, provides clarity bonuses, vulnerable to certain nights.
 * - BellShrine: calls or repels based on tags, affects night selection radius.
 */
UCLASS(BlueprintType, Abstract)
class GLOAMSTEAD_API URitualDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ritual Definition")
    ERitualType RitualType = ERitualType::Invalid;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ritual Definition")
    float DefaultLightContribution = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ritual Definition")
    float DefaultCorruptionClearance = 0.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ritual Definition")
    float RestorationRadius = 800.0f;

    /** Tags this ritual type can satisfy for Veil Heart warnings */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ritual Definition")
    TArray<FName> SatisfiableWarningTags;
};