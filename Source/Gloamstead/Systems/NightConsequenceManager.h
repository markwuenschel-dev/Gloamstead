#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/ExperienceCycleTypes.h"
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

	/**
	 * Prepares the exact authored consequence supplied by the cycle owner. This
	 * is deliberately separate from score-based generic selection: invalid
	 * plans fail closed and emit no runtime-ready delegate.
	 */
	bool PrepareNightConsequencesForPlan(const FExperienceCyclePlan& Plan);

	UFUNCTION(BlueprintPure, Category = "Night")
	ENightConsequenceType GetLastSelectedNightType() const { return LastSelectedNightType; }

	UFUNCTION(BlueprintPure, Category = "Night")
	FName GetOmenClueTagForNightType(ENightConsequenceType NightType) const;

	/** Test seam: deterministic night selection over the assigned catalog, without needing a world/PCG subsystem. */
	ENightConsequenceType Test_SelectNightType(const FNightSanctuarySnapshot& Snapshot) { return SelectNightTypeFromCatalog(Snapshot); }

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
	void EnsureNightCatalog();
	float ScoreRule(const FNightConsequenceRule& Rule, const FNightSanctuarySnapshot& Snapshot) const;
	ENightConsequenceType SelectNightTypeFromCatalog(const FNightSanctuarySnapshot& Snapshot);

	UPROPERTY()
	class UGloamsteadPCGSubsystem* PCGSubsystem = nullptr;

	TMap<int32, float> PathSegmentLightCoverage;

	ENightConsequenceType LastSelectedNightType = ENightConsequenceType::Invalid;

	int32 NightsPrepared = 0;
};
