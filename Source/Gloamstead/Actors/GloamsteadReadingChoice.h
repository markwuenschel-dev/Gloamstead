#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/GloamInteractable.h"
#include "GloamsteadReadingChoice.generated.h"

class USphereComponent;

/**
 * A placed, authored way to act on the SECOND clause of the active Heart warning.
 *
 * Every warning from Cycle II onward names a minimum ("Wake the roots") and then a contrastive pair
 * ("Wet earth shelters; bare ash feeds the Gloam"). Restoring the subject answers the minimum. These
 * actors are how the rest of the sentence becomes something a player can DO: the sluice gate beside
 * the garden bed, the ash brazier next to it, the mirror rotation at the overlook, the bell rope.
 *
 * It is deliberately the same shape as AGloamsteadEvidenceSource, and for the same reason: the actor
 * owns only the world-side encounter endpoint and never a payload. It carries an authored identity
 * that the Heart reads off the live actor and validates against its own active plan, so neither
 * Blueprint nor a generic caller can assert which reading the player took.
 *
 * A choice is inert until the plan interpretation receipt exists. That ordering is the whole point:
 * a second reading configures a restoration the player already earned; it is never a way to skip it.
 */
UCLASS(BlueprintType)
class GLOAMSTEAD_API AGloamsteadReadingChoice : public AActor, public IGloamInteractable
{
	GENERATED_BODY()

public:
	AGloamsteadReadingChoice();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void Examine_Implementation(AActor* Interactor) override;

	/** C++ commit endpoint; deliberately not BlueprintCallable. */
	bool ReportChoice(AActor* Interactor);

	FName GetWarningId() const { return WarningId; }
	FName GetReadingId() const { return ReadingId; }

	/** Authored identity. Blueprint may read it, but cannot forge a commit payload. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Reading Choice")
	FName WarningId = NAME_None;

	/** Must name a second reading the active authored plan declares. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Reading Choice")
	FName ReadingId = NAME_None;

	/**
	 * Optional override for the HUD verb. Left empty, the prompt comes from the authored plan itself,
	 * which is the better default: the reading's own ChoicePrompt is the text the designer wrote for
	 * this exact reading, so it cannot drift out of step with the grade it carries.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Reading Choice")
	FText InteractionPromptOverride;

	/** Query-only focus volume so the normal player interaction trace can discover this choice. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reading Choice")
	TObjectPtr<USphereComponent> InteractionVolume;

private:
	/** The one Heart in this world, or nullptr when ownership is ambiguous. */
	class AVeilHeart* ResolveSoleHeart() const;
};
