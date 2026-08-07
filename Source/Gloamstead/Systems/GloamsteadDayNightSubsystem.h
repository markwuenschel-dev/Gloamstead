#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GloamsteadDayNightSubsystem.generated.h"

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
	UFUNCTION(BlueprintCallable, Category = "DayNight")
	EGloamsteadDayPhase GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintCallable, Category = "DayNight")
	int32 GetNightCount() const { return NightCount; }

	/** 0 = dawn, 1 = dusk (per FRestorationEventPayload contract). */
	UFUNCTION(BlueprintPure, Category = "DayNight")
	float GetNormalizedTimeOfDay() const;

	UFUNCTION(BlueprintCallable, Category = "DayNight")
	void AdvanceToNextPhase();

	UFUNCTION(BlueprintCallable, Category = "DayNight")
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

	/** True when the current phase is one the player may rest through (Day or Dawn). */
	UFUNCTION(BlueprintPure, Category = "DayNight")
	bool CanRestNow() const;

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

	UPROPERTY(BlueprintAssignable, Category = "DayNight")
	FOnGloamsteadDayPhaseChanged OnPhaseChanged;

private:
	void ApplyPhaseChange(EGloamsteadDayPhase NewPhase);
	void HandleEnterDusk();
	void HandleEnterNight();
	void HandleEnterDawn();

	UPROPERTY()
	EGloamsteadDayPhase CurrentPhase = EGloamsteadDayPhase::Day;

	UPROPERTY()
	int32 NightCount = 0;

	UPROPERTY()
	bool bDawnAutosaveEnabled = true;

	/** Set once by the first-night director when the lantern lesson is complete. */
	UPROPERTY()
	bool bFirstRestUnlocked = false;
};
