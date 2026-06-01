#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/VeilHeartWarningTypes.h"
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
    AVeilHeart();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void EvaluateRestorationAgainstWarnings(const FRestorationEventPayload& Payload);

    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void EmitWarningForNight(ENightConsequenceType NightType);

    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void ProcessDawnReflection();

    UFUNCTION(BlueprintPure, Category="Veil Heart")
    int32 GetSatisfiedWarningTagCount() const { return SatisfiedWarningTags.Num(); }

    UFUNCTION(BlueprintImplementableEvent, Category="Veil Heart")
    void OnWarningEmitted(const FVeilHeartWarningFragment& WarningFragment);

protected:
    UFUNCTION()
    void OnRestorationComplete(const FRestorationEventPayload& Payload);

    const FVeilHeartWarningFragment* FindWarningForNight(ENightConsequenceType NightType) const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Veil Heart")
    TObjectPtr<UVeilHeartWarningCatalog> WarningCatalog;

private:
    TSet<FName> SatisfiedWarningTags;
};