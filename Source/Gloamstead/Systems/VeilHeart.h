#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RitualTypes.h"
#include "VeilHeart.generated.h"

/**
 * The Veil Heart - Central protected object and emotional core of Gloamstead.
 * Listens to restoration events to evaluate "I understood the warning".
 */
UCLASS()
class GLOAMSTEAD_API AVeilHeart : public AActor
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;

protected:
    UFUNCTION()
    void OnStructureRestored(const FRestorationEventPayload& Payload);

    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void EvaluateRestorationAgainstWarnings(const FRestorationEventPayload& Payload);

    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void ProcessDawnReflection();

private:
    UPROPERTY()
    class UGloamsteadPCGSubsystem* PCGSubsystem = nullptr;

    TSet<FName> SatisfiedWarningTags;
};