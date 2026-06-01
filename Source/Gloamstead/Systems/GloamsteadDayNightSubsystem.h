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
UCLASS()
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

	UPROPERTY(BlueprintAssignable, Category = "DayNight")
	FOnGloamsteadDayPhaseChanged OnPhaseChanged;

private:
	void ApplyPhaseChange(EGloamsteadDayPhase NewPhase);
	void HandleEnterDusk();
	void HandleEnterDawn();

	UPROPERTY()
	EGloamsteadDayPhase CurrentPhase = EGloamsteadDayPhase::Day;

	UPROPERTY()
	int32 NightCount = 0;
};