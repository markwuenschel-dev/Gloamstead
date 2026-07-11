#pragma once

#include "CoreMinimal.h"
#include "Data/NightConsequenceTypes.h"
#include "NightRuntimeTypes.generated.h"

/**
 * Data contracts for the real night consequence runtime (Corrected Wave 2).
 * A night starts with a CONTEXT, resolves an OBJECTIVE, and ends with an OUTCOME
 * that dawn reflection consumes. See docs/gloamstead/waves/corrected_wave_2_plan.md.
 */

/** What kind of thing the player is being asked to do during the night. */
UENUM(BlueprintType)
enum class ENightObjectiveKind : uint8
{
	/** No objective (benign night / unsupported type). */
	None                    = 0,
	/** Cleanse an escalating corruption bloom (restore the target point) before dawn. */
	CleanseCorruptionBloom  = 1,
	/** Bounded, always-winnable teaching beat. */
	TutorialTeach           = 2,
	/** Heed an omen: interpret the sign and restore the marked vulnerable point before dawn (Night Types II). */
	HeedOmen                = 3,
	/** Hold a restored point the night is trying to reclaim; re-stabilize it before dawn (Night Types II). */
	HoldRestored            = 4,
};

/** How the night resolved, in ascending severity of failure. */
UENUM(BlueprintType)
enum class ENightOutcomeResult : uint8
{
	/** Night never ran / not resolved. */
	None    = 0,
	/** Objective met (bloom cleansed / lesson learned). */
	Success = 1,
	/** Progress made but objective not fully met (bloom reduced, not cleared). */
	Partial = 2,
	/** Objective failed (bloom untouched or worsened) — fail-forward scar. */
	Failure = 3,
};

/** Immutable inputs captured when the night begins. */
USTRUCT(BlueprintType)
struct FNightRuntimeContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	ENightConsequenceType NightType = ENightConsequenceType::Invalid;

	/** Sanctuary aggregates captured at dusk/night-start. */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	FNightSanctuarySnapshot DuskSnapshot;

	/** The bloom to cleanse (-1 if no corrupted point exists). */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	int32 TargetPointIndex = -1;

	/** The target's corruption level at night start (for delta/partial scoring). */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	float TargetStartCorruption = 0.f;

	/** True if the player heeded the dusk warning (satisfied a matching warning tag) before night. */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	bool bWarningHeeded = false;
};

/** Live objective state the strategy tracks through the night. */
USTRUCT(BlueprintType)
struct FNightObjective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	ENightObjectiveKind Kind = ENightObjectiveKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	int32 TargetPointIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	float StartCorruption = 0.f;

	/** Corruption at or below this level counts the bloom as cleansed. */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	float ResolveAtOrBelow = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	bool bResolved = false;
};

/** The night's result, handed to dawn reflection. */
USTRUCT(BlueprintType)
struct FNightRuntimeOutcome
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	ENightOutcomeResult Result = ENightOutcomeResult::None;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	ENightConsequenceType NightType = ENightConsequenceType::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	ENightObjectiveKind ObjectiveKind = ENightObjectiveKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	bool bObjectiveResolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	bool bWarningHeeded = false;

	/** Change in sanctuary-average corruption across the night (+ = worsened). */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	float SanctuaryCorruptionDelta = 0.f;

	/** Change in the target bloom's corruption across the night (+ = worsened, - = cleansed). */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	float TargetCorruptionDelta = 0.f;

	/** A scar (failure) or blessing (success) marker the next cycle / journal can read. */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	FName ResultTag = NAME_None;
};

GLOAMSTEAD_API FString GetNightOutcomeResultDisplayName(ENightOutcomeResult Result);
GLOAMSTEAD_API FString GetNightObjectiveKindDisplayName(ENightObjectiveKind Kind);
