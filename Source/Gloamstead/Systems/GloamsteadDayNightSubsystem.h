#pragma once

#include "CoreMinimal.h"
#include "Data/NightConsequenceTypes.h"
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

/** The phase as a word, for logs and diagnostics. The HUD keeps its own upper-case display form. */
GLOAMSTEAD_API FString GetGloamsteadDayPhaseDisplayName(EGloamsteadDayPhase Phase);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGloamsteadDayPhaseChanged, EGloamsteadDayPhase, OldPhase, EGloamsteadDayPhase, NewPhase);

/**
 * Thin phase authority for the vertical slice: drives dusk night prep and dawn reflection.
 */
UCLASS(BlueprintType)
class GLOAMSTEAD_API UGloamsteadDayNightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * When true, Dusk waits for the player to bring the night at the Heart instead of expiring on
	 * DuskToNightDelaySeconds. That makes every transition the player controls a deliberate act:
	 * Day -> Dusk -> Night by hand, and Night -> Dawn by completing the night's objective.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Cadence")
	bool bPlayerAdvancesDuskToNight = true;

	/** Seconds Dusk remains readable before this phase authority starts the prepared night. Ignored while bPlayerAdvancesDuskToNight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Cadence", meta = (ClampMin = "0.0"))
	float DuskToNightDelaySeconds = 6.0f;

	/**
	 * The night's outer ceiling, in seconds. It is a CEILING, not a length: a night that resolves its
	 * objective ends early through OnNightShouldEnd, so this only governs a night the player has not
	 * yet answered.
	 *
	 * Raised from 45 to 100 on 2026-09-02, from the arithmetic rather than by feel. This sanctuary's
	 * ritual points span 603-1266 units from the Heart, so it is about 25 m across and a player at
	 * MaxWalkSpeed 500 crosses it in ~5 s. A threat approaches at ApproachSpeed 180 (scaled down by
	 * light), so it needs ~7 s to reach its target in the dark, then drains at LightDrainPerSecond
	 * 0.06 - about 17 s to take a full point down - with DisruptionSeconds 3 bought back per strike.
	 * One complete threat lifecycle is therefore ~25-30 s.
	 *
	 * At 45 s the player saw roughly one and a half of those: barely time to notice a threat, cross
	 * to it and strike once, which is not "pressure while you cleanse or activate" so much as a
	 * cutscene with a verb in it. At 100 s they get three or four - enough to misread the night, lose
	 * ground, and recover, which is the shape the design asks for.
	 *
	 * **Raised from 100 to 300 on 2026-09-03, because 100 was never derived from the half-hour
	 * target.** The arithmetic above sizes a night against one threat lifecycle, which is the right
	 * way to pick a *minimum* and says nothing about how long the experience should be. Measured, a
	 * full arc at 100 came to 8m23s. The plan of record asks for six cycles of ~45-60 min for the
	 * six-hour version; the same shape at half an hour is ~5 min a cycle, and the night is most of a
	 * cycle. 300 x the scalars below puts the arc at 27.9-31.0 minutes.
	 *
	 * The honest cost, stated rather than hidden: these nights are long, and a night whose objective
	 * is already answered is time the player spends holding ground rather than being tested. That is
	 * why the quiet nights were reweighted *down* (Tutorial 0.30, Corruption 0.50) while the threat
	 * nights carry the budget - the alternative was three or four minutes of tutorial night with
	 * nothing in it, which is the same dead air the 45 s ceiling was raised to fix.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Cadence", meta = (ClampMin = "0.0"))
	float NightDurationSeconds = 300.0f;

	/**
	 * How a night of this kind scales the base ceiling above.
	 *
	 * One number for all six nights was the wrong shape, and the arithmetic above is what shows it:
	 * that reasoning derives 100 s from a threat lifecycle of ~25-30 s, wanting three or four of
	 * them. But Cycles I and II field NO threat at all - BuildNightThreatRoster returns an empty
	 * roster for Tutorial, Corruption and Omen - so those nights were spending 100 s delivering
	 * three or four lifecycles of nothing, which is the same dead air the 45 s ceiling was raised to
	 * fix, arrived at from the other side. Cycle VI fields three threats at once and got the same
	 * 100 s to answer all of them.
	 *
	 * So the multiplier tracks what is actually out there: below 1.0 for the nights whose pressure
	 * is corruption spreading rather than something walking, at 1.0 for the single-threat nights the
	 * base was derived for, and well above it for the siege. The base UPROPERTY still governs the
	 * whole curve, so tuning one number retunes every night in proportion.
	 *
	 * Static and pure so the arc's total night time is testable without standing up a world.
	 */
	static float NightDurationScaleForType(ENightConsequenceType Type);

	/**
	 * The ceiling for the night now running, in seconds.
	 *
	 * Every reader must use this rather than NightDurationSeconds directly - notably the HUD
	 * countdown, which would otherwise count down to a dawn that arrives at a different time. A
	 * countdown that disagrees with the clock it claims to show is worse than no countdown.
	 */
	UFUNCTION(BlueprintPure, Category = "DayNight|Cadence")
	float GetCurrentNightDurationSeconds() const;

	/**
	 * The least of a night that must actually be endured, as a fraction of its ceiling.
	 *
	 * A night ends early when its objective resolves, which is the right rule and was producing the
	 * wrong game. Measured across a full arc: Cycles I-III ran 70s, 80s and 100s - their whole
	 * authored ceilings - and Cycles IV, V and VI ran **6s, 1s and 10s**. The escalation, the
	 * bargain and the three-threat siege were all over before they began, because their objectives
	 * resolve the moment the runtime evaluates them, so 88% of the arc's night time was spent in its
	 * first half and the climax was a flash.
	 *
	 * A floor keeps the rule and fixes the shape: answering the night still ends it early, but not
	 * before the night has been a night. It is a fraction rather than a constant so it scales with
	 * NightDurationScaleForType - a quiet tutorial night and a siege get proportionate floors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Cadence", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NightMinimumFraction = 0.9f;

	/** Seconds an early dawn must still wait for, or 0 when the night has run its floor. */
	UFUNCTION(BlueprintPure, Category = "DayNight|Cadence")
	float GetEarlyDawnHoldSeconds() const;

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

	/**
	 * True when Dusk is prepared and waiting for the player to bring the night by hand. Kept separate from
	 * CanRestNow() on purpose: "rest" means the Day/Dawn resting phases, and that meaning is asserted across
	 * the suite. This is the Dusk-only gate that RequestRest consults first.
	 */
	UFUNCTION(BlueprintPure, Category = "DayNight")
	bool CanBeginNightNow() const;

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

	/** World time Night began, so the floor above can be measured against something real. */
	float NightBeganWorldTime = 0.f;

	/** Armed when an objective resolved before the night's floor; fires the deferred dawn. */
	FTimerHandle EarlyDawnHoldTimer;

	/** Keeps the entered Night's runtime alive through its one-frame presentation deferral. */
	UPROPERTY(Transient)
	TObjectPtr<UNightConsequenceRuntime> PendingNightRuntime;
};
