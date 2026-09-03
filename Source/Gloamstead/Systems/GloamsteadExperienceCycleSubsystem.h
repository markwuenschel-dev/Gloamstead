#pragma once

#include "CoreMinimal.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GloamsteadExperienceCycleSubsystem.generated.h"

/**
 * Sole owner of authored-cycle identity. It deliberately has no world or
 * scoring dependency, making the armed plan stable across Day, Dusk, Night,
 * Dawn, save/restore, and automation construction.
 */
UCLASS(BlueprintType)
class GLOAMSTEAD_API UGloamsteadExperienceCycleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Arms the next exact authored plan. Returns false for invalid authored data
	 * or for the explicit post-authored generic handoff; callers must inspect
	 * GetActivePlan() and never treat false as permission to substitute meaning.
	 */
	/**
	 * True when every authored cycle has been completed and no further plan exists.
	 *
	 * This is the ending's only signal. FExperienceCyclePlan::IsGenericHandoff() had no production reader
	 * at all - the end of the authored experience manifested as an unhandled boundary: rest was refused
	 * forever, the Heart became permanently non-interactable, and the only trace was one log line.
	 */
	UFUNCTION(BlueprintPure, Category = "Experience Cycle")
	bool IsExperienceComplete() const { return ActivePlan.IsGenericHandoff(); }

	/** How many cycles the authored catalog actually defines. The experience ends after this many. */
	UFUNCTION(BlueprintPure, Category = "Experience Cycle")
	int32 GetAuthoredSlotCount() const;

	bool EnsureUpcomingPlan();

	/** The immutable active result: authored, invalid, or explicit generic handoff. */
	const FExperienceCyclePlan& GetActivePlan() const { return ActivePlan; }

	/** Restores persisted cycle facts and reconstructs an armed canonical plan when present. */
	bool RestorePersistentState(const FExperienceCyclePersistentState& InPersistentState);

	/** Captures the Task 1 state surface plus the currently armed canonical plan ID. */
	FExperienceCyclePersistentState CapturePersistentState() const;

	/**
	 * Records the active authored plan's resolved dawn facts, then clears the
	 * armed plan so only the following Day can author the next one.
	 */
	bool RecordActivePlanOutcome(const FNightRuntimeOutcome& Outcome);

#if WITH_DEV_AUTOMATION_TESTS
	/** Narrow test-only injection seam for malformed authored catalog fixtures. */
	void Test_SetCatalog(UExperienceCycleCatalog* InCatalog);
#endif

private:
	void EnsureCatalog();
	const FExperienceCyclePlan* FindCanonicalRequiredPlan(int32 Slot) const;
	bool RestoreArmedPlan(FName ArmedPlanId, int32 ExpectedSlot);
	void SetInvalidPlan(int32 Slot);
	void SetGenericHandoff(int32 Slot);

	UPROPERTY(Transient)
	TObjectPtr<UExperienceCycleCatalog> ExperienceCatalog;

	FExperienceCyclePersistentState PersistentState;
	FExperienceCyclePlan ActivePlan;
	bool bPersistentRestoreFailed = false;
};
