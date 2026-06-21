// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GloamInteractable.generated.h"

/**
 *  Marks an actor the player can focus and act on with the Gloamstead interaction verbs.
 *
 *  Implementable in both C++ and Blueprint (restored-actor BPs, the Veil Heart rest point,
 *  examinable clues), so UGloamInteractionComponent can drive them without knowing the concrete
 *  type. Each function is a BlueprintNativeEvent; UHT supplies the C++ defaults (CanInteract → false,
 *  GetInteractionPrompt → empty, Interact/Examine → no-op), so an implementer overrides only what it
 *  needs — but MUST override CanInteract (or it is inert). Restoration of ritual *points* is a
 *  separate system (URitualPlacementComponent); this interface is for discrete world objects.
 */
UINTERFACE(MinimalAPI, BlueprintType, Blueprintable, meta = (DisplayName = "Gloam Interactable"))
class UGloamInteractable : public UInterface
{
	GENERATED_BODY()
};

class GLOAMSTEAD_API IGloamInteractable
{
	GENERATED_BODY()

public:
	/** Whether this object can be interacted with by Interactor right now. UHT default is false — override to allow. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Gloam|Interactable")
	bool CanInteract(AActor* Interactor) const;

	/** Short verb/label shown on the HUD prompt while focused (e.g. "Rest", "Examine"). Default: empty. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Gloam|Interactable")
	FText GetInteractionPrompt() const;

	/** Primary verb (the Interact input). Default: no-op. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Gloam|Interactable")
	void Interact(AActor* Interactor);

	/** Secondary "examine / focus" verb (the Examine input). Default: no-op. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Gloam|Interactable")
	void Examine(AActor* Interactor);
};
