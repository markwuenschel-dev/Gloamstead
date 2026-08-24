#pragma once

#include "CoreMinimal.h"
#include "Data/ExperienceCycleTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "GloamsteadDayNightSubsystem.generated.h"

class UNightConsequenceRuntime;
class AVeilHeart;

UENUM(BlueprintType)
enum class EGloamsteadDayPhase : uint8
{
	Day   UMETA(DisplayName = "Day"),
	Dusk  UMETA(DisplayName = "Dusk"),
	Night UMETA(DisplayName = "Night"),
	Dawn  UMETA(DisplayName = "Dawn"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGloamsteadDayPhaseChanged, EGloamsteadDayPhase, OldPhase, EGloamsteadDayPhase, NewPhase);

/**
 * Thin phase authority for the vertical slice: drives dusk night prep and dawn reflection.
 */
UCLASS(BlueprintType)
class GLOAMSTEAD_API UGloamsteadDayNightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Seconds Dusk remains readable before this phase authority starts the prepared night. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Cadence", meta = (ClampMin = "0.0"))
	float DuskToNightDelaySeconds = 6.0f;

	/** Maximum Night duration before this phase authority requests Dawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Cadence", meta = (ClampMin = "0.0"))
	float NightDurationSeconds = 45.0f;

	UFUNCTION(BlueprintCallable, Category = "DayNight")
	EGloamsteadDayPhase GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintCallable, Category = "DayNight")
	int32 GetNightCount() const { return NightCount; }

	/** 0 = dawn, 1 = dusk (per FRestorationEventPayload contract). */
	UFUNCTION(BlueprintPure, Category = "DayNight")
	float GetNormalizedTimeOfDay() const;

	UFUNCTION(BlueprintCallable, Category = "DayNight")
	void AdvanceToNextPhase();

	/**
	 * C++ automation/internal forcing seam. It is deliberately not reflected to
	 * Blueprint: gameplay Day->Dusk authority must use the guarded advance path.
	 */
	void SetPhase(EGloamsteadDayPhase NewPhase);

	/**
	 * Player-driven "rest" advance: the only phases the player controls at the Heart are the resting ones.
	 * Day -> Dusk (rest to bring the night) and Dawn -> Day (wake into the new day). Inert during Dusk/Night
	 * (those are timer/objective-driven). Returns true if it advanced. This is what drives the recurring loop
	 * once the scripted first-night director has gone dormant. */
	UFUNCTION(BlueprintCallable, Category = "DayNight")
	bool RequestRest();

	/** Controls only the dawn disk write; phase progression and reflection remain unchanged. */
	UFUNCTION(BlueprintCallable, Category = "DayNight|Persistence")
	void SetDawnAutosaveEnabled(bool bEnabled) { bDawnAutosaveEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "DayNight|Persistence")
	bool IsDawnAutosaveEnabled() const { return bDawnAutosaveEnabled; }

	/** Arms and presents the exact authored plan for the next rest during Day. */
	UFUNCTION(BlueprintCallable, Category = "DayNight|Experience")
	/**
	 * Re-deliver the warning the player is currently owed to a newly registered presenter.
	 *
	 * Dawn earns the coming night's warning, but the presenter can change between dawn and day - the
	 * first-night director detaches once the tutorial resolves and a generic presenter takes over. A
	 * warning delivered to a presenter that is about to disappear is a warning the player never sees, so
	 * whoever presents now is shown the currently armed warning.
	 */
	void NotifyWarningPresenterChanged();

	bool PrepareUpcomingCycle();

	/** Returns the active exact authored plan, or nullptr when no safe plan is armed. */
	const struct FExperienceCyclePlan* GetUpcomingPlan() const;

	/** Saves PCG and day/cycle progression together in one sanctuary payload. */
	UFUNCTION(BlueprintCallable, Category = "DayNight|Persistence")
	bool SaveProgressionToSlot();
	bool SaveProgressionToSlot(const FString& SlotName, int32 UserIndex = 0) const;

