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

public:

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

