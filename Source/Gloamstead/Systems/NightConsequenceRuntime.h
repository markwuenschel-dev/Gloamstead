#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/RitualTypes.h"
#include "NightConsequenceRuntime.generated.h"

class UNightStrategy;
class UGloamsteadPCGSubsystem;
class ANightPressureActor;
struct FExperienceCyclePlan;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNightConsequenceStarted, ENightConsequenceType, NightType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNightConsequenceEnded, ENightConsequenceType, NightType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOmenClueReady, FName, ClueTag);
/** Broadcast when the night's objective resolves early — the phase authority should advance to dawn. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNightShouldEnd);

/**
 * Runs the selected night as a real consequence (Corrected Wave 2): builds a context, delegates
 * behavior to a per-type UNightStrategy, applies escalating pressure on a cadence, observes
 * restorations to resolve the objective (ending the night intentionally), and produces an
 * FNightRuntimeOutcome that dawn reflection consumes.
 */
UCLASS()
class GLOAMSTEAD_API UNightConsequenceRuntime : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UNightConsequenceRuntime();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Night")
	void BeginNight();

	UFUNCTION(BlueprintCallable, Category = "Night")
	void EndNight();

	/**
	 * Drops a live night before a save payload restores its own PCG baseline.
	 *
	 * This is deliberately not EndNight(): a restore must neither resolve the old
	 * strategy nor publish/record an outcome from the world being replaced.
	 */
	void AbortNightForRestore();

	UFUNCTION(BlueprintPure, Category = "Night")
	ENightConsequenceType GetPlannedNightType() const { return PlannedNightType; }

	UFUNCTION(BlueprintPure, Category = "Night")
	ENightConsequenceType GetActiveNightType() const { return ActiveNightType; }

	UFUNCTION(BlueprintPure, Category = "Night")
	bool IsNightActive() const { return bNightActive; }

	/** True after an objective has asked the phase authority for an early Dawn this run. */
	bool HasRequestedEarlyDawn() const { return bEarlyDawnRequested; }

	UFUNCTION(BlueprintPure, Category = "Night")
	FNightRuntimeOutcome GetLastOutcome() const { return LastOutcome; }

	UFUNCTION(BlueprintPure, Category = "Night")
	bool IsObjectiveResolved() const;

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnNightConsequenceStarted OnNightStarted;

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnNightConsequenceEnded OnNightEnded;

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnOmenClueReady OnOmenClueReady;

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnNightShouldEnd OnNightShouldEnd;

	/** Seconds between escalating pressure steps during the night. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "0.1"))
	float PressureStepSeconds = 2.0f;

	/** Spawn the optional light-reactive pressure actor during a threat night (game world only). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night")
	bool bSpawnPressureActor = true;

	/** Class of the optional pressure actor; defaults to ANightPressureActor if unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night")
	TSubclassOf<ANightPressureActor> PressureActorClass;

	// === Test seams (headless; no world/timer required) ===
	/** Force the planned night type before BeginNight (bypasses the manager). */
	void Test_SetPlannedNightType(ENightConsequenceType Type) { PlannedNightType = Type; }
	/** Manually advance one pressure step (what the timer would do). */
	void Test_StepPressure() { HandlePressureStep(); }
	/** The active strategy instance (for assertions). */
	UNightStrategy* Test_GetActiveStrategy() const { return ActiveStrategy; }
	/** Instantiate the strategy the runtime would use for a night type (proves the type->class mapping). */
	UNightStrategy* Test_MakeStrategyFor(ENightConsequenceType Type);
	/** True when the live pressure cadence still owns a timer in this world. */
	bool Test_IsPressureCadenceScheduled() const;
	/** True when a cosmetic pressure actor from the active night still exists. */
	bool Test_HasActivePressureActor() const { return !!ActivePressureActor; }
	/** Forces the next real BeginNight initial pressure beat to request early Dawn synchronously. */
	void Test_ForceEarlyDawnDuringInitialPressureBeat() { bTestForceEarlyDawnDuringInitialPressureBeat = true; }

	/**
	 * Resolves one stable authored semantic subject through existing PCG metadata.
	 * Returns INDEX_NONE for missing or ambiguous mappings; it never score-selects
	 * a substitute point.
	 */
	int32 ResolveSemanticSubjectToPoint(FName SemanticSubject, const UGloamsteadPCGSubsystem* PCG) const;

protected:
	UFUNCTION()
	void HandleNightPlanReady(ENightConsequenceType SelectedNightType);

	UFUNCTION()
	void HandleRestorationDuringNight(const FRestorationEventPayload& Payload);

private:
	FNightRuntimeContext BuildContext(UGloamsteadPCGSubsystem* PCG) const;
	const FExperienceCyclePlan* ResolveActiveAuthoredPlan() const;
	TSubclassOf<UNightStrategy> ResolveStrategyClass(ENightConsequenceType Type) const;
	void HandlePressureStep();
	void BroadcastOmenClueIfNeeded();
	void MaybeSpawnPressureActor(UGloamsteadPCGSubsystem* PCG);
	void DestroyPressureActor();

	UPROPERTY()
	TMap<ENightConsequenceType, TSubclassOf<UNightStrategy>> StrategyClasses;

	UPROPERTY()
	TObjectPtr<UNightStrategy> ActiveStrategy = nullptr;

	UPROPERTY()
	FNightRuntimeContext ActiveContext;

	UPROPERTY()
	FNightRuntimeOutcome LastOutcome;

	UPROPERTY()
	ENightConsequenceType PlannedNightType = ENightConsequenceType::Invalid;

	UPROPERTY()
	ENightConsequenceType ActiveNightType = ENightConsequenceType::Invalid;

	UPROPERTY()
	bool bNightActive = false;

	/** Suppresses pressure cadence after an objective already asked the phase authority for Dawn. */
	bool bEarlyDawnRequested = false;
	/** True only around BeginNight's synchronous first pressure beat. */
	bool bInitialPressureBeatInProgress = false;
	/** Narrow live-regression hook; consumed only during the initial beat above. */
	bool bTestForceEarlyDawnDuringInitialPressureBeat = false;

	UPROPERTY()
	TObjectPtr<ANightPressureActor> ActivePressureActor = nullptr;

	FTimerHandle PressureTimer;
};