	/** Restores PCG before day/cycle state, then reconciles a safe upcoming plan. */
	UFUNCTION(BlueprintCallable, Category = "DayNight|Persistence")
	bool LoadProgressionFromSlot();
	bool LoadProgressionFromSlot(const FString& SlotName, int32 UserIndex = 0);

	/** Called by a late-spawned Heart to apply a validated v3 interpretation snapshot once. */
	void NotifyHeartReadyForProgressionRestore(AVeilHeart* Heart);

	/** True when the current phase is one the player may rest through (Day or Dawn). */
	UFUNCTION(BlueprintPure, Category = "DayNight")
	bool CanRestNow() const;

	/** True while an otherwise valid authored plan awaits exact Heart presentation. */
	UFUNCTION(BlueprintPure, Category = "DayNight|Experience")
	bool IsWarningPresentationPending() const { return bWarningPresentationPending; }

	/**
	 * Opens the very first Day->Dusk rest, which CanRestNow() otherwise refuses while NightCount==0.
	 *
	 * The first night's Day->Dusk transition is gated on the lantern tutorial. The gate itself still
	 * belongs to the FirstNightDirector — this only tells the phase authority that the gate has been
	 * satisfied, so the *player* performs the transition by resting at the Heart. Without this the
	 * director had to advance the phase itself, which skipped the player's rest entirely.
	 */
	UFUNCTION(BlueprintCallable, Category = "DayNight")
	void UnlockFirstRest();

	UFUNCTION(BlueprintPure, Category = "DayNight")
	bool IsFirstRestUnlocked() const { return bFirstRestUnlocked; }

	/** Runtime delegate: an objective resolved before the cadence deadline. */
	UFUNCTION()
	void HandleNightShouldEnd();

	// === Test seams (unconditional inline; unused in shipping → linker drops them) ===
	/** Binds the same exact runtime early-objective delegate used by HandleEnterNight. */
	void Test_BindCadenceRuntime(UNightConsequenceRuntime* InRuntime);
	/** Number of distinct runtime/cadence requests that actually entered Dawn this session. */
	int32 Test_GetCadenceDawnRequestCount() const { return CadenceDawnRequestCount; }
	bool Test_IsDuskToNightCadenceScheduled() const { return bDuskToNightCadenceScheduled; }
	bool Test_IsNightToDawnCadenceScheduled() const { return bNightToDawnCadenceScheduled; }
	/** True while Night has been announced to presentation but its runtime has not started yet. */
	bool Test_IsNightRuntimeStartScheduled() const { return bNightRuntimeStartQueued; }

	UPROPERTY(BlueprintAssignable, Category = "DayNight")
	FOnGloamsteadDayPhaseChanged OnPhaseChanged;

private:
	void ApplyPhaseChange(EGloamsteadDayPhase NewPhase);
	/** Checks/retries the exact Day warning before any gameplay Day->Dusk advance. */
	bool CanAdvanceFromDayToDusk();
	void HandleEnterDay();
	void HandleEnterDusk();
	void HandleEnterNight();
	void HandleEnterDawn();
	/** Rejects a progression payload as one coherent, rest-ineligible Day state. */
	void ResetToSafeDayReconciliation();
	/**
	 * Reconciles visual listeners after a restore assigned CurrentPhase directly.
	 * This deliberately broadcasts only: it must never re-enter cadence, phase
	 * entry work, outcome recording, autosave, or authored-plan preparation.
	 */
	void SynchronizePhasePresentationAfterProgressionRestore(EGloamsteadDayPhase PreviousPhase);
	void QueueWarningPresentationRetry();
	void RetryPendingWarningPresentation();
	void ClearWarningPresentationRetry();
	/** Clears all live/pending interpretation state before a save payload replaces the world. */
	void ResetHeartInterpretationForProgressionRestore();
	/** Applies the loaded v3 state to one exact Heart, or clears it safely on mismatch. */
	bool RestorePendingHeartInterpretation(AVeilHeart* Heart);
	/** Quiesces timers, early-dawn callbacks, and a live runtime before any PCG restore. */
	void QuiesceLiveWorldForProgressionRestore();
	/** A safe later-cycle Day cannot retain a Cycle I tutorial presenter or callbacks. */
	void DetachStaleFirstNightDirectorsForLaterCycleResume();
	void ScheduleDuskToNightCadence();
	void ScheduleNightToDawnCadence();
	void ClearCadenceTimers();
	void ClearDuskToNightCadence();
	void ClearNightToDawnCadence();
	void AdvanceFromDuskCadence();
	void RequestDawnFromCadence();
	void CommitDawnFromCadence();
	void DrainQueuedDawnTransition();
	void BindCadenceRuntime(UNightConsequenceRuntime* InRuntime);
	void UnbindCadenceRuntime();
	/** Starts the prepared runtime only after the Night phase event/presentation has completed. */
	void QueueNightRuntimeStart(UNightConsequenceRuntime* Runtime);
	void StartNightRuntimeAfterPhasePresentation();
	void ClearQueuedNightRuntimeStart();
	class UGloamsteadExperienceCycleSubsystem* GetExperienceCycleSubsystem() const;

