#pragma once

#include "CoreMinimal.h"
#include "Data/NightConsequenceTypes.h"
#include "Engine/DataAsset.h"
#include "ExperienceCycleTypes.generated.h"

/** How the cycle subsystem resolved its active result. */
UENUM(BlueprintType)
enum class EExperiencePlanResolution : uint8
{
	Invalid UMETA(Hidden),
	Authored,
	/** The authored catalog has ended; a later owner may choose a generic consequence. */
	GenericHandoff,
};

/**
 * Immutable semantic contract for one upcoming experience beat. This contains
 * authored identity only; it does not choose an outcome or a generic target.
 */
USTRUCT(BlueprintType)
struct GLOAMSTEAD_API FExperienceCyclePlan
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	int32 Slot = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	FName PlanId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	FName WarningId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	ENightConsequenceType NightType = ENightConsequenceType::Invalid;

	/** Stable Gloamstead-owned subject identity, never a score-selected point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	FName SemanticSubject = NAME_None;

	/** Tags required for a later restoration evaluator to interpret this plan. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	TArray<FName> RequiredRestorationTags;

	/** Generic world-state key which an external generator may mirror but never author. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	FName VisualStateKey = NAME_None;

	/** Stable key for the later player-facing dawn summary. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	FName OutcomeSummaryKey = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	EExperiencePlanResolution Resolution = EExperiencePlanResolution::Invalid;

	bool IsAuthoredPlan() const { return Resolution == EExperiencePlanResolution::Authored; }
	bool IsGenericHandoff() const { return Resolution == EExperiencePlanResolution::GenericHandoff; }
	bool IsInvalid() const { return Resolution == EExperiencePlanResolution::Invalid; }

	static FExperienceCyclePlan MakeInvalid(int32 InSlot);
	static FExperienceCyclePlan MakeGenericHandoff(int32 InSlot);
};

/** Designer-facing catalog for the explicitly authored opening sequence. */
UCLASS(BlueprintType)
class GLOAMSTEAD_API UExperienceCycleCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Exactly the currently authorized authored rows. Later cycles require a separate authored change. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	TArray<FExperienceCyclePlan> AuthoredPlans;
};

/** Fill the two currently authorized authored rows for development and tests when no asset is assigned. */
GLOAMSTEAD_API void PopulateDefaultExperienceCyclePlans(UExperienceCycleCatalog& Catalog);

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
