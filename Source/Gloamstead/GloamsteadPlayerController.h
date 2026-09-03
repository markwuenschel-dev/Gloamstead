// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GloamsteadPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AGloamsteadPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/**
	 * Contextual prompt HUD ("[E] Rest at the Heart", ritual confirm/cancel).
	 *
	 * Created here rather than by a Blueprint child so the affordance exists for any controller in
	 * the slice: the interaction component has always produced prompt text and nothing displayed it.
	 * Leave unset to use the project prompt widget; clear bUseProjectDefaultPromptWidget to opt out.
	 */
	UPROPERTY(EditAnywhere, Category = "Gloamstead|UI")
	TSubclassOf<UUserWidget> PromptWidgetClass;

	UPROPERTY(EditAnywhere, Category = "Gloamstead|UI", meta = (AdvancedDisplay))
	bool bUseProjectDefaultPromptWidget = true;

	UPROPERTY()
	TObjectPtr<UUserWidget> PromptWidget;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Creates and screen-adds the contextual prompt HUD. Safe to call once per local controller. */
	void CreatePromptWidget();

public:

	/**
	 * True while the player is holding the sanctuary. Read by AGloamsteadHUD, which draws the
	 * overlay; the controller owns the state because it is the thing that actually pauses the world
	 * and it outlives HUD recreation.
	 */
	UFUNCTION(BlueprintPure, Category = "Gloamstead|Pause")
	bool IsSanctuaryPaused() const { return bSanctuaryPaused; }

	/** Automated-playtest and accessibility hook for the pause (also bound to Escape). */
	UFUNCTION(Exec)
	void GloamPause() { ToggleSanctuaryPause(); }

	/** Holds or releases the sanctuary. Idempotent per press; safe with no world. */
	void ToggleSanctuaryPause();

private:

	bool bSanctuaryPaused = false;

};