	virtual void Deinitialize() override;

	UPROPERTY()
	EGloamsteadDayPhase CurrentPhase = EGloamsteadDayPhase::Day;

	UPROPERTY()
	int32 NightCount = 0;

	UPROPERTY()
	bool bDawnAutosaveEnabled = true;

	/** Set once by the first-night director when the lantern lesson is complete. */
	UPROPERTY()
	bool bFirstRestUnlocked = false;

	/** True only after Dusk prepared the exact active authored plan for runtime. */
	bool bDuskPlanPrepared = false;

	/** Reentrancy guard: emitting a warning must not recurse if a presenter registers while handling it. */
	bool bRepresentingForNewPresenter = false;

	/** The authored warning successfully exposed during the current Day. */
	FName PresentedPlanId = NAME_None;

	/** A valid authored plan can outlive Heart/catalog startup ordering. */
	bool bWarningPresentationPending = false;

	/** An injected legacy-v2 Dusk/Night snapshot may keep PCG aftermath but may never replay its plan. */
	bool bInProgressSaveReconciliation = false;
	FName PendingPresentationPlanId = NAME_None;
	bool bWarningPresentationRetryQueued = false;
	bool bWarningPresentationDeferralLogged = false;
	FTimerHandle WarningPresentationRetryTimer;

	/** Holds a valid v3 Heart snapshot while the map/bootstrap has not yet spawned its Heart actor. */
	FVeilHeartInterpretationPersistentState PendingHeartInterpretationState;
	bool bHasPendingHeartInterpretationState = false;

	/** Exactly-one guard shared by deadline and runtime objective completion. */
	bool bDawnTransitionRequested = false;
	/** An early-dawn request raised during a phase event/runtime startup waits for that work to settle. */
	bool bQueuedDawnTransition = false;
	/** Supports queuing a runtime early-dawn request until the complete phase event is observable. */
	int32 PhaseTransitionDepth = 0;
	/** Prevents an early objective raised by BeginNight from re-entering the runtime's own stack. */
	bool bNightRuntimeStartupInProgress = false;
	int32 CadenceDawnRequestCount = 0;
	bool bDuskToNightCadenceScheduled = false;
	bool bNightToDawnCadenceScheduled = false;
	bool bNightRuntimeStartQueued = false;
	FTimerHandle DuskToNightCadenceTimer;
	FTimerHandle NightToDawnCadenceTimer;
	FTimerHandle NightRuntimeStartTimer;

	UPROPERTY(Transient)
	TObjectPtr<UNightConsequenceRuntime> CadenceRuntime;

	/** Keeps the entered Night's runtime alive through its one-frame presentation deferral. */
	UPROPERTY(Transient)
	TObjectPtr<UNightConsequenceRuntime> PendingNightRuntime;
};
