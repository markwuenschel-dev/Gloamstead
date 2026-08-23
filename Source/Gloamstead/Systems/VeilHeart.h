#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Interfaces/GloamInteractable.h"
#include "VeilHeart.generated.h"

class USphereComponent;

/**
 * Broadcast when the Heart warns about the coming night, carrying the catalog fragment it chose.
 *
 * The BlueprintImplementableEvents below can only be answered by a Blueprint SUBCLASS of the Heart,
 * which meant the warning text was unreachable by anything else — including the first-night director,
 * which is the actor that actually owns the caption widget. These delegates let any listener present
 * the Heart's voice without having to be the Heart.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVeilHeartWarning, const FVeilHeartWarningFragment&, WarningFragment);

/** Broadcast at dawn with the night's real outcome, for the same reason as FOnVeilHeartWarning. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVeilHeartDawnReflection, const FNightRuntimeOutcome&, Outcome);

/**
 * The Veil Heart - Central protected object and emotional core of Gloamstead.
 * Listens to restoration events to evaluate "I understood the warning".
 *
 * It is also the player's "rest point": implementing IGloamInteractable, the Interact verb rests through
 * the resting phases (Day -> Dusk to bring the night, Dawn -> Day to wake), the player-driven advance that
 * carries the recurring loop; Examine speaks the Heart's memory of the last night.
 */
UCLASS(Blueprintable, BlueprintType)
class GLOAMSTEAD_API AVeilHeart : public AActor, public IGloamInteractable
{
    GENERATED_BODY()

public:
    AVeilHeart();

    virtual void BeginPlay() override;

    // IGloamInteractable — the Heart is the player's rest point.
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual FText GetInteractionPrompt_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual void Examine_Implementation(AActor* Interactor) override;

    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void EvaluateRestorationAgainstWarnings(const FRestorationEventPayload& Payload);

	UFUNCTION(BlueprintCallable, Category="Veil Heart")
	void EmitWarningForNight(ENightConsequenceType NightType);

	/**
	 * Emits one exact authored warning. The ID must appear exactly once in the
	 * assigned catalog and its associated type must equal ExpectedNightType.
	 */
	UFUNCTION(BlueprintCallable, Category="Veil Heart")
	bool EmitWarningById(FName WarningId, ENightConsequenceType ExpectedNightType);

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

	/** Exact authored warning most recently presented by this Heart. */
	UFUNCTION(BlueprintPure, Category="Veil Heart")
	FName GetLastEmittedWarningId() const { return LastEmittedWarningId; }

	/**
	 * Presentation is player-facing only when a non-Heart consumer has attached
	 * to the explicit warning delegate (the first-night director owns that role
	 * in the current slice). Blueprint events on this actor are not readiness.
	 */
	bool HasExternalWarningPresenter() const { return OnWarningEmittedDelegate.IsBound(); }

    UFUNCTION(BlueprintImplementableEvent, Category="Veil Heart")
    void OnWarningEmitted(const FVeilHeartWarningFragment& WarningFragment);

    /** BP presentation hook for the dawn payoff (journal/feedback/VFX), fed the night's outcome. */
    UFUNCTION(BlueprintImplementableEvent, Category="Veil Heart")
    void OnDawnReflection(const FNightRuntimeOutcome& Outcome);

    /** Fires alongside OnWarningEmitted, for listeners that are not Blueprint subclasses of the Heart. */
    UPROPERTY(BlueprintAssignable, Category="Veil Heart")
    FOnVeilHeartWarning OnWarningEmittedDelegate;

    /** Fires alongside OnDawnReflection, for listeners that are not Blueprint subclasses of the Heart. */
    UPROPERTY(BlueprintAssignable, Category="Veil Heart")
    FOnVeilHeartDawnReflection OnDawnReflectionDelegate;

    /** Assign Content/Data/DA_VeilHeartWarningCatalog (auto-loaded at BeginPlay if left empty). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Veil Heart")
    TObjectPtr<UVeilHeartWarningCatalog> WarningCatalog;

    /**
     * Query-only volume so the player's interaction focus trace (an object-type overlap) can find the Heart.
     * Without it the Heart has no collision at all and rest/greet-dawn can never fire for a real player.
     * Non-blocking (overlap responses) so it never impedes movement.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Veil Heart")
    TObjectPtr<USphereComponent> InteractionVolume;

protected:
    UFUNCTION()
    void OnRestorationComplete(const FRestorationEventPayload& Payload);

    const FVeilHeartWarningFragment* FindWarningForNight(ENightConsequenceType NightType) const;

private:
	/** Lazily loads the assigned catalog for startup-order-safe exact emission. */
	bool EnsureWarningCatalog();

    TSet<FName> SatisfiedWarningTags;

    UPROPERTY()
    FNightRuntimeOutcome LastNightOutcome;

	UPROPERTY()
	FName LastEmittedWarningId = NAME_None;
};
