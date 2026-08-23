// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "GloamsteadCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class URitualPlacementComponent;
class UGloamInteractionComponent;
class UNightConsequenceRuntime;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AGloamsteadCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Restoration of ritual points (queries the PCG subsystem, previews + confirms placement). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gloamstead", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URitualPlacementComponent> RitualPlacement;

	/** Focus + verbs for discrete world IGloamInteractable objects (Veil Heart rest, examinables). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gloamstead", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGloamInteractionComponent> Interaction;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Restore Input Action — the signature verb: enter ritual placement, then confirm a valid target. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* RestoreAction;

	/** Interact Input Action — primary verb on the focused IGloamInteractable world object. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractAction;

	/** Examine Input Action — secondary "examine/focus" verb on the focused IGloamInteractable. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ExamineAction;

public:

	/** Constructor */
	AGloamsteadCharacter();	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Restore input: enter ritual placement mode, then confirm once the previewed target is valid. */
	void OnRestoreInput();

	/** Interact input: run the primary verb on the currently focused IGloamInteractable. */
	void OnInteractInput();

	/** Examine input: run the examine verb on the currently focused IGloamInteractable. */
	void OnExamineInput();

	/** Ward input: spend a deliberate light beat against a strategy-owned night threat. */
	void OnWardInput();

public:

	// === Automated-playtest console hooks ===
	//
	// Restore/Interact/Examine bind to ETriggerEvent::Started, and simulated key events do NOT produce
	// the press transition Enhanced Input needs for an edge trigger — verified empirically: with logging
	// on the handlers, playtest_key produced no handler call at all. These exec commands call the SAME
	// handler functions the input bindings call, so an automated run can exercise everything downstream
	// of the input layer. They add no behaviour of their own and are not bound to any key.
	//
	// They do NOT prove the keyboard path; that is established by the asset wiring (IMC_Default maps
	// R -> IA_Restore / E -> IA_Interact / Q -> IA_Examine, and the character's action slots are set).

	UFUNCTION(Exec)
	void GloamRestore() { OnRestoreInput(); }

	UFUNCTION(Exec)
	void GloamInteract() { OnInteractInput(); }

	UFUNCTION(Exec)
	void GloamExamine() { OnExamineInput(); }

	/** Automated-playtest and accessibility hook for the light ward (also bound to Right Mouse). */
	UFUNCTION(Exec)
	void GloamWard() { OnWardInput(); }

	/** Playtest positioning only: walking the plaza needs movement input the harness cannot simulate. */
	UFUNCTION(Exec)
	void GloamTeleport(float X, float Y, float Z);

	/**
	 * The single most relevant on-screen prompt for what the player can do right now, or empty.
	 *
	 * Nothing previously surfaced GloamInteractionComponent::GetCurrentPrompt(), so the Heart's
	 * "Rest at the Heart" and the ritual's confirm/cancel existed only as data. Resolution order is
	 * placement first, then the focused interactable, because while a ritual is armed that IS the
	 * thing the player is doing. The bracketed keys mirror IMC_Default (R = Restore, E = Interact).
	 */
	UFUNCTION(BlueprintPure, Category = "Gloamstead|UI")
	FText GetPlayerPromptText() const;

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Returns the RitualPlacement subobject **/
	FORCEINLINE URitualPlacementComponent* GetRitualPlacement() const { return RitualPlacement; }

	/** Returns the Interaction subobject **/
	FORCEINLINE UGloamInteractionComponent* GetInteraction() const { return Interaction; }
};
