#pragma once

#include "CoreMinimal.h"
#include "ExperienceCycleTypes.generated.h"

/**
 * Durable authored-cycle state owned by the save payload.  It contains only
 * serializable facts; the experience-cycle subsystem will interpret it later.
 */
USTRUCT(BlueprintType)
struct GLOAMSTEAD_API FExperienceCyclePersistentState
{
    GENERATED_BODY()

    /** The last authored cycle known to have completed. Zero means no completed authored cycle is proven. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
    int32 CompletedCycleSlot = 0;

    /** Stable identifier of the plan armed before the next rest. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
    FName ArmedPlanId = NAME_None;

    /** Stable identifier of the most recently executed plan. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
    FName LastPlanId = NAME_None;

    /** Result tag from the most recently completed authored plan. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
    FName LastOutcomeResultTag = NAME_None;

    /** Durable failure/aftermath markers for later authored cycles. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
    TArray<FName> ScarTags;

    /** Whether the player has already consumed the tutorial's first rest. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
    bool bFirstRestCompleted = false;

    /** Persisted day/night phase enum ordinal; INDEX_NONE means no phase is safely known. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
    int32 SavedPhaseOrdinal = INDEX_NONE;

    /** True when a v1 payload needs explicit runtime reconciliation before authoring a new plan. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
    bool bRequiresLegacyReconciliation = false;

    /** Clear data that a legacy payload cannot establish and require an explicit reconciliation. */
    void ResetForLegacyReconciliation();
};
