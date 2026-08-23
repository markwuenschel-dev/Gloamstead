#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/GloamInteractable.h"
#include "GloamsteadEvidenceSource.generated.h"

class USphereComponent;

/**
 * A placed, authored source of one readable warning support channel.
 *
 * WorldForge owns where instances and their provenance are materialized; this
 * actor owns only the player-world encounter endpoint. It never accepts a
 * caller-supplied warning/support pair: the Heart reads the authored values
 * from this live actor and validates them against its active plan.
 */
UCLASS(BlueprintType)
class GLOAMSTEAD_API AGloamsteadEvidenceSource : public AActor, public IGloamInteractable
{
	GENERATED_BODY()

public:
	AGloamsteadEvidenceSource();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void Examine_Implementation(AActor* Interactor) override;

	/** C++ interaction endpoint; deliberately not BlueprintCallable. */
	bool ReportEncounter(AActor* Interactor);

	FName GetWarningId() const { return WarningId; }
	FName GetSupportId() const { return SupportId; }
	FName GetChannelType() const { return ChannelType; }

	/** WorldForge-authored identity values. Blueprint may read but cannot forge an encounter payload. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Evidence Source")
	FName WarningId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Evidence Source")
	FName SupportId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Evidence Source")
	FName ChannelType = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Evidence Source")
	FText InteractionPrompt;

	/** Query-only focus volume so the normal player interaction trace can discover this source. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Evidence Source")
	TObjectPtr<USphereComponent> InteractionVolume;
};
