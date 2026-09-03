#pragma once

#include "CoreMinimal.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/RitualTypes.h"
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
 * How sharply the player read the SECOND clause of an authored Heart warning.
 *
 * Every warning from Cycle II onward is built the same way: an imperative that names the
 * minimum restoration ("Wake the roots"), then a contrastive pair that names a sharper
 * reading and a plausible-but-wrong overreading ("Wet earth shelters; bare ash feeds the
 * Gloam"). The imperative alone gets the player through the night. This enum grades what
 * they did with the rest of the sentence.
 *
 * It is deliberately NOT a difficulty setting or a hidden collectible: the Heart already
 * said both halves out loud, and the world already carries the evidence for both. The grade
 * records an interpretation the player could have reached, never a secret they had to guess.
 */
UENUM(BlueprintType)
enum class EExperienceReadingGrade : uint8
{
	/** No second reading was committed. The minimum restoration stands alone; the night is winnable. */
	Unread    = 0,
	/** The sharper reading the warning's second clause rewards. Earns a durable advantage for the night. */
	Insight   = 1,
	/** A defensible reading that neither earns an advantage nor costs one. */
	Plain     = 2,
	/** The plausible-but-wrong overinterpretation the warning explicitly cautions against. Makes the night worse. */
	Overreach = 3,
};

GLOAMSTEAD_API FString GetExperienceReadingGradeDisplayName(EExperienceReadingGrade Grade);

/**
 * One authored way to act on a warning's second clause, offered at the restored subject.
 *
 * A reading is a CONFIGURATION of a restoration the player has already earned, never a
 * substitute for it. That ordering is enforced at runtime: the Heart refuses a reading until
 * the plan's interpretation receipt exists.
 */
USTRUCT(BlueprintType)
struct GLOAMSTEAD_API FExperienceCycleSecondReading
{
	GENERATED_BODY()

	/** Stable authored identity, unique within its plan. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	FName ReadingId = NAME_None;

	/** What this reading is worth. Exactly one Insight and exactly one Overreach per plan. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	EExperienceReadingGrade Grade = EExperienceReadingGrade::Unread;

	/** Player-facing verb shown on the world object that offers this configuration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	FText ChoicePrompt;

	/** Player-facing dawn line naming, in plain terms, what this reading actually did. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	FText OutcomeSummary;

	/** Durable boon/scar marker later cycles and the journal read. NAME_None is correct for Plain. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	FName ConsequenceTag = NAME_None;

	/** A reading is authored only when it is identifiable, graded, and readable by the player. */
	bool IsAuthored() const
	{
		return ReadingId != NAME_None
			&& Grade != EExperienceReadingGrade::Unread
			&& !ChoicePrompt.ToString().TrimStartAndEnd().IsEmpty()
			&& !OutcomeSummary.ToString().TrimStartAndEnd().IsEmpty();
	}
};

/**
 * Concrete proof that the player committed one exact authored second reading.
 *
 * Like FExperienceInterpretationReceipt, this is minted only by AVeilHeart after the whole
 * chain agrees, and it is the only thing the night runtime is allowed to read when it decides
 * how much pressure this night carries.
 */
USTRUCT(BlueprintType)
struct GLOAMSTEAD_API FExperienceSecondReadingVerdict
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	FName ReadingId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	FName PlanId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	FName WarningId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	EExperienceReadingGrade Grade = EExperienceReadingGrade::Unread;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	FName ConsequenceTag = NAME_None;

	bool IsValid() const
	{
		return ReadingId != NAME_None
			&& PlanId != NAME_None
			&& WarningId != NAME_None
			&& Grade != EExperienceReadingGrade::Unread;
	}

	/** Any partially populated verdict is malformed persisted state, not an absent verdict. */
	bool HasAnyFacts() const
	{
		return ReadingId != NAME_None
			|| PlanId != NAME_None
			|| WarningId != NAME_None
			|| Grade != EExperienceReadingGrade::Unread
			|| ConsequenceTag != NAME_None;
	}

	void Reset() { *this = FExperienceSecondReadingVerdict(); }
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

	/** The ritual form that can satisfy the plan's exact restoration requirement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	ERitualType RequiredRitualType = ERitualType::Invalid;

	/** Stable identifiers for the readable evidence channels this plan permits. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	TArray<FName> RequiredSupportIds;

	/**
	 * Required readable medium for each RequiredSupportIds entry at the same
	 * array index.  The contract is deliberately authored here rather than
	 * inferred from text: a GardenRot clue is fair only when the player can find
	 * its environmental, object-reaction, and audio evidence as distinct modes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	TArray<FName> RequiredSupportChannelTypes;

	/** Number of distinct known supports the player must encounter before interpreting this plan. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle", meta = (ClampMin = "0"))
	int32 MinimumDistinctSupportCount = 0;

	/** Stable receipt id written only after the exact warning, supports, and restoration agree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	FName InterpretationReceiptId = NAME_None;

	/** Generic world-state key which an external generator may mirror but never author. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	FName VisualStateKey = NAME_None;

	/** Stable key for the later player-facing dawn summary. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	FName OutcomeSummaryKey = NAME_None;

	/**
	 * Authored second-order readings of this warning's second clause. Empty is legal and
	 * means this cycle asks only for the minimum reading - which is exactly right for the
	 * Cycle I tutorial, where the player is still learning that restoration matters at all.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle|Second Reading")
	TArray<FExperienceCycleSecondReading> SecondReadings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience Cycle")
	EExperiencePlanResolution Resolution = EExperiencePlanResolution::Invalid;

	bool IsAuthoredPlan() const { return Resolution == EExperiencePlanResolution::Authored; }
	bool IsGenericHandoff() const { return Resolution == EExperiencePlanResolution::GenericHandoff; }
	bool IsInvalid() const { return Resolution == EExperiencePlanResolution::Invalid; }

	/** True when this plan asks the player for anything beyond the minimum restoration. */
	bool OffersSecondReading() const { return !SecondReadings.IsEmpty(); }

	const FExperienceCycleSecondReading* FindSecondReading(FName InReadingId) const;

	/**
	 * The authoring rule the whole mechanic rests on: a plan either offers NO readings, or it
	 * offers a complete, unambiguous set - unique authored IDs, exactly one Insight, exactly
	 * one Overreach, and at least one Plain middle so the sharp reading is a choice rather
	 * than a coin flip. A half-authored set would let a night silently grade a player against
	 * a reading they were never offered, so it is refused rather than tolerated.
	 */
	bool HasCoherentSecondReadings(FString* OutError = nullptr) const;

	static FExperienceCyclePlan MakeInvalid(int32 InSlot);
	static FExperienceCyclePlan MakeGenericHandoff(int32 InSlot);
};

