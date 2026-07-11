#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "VeilHeart.generated.h"

/**
 * The Veil Heart - Central protected object and emotional core of Gloamstead.
 * Listens to restoration events to evaluate "I understood the warning".
 */
UCLASS(Blueprintable, BlueprintType)
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

    /** Legacy no-outcome dawn reflection (BP compat): reflects on an empty outcome. */
    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void ProcessDawnReflection();

    /** Dawn reflection with the night's real outcome; distinguishes success / partial / failure. */
    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void ProcessDawnReflectionWithOutcome(const FNightRuntimeOutcome& Outcome);

    UFUNCTION(BlueprintPure, Category="Veil Heart")
    int32 GetSatisfiedWarningTagCount() const { return SatisfiedWarningTags.Num(); }

    /** The outcome of the most recently reflected-upon night (session memory the next cycle can read). */
    UFUNCTION(BlueprintPure, Category="Veil Heart")
    FNightRuntimeOutcome GetLastNightOutcome() const { return LastNightOutcome; }

    UFUNCTION(BlueprintImplementableEvent, Category="Veil Heart")
    void OnWarningEmitted(const FVeilHeartWarningFragment& WarningFragment);

    /** BP presentation hook for the dawn payoff (journal/feedback/VFX), fed the night's outcome. */
    UFUNCTION(BlueprintImplementableEvent, Category="Veil Heart")
    void OnDawnReflection(const FNightRuntimeOutcome& Outcome);

    /** Assign Content/Data/DA_VeilHeartWarningCatalog (auto-loaded at BeginPlay if left empty). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Veil Heart")
    TObjectPtr<UVeilHeartWarningCatalog> WarningCatalog;

protected:
    UFUNCTION()
    void OnRestorationComplete(const FRestorationEventPayload& Payload);

    const FVeilHeartWarningFragment* FindWarningForNight(ENightConsequenceType NightType) const;

private:
    TSet<FName> SatisfiedWarningTags;

    UPROPERTY()
    FNightRuntimeOutcome LastNightOutcome;
};