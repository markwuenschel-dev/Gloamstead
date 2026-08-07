// Copyright Epic Games, Inc. All Rights Reserved.

#include "GloamsteadCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Components/RitualPlacementComponent.h"
#include "Components/GloamInteractionComponent.h"
#include "Gloamstead.h"

AGloamsteadCharacter::AGloamsteadCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Gloamstead gameplay components: ritual-point restoration + world-object interaction verbs.
	RitualPlacement = CreateDefaultSubobject<URitualPlacementComponent>(TEXT("RitualPlacement"));
	Interaction = CreateDefaultSubobject<UGloamInteractionComponent>(TEXT("Interaction"));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AGloamsteadCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGloamsteadCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AGloamsteadCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGloamsteadCharacter::Look);

		// Gloamstead verbs (null-checked: their IA assets may not be assigned in early Blueprints yet).
		if (RestoreAction)
		{
			EnhancedInputComponent->BindAction(RestoreAction, ETriggerEvent::Started, this, &AGloamsteadCharacter::OnRestoreInput);
		}
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AGloamsteadCharacter::OnInteractInput);
		}
		if (ExamineAction)
		{
			EnhancedInputComponent->BindAction(ExamineAction, ETriggerEvent::Started, this, &AGloamsteadCharacter::OnExamineInput);
		}
	}
	else
	{
		UE_LOG(LogGloamstead, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AGloamsteadCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AGloamsteadCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AGloamsteadCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AGloamsteadCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AGloamsteadCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AGloamsteadCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AGloamsteadCharacter::OnRestoreInput()
{
	// Logged unconditionally: this verb reaches the player only through Enhanced Input, and every
	// failure mode below (no component, mode toggled but no valid target) is otherwise silent. Without
	// this line there is no way to tell "the key never arrived" from "the key arrived and was refused".
	UE_LOG(LogTemp, Log, TEXT("GloamInput: Restore pressed (placement=%s, valid=%s)"),
		RitualPlacement && RitualPlacement->IsInPlacementMode() ? TEXT("on") : TEXT("off"),
		RitualPlacement && RitualPlacement->IsCurrentPlacementValid() ? TEXT("yes") : TEXT("no"));

	if (!RitualPlacement)
	{
		return;
	}

	// First press arms placement (the preview begins); a press while a valid target is previewed confirms
	// the restoration — this is the player action the FirstNightDirector's dusk gate waits on.
	if (!RitualPlacement->IsInPlacementMode())
	{
		RitualPlacement->EnterPlacementMode();
		UE_LOG(LogTemp, Log, TEXT("GloamInput: entered placement mode (valid target=%s)"),
			RitualPlacement->IsCurrentPlacementValid() ? TEXT("yes") : TEXT("no"));
		return;
	}

	if (RitualPlacement->IsCurrentPlacementValid())
	{
		RitualPlacement->ConfirmPlacement();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("GloamInput: Restore ignored — no valid target in range."));
	}
}

FText AGloamsteadCharacter::GetPlayerPromptText() const
{
	if (RitualPlacement && RitualPlacement->IsInPlacementMode())
	{
		return RitualPlacement->IsCurrentPlacementValid()
			? NSLOCTEXT("Gloamstead", "PromptConfirmRitual", "[R]  Restore the lantern        [E]  Cancel")
			: NSLOCTEXT("Gloamstead", "PromptNoRitualSite", "No ritual site within reach        [E]  Cancel");
	}

	if (Interaction)
	{
		const FText Focused = Interaction->GetCurrentPrompt();
		if (!Focused.IsEmpty())
		{
			return FText::Format(NSLOCTEXT("Gloamstead", "PromptInteract", "[E]  {0}"), Focused);
		}
	}

	return FText::GetEmpty();
}

void AGloamsteadCharacter::GloamTeleport(float X, float Y, float Z)
{
	const FVector Target(X, Y, Z);
	SetActorLocation(Target, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogTemp, Log, TEXT("GloamInput: teleported to %s (playtest positioning)."), *Target.ToCompactString());
}

void AGloamsteadCharacter::OnInteractInput()
{
	UE_LOG(LogTemp, Log, TEXT("GloamInput: Interact pressed (placement=%s)"),
		RitualPlacement && RitualPlacement->IsInPlacementMode() ? TEXT("on") : TEXT("off"));

	// While arming a restoration, Interact doubles as cancel — the player can back out of placement
	// (Restore itself is a place/confirm toggle with no other exit).
	if (RitualPlacement && RitualPlacement->IsInPlacementMode())
	{
		RitualPlacement->ExitPlacementMode();
		return;
	}

	if (Interaction)
	{
		Interaction->TryInteract();
	}
}

void AGloamsteadCharacter::OnExamineInput()
{
	if (Interaction)
	{
		Interaction->TryExamine();
	}
}
