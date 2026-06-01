#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "NightConsequenceManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNightPlanReady, ENightConsequenceType, SelectedNightType);

/**
 * Night Consequence Manager - Reads restorations to shape night behavior.
 */
UCLASS()
class GLOAMSTEAD_API UNightConsequenceManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Night")
	void PrepareNightConsequences();

	UFUNCTION(BlueprintPure, Category = "Night")
	ENightConsequenceType GetLastSelectedNightType() const { return LastSelectedNightType; }

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnNightPlanReady OnNightPlanReady;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night")
	TObjectPtr<UNightConsequenceCatalog> NightCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night")
	bool bForceTutorialOnFirstNight = true;

protected:
	UFUNCTION()
	void OnStructureRestored(const FRestorationEventPayload& Payload);

private:
	float ScoreRule(const FNightConsequenceRule& Rule, const FNightSanctuarySnapshot& Snapshot) const;
	ENightConsequenceType SelectNightTypeFromCatalog(const FNightSanctuarySnapshot& Snapshot);

	UPROPERTY()
	class UGloamsteadPCGSubsystem* PCGSubsystem = nullptr;

	TMap<int32, float> PathSegmentLightCoverage;

	ENightConsequenceType LastSelectedNightType = ENightConsequenceType::Invalid;

	int32 NightsPrepared = 0;
};