/** Concrete proof that a player interpreted one exact authored warning fairly. */
USTRUCT(BlueprintType)
struct GLOAMSTEAD_API FExperienceInterpretationReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Interpretation")
	FName ReceiptId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Interpretation")
	FName PlanId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Interpretation")
	FName WarningId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Interpretation")
	FName SemanticSubject = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Interpretation")
	FName RestorationTag = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Interpretation")
	ERitualType RestorationRitualType = ERitualType::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Interpretation")
	int32 RestorationPointIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Experience Cycle|Interpretation")
	TArray<FName> SupportIds;

	bool IsValid() const
	{
		return ReceiptId != NAME_None
			&& PlanId != NAME_None
			&& WarningId != NAME_None
			&& SemanticSubject != NAME_None
			&& RestorationTag != NAME_None
			&& RestorationRitualType != ERitualType::Invalid
			&& RestorationPointIndex != INDEX_NONE
			&& SupportIds.Num() > 0;
	}

	/** Any partially populated receipt is malformed persisted state, not an empty receipt. */
	bool HasAnyFacts() const
	{
		return ReceiptId != NAME_None
			|| PlanId != NAME_None
			|| WarningId != NAME_None
			|| SemanticSubject != NAME_None
			|| RestorationTag != NAME_None
			|| RestorationRitualType != ERitualType::Invalid
			|| RestorationPointIndex != INDEX_NONE
			|| !SupportIds.IsEmpty();
	}
};

/**
 * The durable, player-meaningful portion of the Heart's interpretation state.
 * This deliberately excludes timers, presenters, and all runtime pressure;
 * those have no safe resume contract.  DayNight restores this atomically with
 * the authored plan, or clears it before a rollback can leak a future clue.
 */
USTRUCT(BlueprintType)
struct GLOAMSTEAD_API FVeilHeartInterpretationPersistentState
{
	GENERATED_BODY()

	/** Exact authored plan that presented this warning; warning IDs may be reused across night types. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
	FName PresentedPlanId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
	FName PresentedWarningId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
	TArray<FName> EncounteredSupportIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
	FExperienceInterpretationReceipt InterpretationReceipt;

	/**
	 * The second reading the player committed, if any. It is persisted beside the receipt rather
	 * than derived from it because the two are separate acts: the receipt proves the player read
	 * the warning, and this proves what they did with the rest of the sentence.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
	FExperienceSecondReadingVerdict SecondReadingVerdict;

	bool HasAnyFacts() const
	{
		return PresentedPlanId != NAME_None
			|| PresentedWarningId != NAME_None
			|| !EncounteredSupportIds.IsEmpty()
			|| InterpretationReceipt.HasAnyFacts()
			|| SecondReadingVerdict.HasAnyFacts();
	}

	void Reset()
	{
		PresentedPlanId = NAME_None;
		PresentedWarningId = NAME_None;
		EncounteredSupportIds.Reset();
		InterpretationReceipt = FExperienceInterpretationReceipt();
		SecondReadingVerdict.Reset();
	}
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

/** Fill the currently authorized authored rows for development and tests when no asset is assigned. */
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

	/** Presented warning, encountered evidence, and exact receipt for the armed plan. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience Cycle|Persistence")
	FVeilHeartInterpretationPersistentState HeartInterpretationState;

    /** Clear data that a legacy payload cannot establish and require an explicit reconciliation. */
    void ResetForLegacyReconciliation();
};
