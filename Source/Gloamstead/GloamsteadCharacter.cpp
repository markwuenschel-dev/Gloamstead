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
#include "InputCoreTypes.h"
#include "Components/RitualPlacementComponent.h"
#include "Components/GloamInteractionComponent.h"
#include "Systems/NightConsequenceRuntime.h"
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

		// The ward is intentionally a key-level fallback: early character Blueprints do not yet carry
		// a dedicated IA asset, but the possession night must still be playable without editor wiring.
		PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AGloamsteadCharacter::OnWardInput);

		// Strike, on the same key-level footing and for the same reason. Ward answers a threat; strike
		// only buys the seconds needed to reach the light or the mirror that will. Without it a player
		// watching a Gatherer drain the lantern they raised has nothing to do about it at all - the
		// runtime's DisruptNearestThreat was fully implemented and had no caller anywhere.
		PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AGloamsteadCharacter::OnStrikeInput);

		// Cycle 5's deliberate choice uses number keys so it remains playable even when
		// a project's early input-mapping assets have no dedicated choice actions yet.
		PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AGloamsteadCharacter::OnMirrorRefuseInput);
		PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AGloamsteadCharacter::OnMirrorAcceptInput);
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
		// Refresh before reporting refusal: a plan can change between placement
		// ticks, and the component owns the player-facing reason a nearby ritual
		// no longer answers the Heart.
		RitualPlacement->ForceUpdatePreview();
		UE_LOG(LogTemp, Log, TEXT("GloamInput: Restore ignored — no valid target in range."));
	}
}

FText AGloamsteadCharacter::GetPlayerPromptText() const
{
	if (RitualPlacement && RitualPlacement->IsInPlacementMode())
	{
		const ERitualType RitualType = RitualPlacement->GetPlacementRitualType();
		if (RitualPlacement->IsCurrentPlacementValid())
		{
			switch (RitualType)
			{
			case ERitualType::GardenBed:
				return NSLOCTEXT("Gloamstead", "PromptConfirmGardenBed", "[R]  Tend the garden bed        [E]  Cancel");
			case ERitualType::LanternPost:
				return NSLOCTEXT("Gloamstead", "PromptConfirmLantern", "[R]  Restore the lantern        [E]  Cancel");
			default:
				return NSLOCTEXT("Gloamstead", "PromptConfirmRitual", "[R]  Complete the restoration        [E]  Cancel");
			}
		}

		const FText PlacementStatus = RitualPlacement->GetPlacementStatusText();
		if (!PlacementStatus.IsEmpty())
		{
			return PlacementStatus;
		}

		return RitualType == ERitualType::GardenBed
			? NSLOCTEXT("Gloamstead", "PromptNoGardenBed", "No garden bed within reach        [E]  Cancel")
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

	if (UWorld* World = GetWorld())
	{
		if (const UNightConsequenceRuntime* Night = World->GetSubsystem<UNightConsequenceRuntime>();
			Night
			&& Night->IsNightActive()
			&& Night->GetActiveNightType() == ENightConsequenceType::SilencePossession
			&& !Night->IsObjectiveResolved())
		{
			return NSLOCTEXT("Gloamstead", "PromptWardPossession", "[RMB]  Ward the possessed place with light");
		}

		if (const UNightConsequenceRuntime* Night = World->GetSubsystem<UNightConsequenceRuntime>();
			Night
			&& Night->IsMirrorChoicePending())
		{
			return NSLOCTEXT("Gloamstead", "PromptMirrorChoice", "[1]  Refuse the reflection        [2]  Accept the bargain");
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

void AGloamsteadCharacter::OnWardInput()
{
	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceRuntime* Night = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			// One beat of light answers both things it can answer: the night's objective, and
			// whatever is standing in front of the player. Splitting them across two keys would make
			// the player choose between the threat and the night, and the whole design is that the
			// threat IS answered by the same light that answers the night.
			const bool bWarded = Night->WardActiveThreat();
			const bool bCleansed = Night->CleanseNearestThreat(GetActorLocation());
			UE_LOG(LogTemp, Log, TEXT("GloamInput: Ward pressed (objective=%s, threat=%s)."),
				bWarded ? TEXT("answered") : TEXT("no"),
				bCleansed ? TEXT("cleansed") : TEXT("no"));
			return;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("GloamInput: Ward pressed with no night runtime."));
}

void AGloamsteadCharacter::OnStrikeInput()
{
	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceRuntime* Night = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			// Deliberately never resolves anything - only Cleanse does that, and only for archetypes
			// that can be cleansed at all. Striking is the way to spend time instead of light.
			const bool bStruck = Night->DisruptNearestThreat(GetActorLocation());
			UE_LOG(LogTemp, Log, TEXT("GloamInput: Strike pressed (threat=%s)."),
				bStruck ? TEXT("interrupted") : TEXT("nothing in reach"));
			return;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("GloamInput: Strike pressed with no night runtime."));
}

void AGloamsteadCharacter::OnMirrorRefuseInput()
{
	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceRuntime* Night = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			const bool bAccepted = Night->ResolveMirrorChoice(/*bAccept*/ false);
			UE_LOG(LogTemp, Log, TEXT("GloamInput: Mirror refusal pressed (accepted=%s)."), bAccepted ? TEXT("yes") : TEXT("no"));
			return;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("GloamInput: Mirror refusal pressed with no night runtime."));
}

void AGloamsteadCharacter::OnMirrorAcceptInput()
{
	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceRuntime* Night = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			const bool bAccepted = Night->ResolveMirrorChoice(/*bAccept*/ true);
			UE_LOG(LogTemp, Log, TEXT("GloamInput: Mirror bargain pressed (accepted=%s)."), bAccepted ? TEXT("yes") : TEXT("no"));
			return;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("GloamInput: Mirror bargain pressed with no night runtime."));
}
