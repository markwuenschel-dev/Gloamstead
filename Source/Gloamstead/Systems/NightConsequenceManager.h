#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/RitualTypes.h"
#include "NightConsequenceManager.generated.h"

/**
 * Night Consequence Manager - Reads restorations to shape night behavior.
 * Uses fast parallel state from PCG Subsystem for performance.
 */
UCLASS()
class GLOAMSTEAD_API UNightConsequenceManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

protected:
    UFUNCTION()
    void OnStructureRestored(const FRestorationEventPayload& Payload);

    UFUNCTION(BlueprintCallable, Category="Night")
    void PrepareNightConsequences();

private:
    UPROPERTY()
    class UGloamsteadPCGSubsystem* PCGSubsystem = nullptr;

    TMap<int32, float> PathSegmentLightCoverage;
};