#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RitualTypes.generated.h"

/**
 * Ritual Type Enum - Phase 1 scope locked.
 * Only LanternPost, GardenBed, and PathPoint are active in Phase 1.
 */
UENUM(BlueprintType)
enum class ERitualType : uint8
{
    Invalid         = 0,
    LanternPost     = 1,
    GardenBed       = 2,
    PathPoint       = 3,
    // MirrorPillar and BellShrine are deferred to Phase 2
};

/**
 * Event payload broadcast when a player successfully restores a ritual point.
 * This is the primary communication contract between placement, the PCG Subsystem,
 * the Veil Heart, Night Consequence systems, and VFX.
 */
USTRUCT(BlueprintType)
struct FRestorationEventPayload
{
    GENERATED_BODY()

    /** The type of ritual that was restored */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    ERitualType RitualType = ERitualType::Invalid;

    /** World location of the restoration */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    FVector WorldLocation = FVector::ZeroVector;

    /** Path segment this point belongs to (used for light propagation) */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    int32 PathSegmentID = -1;

    /** Normalized position (0-1) along the path segment */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    float PathPosition = 0.0f;

    /** Reference to the spawned restored actor (if any) */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    TWeakObjectPtr<AActor> RestoredActor;

    /** Amount of light contributed by this restoration */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    float LightDelta = 0.0f;

    /** Amount of corruption cleared by this restoration */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    float CorruptionCleared = 0.0f;

    /** The warning tag this restoration satisfies (used by Veil Heart) */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    FName WarningTagSatisfied;

    /** Unique ID for this restoration instance (useful for tracking) */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    int32 RestorationInstanceID = 0;

    /** Which night this restoration occurred on */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    int32 NightCountAtRestoration = 0;

    /** Time of day when restoration happened (0 = dawn, 1 = dusk) */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    float TimeOfDayAtRestoration = 0.0f;

    /** 
     * The index of the ritual point that was restored inside the PCG subsystem.
     * This enables direct, high-performance access via the optimized getters
     * (GetLightLevel, GetCorruptionLevel, IsPointRestored, etc.).
     * 
     * Value is -1 if the index was not available at the time of restoration.
     */
    UPROPERTY(BlueprintReadOnly, Category="Restoration")
    int32 PointIndex = -1;
};

/**
 * Lightweight base Data Asset for ritual definitions.
 * Used for tuning values without hardcoding them in C++ or Blueprints.
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

// Helper functions (implemented in .cpp)
GLOAMSTEAD_API FString GetRitualTypeDisplayName(ERitualType Type);
GLOAMSTEAD_API bool IsDirectlyRestorable(ERitualType Type);
GLOAMSTEAD_API float GetDefaultLightContribution(ERitualType Type);
GLOAMSTEAD_API float GetDefaultCorruptionClearance(ERitualType Type);