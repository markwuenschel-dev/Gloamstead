#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/NightConsequenceTypes.h"
#include "NightConsequenceRuntime.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNightConsequenceStarted, ENightConsequenceType, NightType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNightConsequenceEnded, ENightConsequenceType, NightType);

/**
 * Executes the prepared night plan during the Night phase (stubs in NC-3; spawn/combat later).
 */
UCLASS()
class GLOAMSTEAD_API UNightConsequenceRuntime : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Night")
	void BeginNight();

	UFUNCTION(BlueprintCallable, Category = "Night")
	void EndNight();

	UFUNCTION(BlueprintPure, Category = "Night")
	ENightConsequenceType GetPlannedNightType() const { return PlannedNightType; }

	UFUNCTION(BlueprintPure, Category = "Night")
	ENightConsequenceType GetActiveNightType() const { return ActiveNightType; }

	UFUNCTION(BlueprintPure, Category = "Night")
	bool IsNightActive() const { return bNightActive; }

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnNightConsequenceStarted OnNightStarted;

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnNightConsequenceEnded OnNightEnded;

protected:
	UFUNCTION()
	void HandleNightPlanReady(ENightConsequenceType SelectedNightType);

	UPROPERTY()
	ENightConsequenceType PlannedNightType = ENightConsequenceType::Invalid;

	UPROPERTY()
	ENightConsequenceType ActiveNightType = ENightConsequenceType::Invalid;

	UPROPERTY()
	bool bNightActive = false;
};