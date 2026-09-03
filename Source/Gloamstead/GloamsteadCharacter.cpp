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
#include "Actors/GloamsteadEvidenceSource.h"
#include "Actors/GloamsteadReadingChoice.h"
#include "Interfaces/GloamInteractable.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/VeilHeart.h"
#include "Systems/NightConsequenceRuntime.h"
#include "UI/GloamsteadHUD.h"
#include "Gloamstead.h"

AGloamsteadCharacter::AGloamsteadCharacter()
{
	// Explicit rather than inherited: the walk-mode harness steers movement from Tick, and a pawn
	// that silently stopped ticking would report "never arrived" for a journey it never started.
	PrimaryActorTick.bCanEverTick = true;

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

	// The opening screen consumes the first Interact. Deliberately the same key the whole game uses
	// for "yes, this one" rather than a bespoke menu binding: the first thing a player presses
	// should already be teaching them the verb they will press most.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AGloamsteadHUD* Readout = Cast<AGloamsteadHUD>(PC->GetHUD()))
		{
			if (Readout->IsTitlePending())
			{
				Readout->DismissTitleScreen();
				UE_LOG(LogTemp, Log, TEXT("GloamInput: stepped into the sanctuary."));
				return;
			}
		}
	}

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

void AGloamsteadCharacter::GloamRest()
{
	UWorld* World = GetWorld();
	UGloamsteadDayNightSubsystem* DayNight =
		World ? World->GetSubsystem<UGloamsteadDayNightSubsystem>() : nullptr;
	if (!DayNight)
	{
		UE_LOG(LogTemp, Warning, TEXT("GloamRest: no phase authority in this world."));
		return;
	}

	const EGloamsteadDayPhase Before = DayNight->GetCurrentPhase();
	const bool bRested = DayNight->RequestRest();

	// Report the refusal as loudly as the success. A rest that is declined because the cycle's
	// work is not done is the correct answer, and a silent false here would read to an automated
	// pass as "nothing happened", which is the same thing a real defect reads as.
	UE_LOG(LogTemp, Log, TEXT("GloamRest: %s (phase %s -> %s)."),
		bRested ? TEXT("the night was brought") : TEXT("REFUSED - this cycle is not ready to rest"),
		*GetGloamsteadDayPhaseDisplayName(Before),
		*GetGloamsteadDayPhaseDisplayName(DayNight->GetCurrentPhase()));
}

void AGloamsteadCharacter::GloamUnlockFirstRest()
{
	UWorld* World = GetWorld();
	if (UGloamsteadDayNightSubsystem* DayNight =
			World ? World->GetSubsystem<UGloamsteadDayNightSubsystem>() : nullptr)
	{
		DayNight->UnlockFirstRest();
		UE_LOG(LogTemp, Log, TEXT("GloamUnlockFirstRest: the first rest is open."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GloamUnlockFirstRest: no phase authority in this world."));
	}
}

void AGloamsteadCharacter::GloamStatus()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>();
	const UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	const UGloamsteadExperienceCycleSubsystem* Cycles =
		World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UGloamsteadExperienceCycleSubsystem>() : nullptr;

	// One line, every fact an automated pass needs to assert the arc actually moved. Named
	// individually rather than dumped as a struct so a grep can pin any single one of them.
	UE_LOG(LogTemp, Log,
		TEXT("GloamStatus: phase=%s nights=%d cycle=%d complete=%s light=%.2f corruption=%.2f"),
		DayNight ? *GetGloamsteadDayPhaseDisplayName(DayNight->GetCurrentPhase()) : TEXT("NO-AUTHORITY"),
		DayNight ? DayNight->GetNightCount() : -1,
		Cycles ? Cycles->GetActivePlan().Slot : -1,
		(Cycles && Cycles->IsExperienceComplete()) ? TEXT("yes") : TEXT("no"),
		PCG ? PCG->GetSanctuaryAverageLightLevel() : -1.f,
		PCG ? PCG->GetSanctuaryAverageCorruptionLevel() : -1.f);
}

void AGloamsteadCharacter::BeginPlay()
{
	Super::BeginPlay();
	StartAutoPlayIfRequested();
}

void AGloamsteadCharacter::StartAutoPlayIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("GloamAutoPlay")))
	{
		return;
	}

	float Beat = 1.0f;
	FParse::Value(FCommandLine::Get(), TEXT("GloamAutoPlayBeat="), Beat);
	bAutoShots = FParse::Param(FCommandLine::Get(), TEXT("GloamAutoShots"));
	bAutoWalk  = FParse::Param(FCommandLine::Get(), TEXT("GloamAutoWalk"));

	UE_LOG(LogTemp, Log, TEXT("GloamAutoPlay: requested on the command line; beat %.2fs."), Beat);
	GloamAutoPlay(Beat);
}

void AGloamsteadCharacter::GloamAutoPlay(float BeatSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AutoPlayBeats = 0;

	// Deliberately does NOT open the first rest any more. It used to, on the assumption that the
	// harness could not reach a ritual point - and once it could, that shortcut turned out to skip
	// the one path that ends the tutorial: AGloamsteadFirstNightDirector only relinquishes control
	// once it has SEEN the lantern restored, so a run that unlocked its way past Cycle I left the
	// director attached for the whole arc with its caption still on screen. Restoring the lantern is
	// both the real Cycle I lesson and the thing that cleans up after it.
	const float Beat = FMath::Max(BeatSeconds, 0.25f);
	World->GetTimerManager().SetTimer(
		AutoPlayTimer, this, &AGloamsteadCharacter::AutoPlayBeat, Beat, /*bLoop*/ true, /*FirstDelay*/ Beat);

	UE_LOG(LogTemp, Log, TEXT("GloamAutoPlay: driving the arc unattended, one beat every %.2fs."), Beat);

}

void AGloamsteadCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Walk-mode locomotion. Movement input has to be applied per frame, not per harness beat, or
	// the character accelerates for one frame a second and effectively never arrives.
	if (bAutoWalkActive)
	{
		const FVector ToTarget = AutoWalkTarget - GetActorLocation();
		const FVector Flat(ToTarget.X, ToTarget.Y, 0.f);
		if (Flat.SizeSquared() > FMath::Square(AutoPlayArriveRadius))
		{
			const FVector Toward = Flat.GetSafeNormal();

			// Pinned against geometry: real movement input, no movement. Sidestep along the wall
			// for a moment, alternating sides so a corner cannot trap it in one direction forever.
			if (GetVelocity().SizeSquared() < FMath::Square(20.f))
			{
				AutoWalkStalledFor += DeltaSeconds;
			}
			else if (AutoWalkSidestepFor <= 0.f)
			{
				AutoWalkStalledFor = 0.f;
			}

			if (AutoWalkStalledFor > 0.6f && AutoWalkSidestepFor <= 0.f)
			{
				AutoWalkSidestepFor = 1.2f;
				AutoWalkSidestepSign = -AutoWalkSidestepSign;
				AutoWalkStalledFor = 0.f;
			}

			if (AutoWalkSidestepFor > 0.f)
			{
				AutoWalkSidestepFor -= DeltaSeconds;
				const FVector Along = FVector::CrossProduct(FVector::UpVector, Toward) * AutoWalkSidestepSign;
				// Mostly sideways, still leaning at the target, so it slides along the obstruction
				// rather than orbiting it.
				AddMovementInput((Along * 0.85f + Toward * 0.35f).GetSafeNormal(), 1.f);
			}
			else
			{
				AddMovementInput(Toward, 1.f);
			}
			SetActorRotation(FRotator(0.f, Flat.Rotation().Yaw, 0.f));

			// Distinguish "cannot move" from "cannot get there". A pawn with no controller silently
			// discards movement input, and a pawn walking into geometry stalls at a fixed distance
			// with real velocity - those are a harness bug and a map defect respectively, and they
			// look identical from outside.
			AutoWalkReportAccumulator += DeltaSeconds;
			if (AutoWalkReportAccumulator >= 2.f)
			{
				AutoWalkReportAccumulator = 0.f;
				UE_LOG(LogTemp, Log,
					TEXT("GloamAutoWalk: %.0f uu to go, speed %.0f, controller=%s, mode=%d"),
					Flat.Size(), GetVelocity().Size(),
					Controller ? TEXT("yes") : TEXT("NONE"),
					GetCharacterMovement() ? static_cast<int32>(GetCharacterMovement()->MovementMode) : -1);
			}
		}
		else
		{
			bAutoWalkActive = false;
		}
	}
}

bool AGloamsteadCharacter::AutoPlayApproach(const FVector& Where)
{
	if (!bAutoWalk)
	{
		// Teleport mode: arrive instantly, and lift clear of the ground so the capsule does not
		// spawn inside the geometry it was aimed at.
		SetActorLocation(Where + FVector(0.f, 0.f, 120.f), /*bSweep*/ false);
		bAutoWalkActive = false;
		return false;
	}

	const FVector Flat(Where.X - GetActorLocation().X, Where.Y - GetActorLocation().Y, 0.f);
	if (Flat.SizeSquared() <= FMath::Square(AutoPlayArriveRadius))
	{
		bAutoWalkActive = false;
		return false;
	}

	AutoWalkTarget = Where;
	bAutoWalkActive = true;
	return true;
}

bool AGloamsteadCharacter::AutoPlayDoDayWork()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	const UGloamsteadExperienceCycleSubsystem* Cycles =
		World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UGloamsteadExperienceCycleSubsystem>()
			: nullptr;
	if (!PCG || !Cycles)
	{
		return false;
	}

	const FExperienceCyclePlan& Plan = Cycles->GetActivePlan();
	if (!Plan.IsAuthoredPlan())
	{
		// The tutorial is the one stretch of the game with NO active plan to consult. Cycle I arms
		// no plan until the first rest is unlocked, and the first rest unlocks only once the lantern
		// has been restored - so during Cycle I the active slot is 0 and every plan-driven query
		// returns nothing. A player does not need a plan here either: AGloamsteadFirstNightDirector
		// simply says "find the ruined lantern", and that is the whole lesson.
		//
		// This is why three earlier runs "never accepted a lantern restoration". The harness was
		// asking the plan what to do, in the only cycle that has not got one.
		if (RitualPlacement)
		{
			const int32 Lantern = PCG->FindNearestUnrestoredPointIndex(
				GetActorLocation(), ERitualType::LanternPost, /*SearchRadius*/ 6000.f);
			if (Lantern != INDEX_NONE)
			{
				if (AutoPlayApproach(PCG->GetRitualPointLocation(Lantern)))
				{
					return true; // still walking to it
				}
				RitualPlacement->EnterPlacementMode();
				const bool bRestored = RitualPlacement->ConfirmPlacement();
				UE_LOG(LogTemp, Log, TEXT("GloamAutoPlay: tutorial lantern at point %d %s."),
					Lantern, bRestored ? TEXT("restored") : TEXT("refused restoration"));
				return bRestored;
			}
		}
		return false;
	}

	// --- 1. Read the evidence this cycle's warning is backed by --------------------------------
	//
	// Which clues are still outstanding comes from the Heart, via the same accessor the evidence
	// journal draws from. Without it the harness re-read the first source forever: a source stays
	// interactable after being reported, because the Heart dedupes the encounter internally rather
	// than closing the object. "What have I not found" is the question a player answers by looking
	// at the journal, so it is the right one for the harness too.
	AVeilHeart* Heart = nullptr;
	for (TActorIterator<AVeilHeart> HeartIt(World); HeartIt; ++HeartIt)
	{
		Heart = *HeartIt;
		break;
	}

	if (Heart)
	{
		TArray<FVeilHeartEvidenceLine> Lines;
		int32 Required = 0;
		if (Heart->GetStandingEvidence(Lines, Required))
		{
			for (const FVeilHeartEvidenceLine& Line : Lines)
			{
				if (Line.bFound)
				{
					continue;
				}
				for (TActorIterator<AGloamsteadEvidenceSource> It(World); It; ++It)
				{
					AGloamsteadEvidenceSource* Source = *It;
					if (!IsValid(Source)
						|| Source->GetWarningId() != Plan.WarningId
						|| Source->GetSupportId() != Line.SupportId
						|| !IGloamInteractable::Execute_CanInteract(Source, this))
					{
						continue;
					}
					if (AutoPlayApproach(Source->GetActorLocation()))
					{
						return true; // still walking to it
					}
					IGloamInteractable::Execute_Interact(Source, this);
					UE_LOG(LogTemp, Log, TEXT("GloamAutoPlay: read evidence %s (%d of %d needed)."),
						*Line.SupportId.ToString(), Required, Lines.Num());
					return true;
				}
			}
		}
	}

	// --- 2. Restore the place the warning names ------------------------------------------------
	if (RitualPlacement)
	{
		int32 PointIndex = PCG->FindNearestUnrestoredPointMatchingExperiencePlan(
			GetActorLocation(), Plan, /*SearchRadius*/ 6000.f);

		// Cycle I is not an authored ritual site: its lantern sits on BP_FirstLanternAnchor, and the
		// five site declarations the catalog binds are Cycles II-VI. So the plan-contract matcher
		// correctly finds nothing for the tutorial. Asking for the plan's ritual FORM does not
		// rescue it either, because Cycle I is the one authored plan that never sets
		// RequiredRitualType - Cycles II-VI all do (ExperienceCycleTypes.cpp:233,271,310,349,388)
		// and the tutorial declares only RequiredRestorationTags={"LanternPost"}. So when the plan
		// names no form, try each in turn and let ConfirmPlacement's own contract check decide: it
		// validates the point against the plan regardless, so guessing can only find a candidate,
		// never authorise a wrong one.
		if (PointIndex == INDEX_NONE)
		{
			// Sized to the sanctuary (~1500 units across), not to infinity: this finder derives a
			// cell radius from the search radius and triple-loops the spatial grid, so a huge value
			// is a hang, not a wider net.
			constexpr float SanctuaryRadius = 6000.f;
			static const ERitualType Forms[] = {
				ERitualType::LanternPost,  ERitualType::GardenBed,  ERitualType::PathPoint,
				ERitualType::MirrorPillar, ERitualType::BellShrine, ERitualType::AnchorStone,
			};
			if (Plan.RequiredRitualType != ERitualType::Invalid)
			{
				PointIndex = PCG->FindNearestUnrestoredPointIndex(
					GetActorLocation(), Plan.RequiredRitualType, SanctuaryRadius);
			}
			for (int32 i = 0; PointIndex == INDEX_NONE && i < UE_ARRAY_COUNT(Forms); ++i)
			{
				PointIndex = PCG->FindNearestUnrestoredPointIndex(
					GetActorLocation(), Forms[i], SanctuaryRadius);
			}
		}

		if (PointIndex != INDEX_NONE)
		{
			const FVector Where = PCG->GetRitualPointLocation(PointIndex);
			if (AutoPlayApproach(Where))
			{
				return true; // still walking to it
			}

			// The real two-beat verb: enter placement, then confirm what the preview resolved.
			// ConfirmPlacement needs the preview target the spatial grid supplies, so standing on
			// the point is not a shortcut around the contract - it is the precondition for it.
			RitualPlacement->EnterPlacementMode();
			const bool bRestored = RitualPlacement->ConfirmPlacement();
			UE_LOG(LogTemp, Log, TEXT("GloamAutoPlay: restore at point %d (%s) %s."),
				PointIndex, *GetRitualTypeDisplayName(PCG->GetRitualTypeAt(PointIndex)),
				bRestored ? TEXT("succeeded") : TEXT("was refused"));
			return bRestored;
		}
	}

	// --- 3. Commit the sharper reading, when the cycle offers one ------------------------------
	//
	// Deliberately the Insight, not a random one: a harness that picked arbitrarily would make the
	// dawn verdict a coin flip and the run unrepeatable. AVeilHeart re-derives the grade from the
	// plan regardless, so this cannot forge an advantage it did not earn.
	for (TActorIterator<AGloamsteadReadingChoice> It(World); It; ++It)
	{
		AGloamsteadReadingChoice* Choice = *It;
		if (!IsValid(Choice) || Choice->GetWarningId() != Plan.WarningId)
		{
			continue;
		}
		const FExperienceCycleSecondReading* Reading = Plan.FindSecondReading(Choice->GetReadingId());
		if (!Reading || Reading->Grade != EExperienceReadingGrade::Insight)
		{
			continue;
		}
		if (!IGloamInteractable::Execute_CanInteract(Choice, this))
		{
			continue;
		}
		if (AutoPlayApproach(Choice->GetActorLocation()))
		{
			return true; // still walking to it
		}
		IGloamInteractable::Execute_Interact(Choice, this);
		UE_LOG(LogTemp, Log, TEXT("GloamAutoPlay: committed the sharper reading %s."),
			*Choice->GetReadingId().ToString());
		return true;
	}

	return false;
}

void AGloamsteadCharacter::AutoPlayBeat()
{
	UWorld* World = GetWorld();
	UGloamsteadDayNightSubsystem* DayNight =
		World ? World->GetSubsystem<UGloamsteadDayNightSubsystem>() : nullptr;
	if (!DayNight)
	{
		return;
	}

	++AutoPlayBeats;

	// The harness presses past the opening screen exactly as a player does, rather than being
	// exempted from it - if the title could not be dismissed, that is a finding, not a detail.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AGloamsteadHUD* Readout = Cast<AGloamsteadHUD>(PC->GetHUD()))
		{
			if (Readout->IsTitlePending())
			{
				OnInteractInput();
				return;
			}
		}
	}

	// Beat 5, not BeginPlay: AGloamsteadSanctuaryBootstrap initialises the PCG state a frame or more
	// after the pawn exists, so a dump at start reported "0 ritual point(s)" and said nothing.
	if (AutoPlayBeats == 5)
	{
		if (const UGloamsteadPCGSubsystem* Points = World->GetSubsystem<UGloamsteadPCGSubsystem>())
		{
			const int32 Count = Points->GetRitualPointCount();
			UE_LOG(LogTemp, Log, TEXT("GloamAutoPlay: sanctuary holds %d ritual point(s)."), Count);
			for (int32 i = 0; i < Count; ++i)
			{
				const FVector At = Points->GetRitualPointLocation(i);
				UE_LOG(LogTemp, Log,
					TEXT("  point %d: type=%s restored=%s light=%.2f corruption=%.2f at (%.0f,%.0f,%.0f)"),
					i,
					*GetRitualTypeDisplayName(Points->GetRitualTypeAt(i)),
					Points->IsPointRestored(i) ? TEXT("yes") : TEXT("no"),
					Points->GetLightLevel(i), Points->GetCorruptionLevel(i), At.X, At.Y, At.Z);
			}
		}
	}

	// Photograph each phase the first time it is entered. The HUD is Canvas draw code, which the
	// automation suite is structurally blind to - it can assert what the readout HOLDS and never
	// that it was drawn. A screenshot is the only evidence that closes that gap, and the dawn panel
	// and the ending screen are precisely the two surfaces no one had ever seen.
	if (bAutoShots)
	{
		const EGloamsteadDayPhase Now = DayNight->GetCurrentPhase();
		if (Now != LastShotPhase)
		{
			LastShotPhase = Now;
			++AutoShotIndex;
			const FString Name = FString::Printf(TEXT("gloam_%02d_%s"),
				AutoShotIndex, *GetGloamsteadDayPhaseDisplayName(Now));
			// A short delay so the phase's first frame - the one carrying the new panel - is the
			// one captured, rather than the frame the transition happened on.
			FTimerHandle ShotTimer;
			World->GetTimerManager().SetTimer(ShotTimer, [Name]()
			{
				if (GEngine && GEngine->GameViewport)
				{
					FScreenshotRequest::RequestScreenshot(Name, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
				}
			}, 0.6f, false);
		}
	}

	const UGloamsteadExperienceCycleSubsystem* Cycles =
		World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UGloamsteadExperienceCycleSubsystem>()
			: nullptr;

	// Stop on the ending rather than on a beat budget, so the log says the arc was actually walked
	// to its end rather than that the harness ran out of patience.
	if (Cycles && Cycles->IsExperienceComplete())
	{
		UE_LOG(LogTemp, Log,
			TEXT("GloamAutoPlay: the authored experience is complete after %d beat(s)."), AutoPlayBeats);
		GloamStatus();
		World->GetTimerManager().ClearTimer(AutoPlayTimer);
		return;
	}

	// A ceiling so a genuinely stuck arc ends the run loudly instead of spinning until the process
	// is killed - which is the failure mode that made the last investigation cost three long boots.
	// Sized from the arc's own night budget rather than picked: the six authored nights now carry a
	// combined ceiling of ~1860s (NightDurationScaleForType x NightDurationSeconds), and a run that
	// rests instantly still spends a beat per phase change on top. Every earlier value was too
	// tight, and each time the harness reported "gave up" for an arc that had not failed - so this
	// is deliberately well clear of the ceiling rather than close to it.
	constexpr int32 MaxBeats = 2600;
	if (AutoPlayBeats > MaxBeats)
	{
		UE_LOG(LogTemp, Error,
			TEXT("GloamAutoPlay: gave up after %d beats - the arc did not reach its ending. "
				 "The last status line below is where it stopped."), AutoPlayBeats);
		GloamStatus();
		World->GetTimerManager().ClearTimer(AutoPlayTimer);
		return;
	}

	switch (DayNight->GetCurrentPhase())
	{
	case EGloamsteadDayPhase::Night:
		// Answer the night with the two verbs a player has. Ward first: it is the one that resolves
		// an objective, and Strike only ever buys seconds.
		OnWardInput();
		OnStrikeInput();
		break;

	case EGloamsteadDayPhase::Day:
		// Do the cycle's actual work before asking for the night. Resting first is what made every
		// earlier traversal resolve its nights with no target and heed no warning: the arc advanced
		// correctly and nothing in it had been played.
		if (AutoPlayDoDayWork())
		{
			break;
		}
		if (!DayNight->RequestRest() && AutoPlayBeats == 90)
		{
			// Ninety beats in Day with nothing left to do and rest still refused means the cycle's
			// own gate is holding. Say which, then open the first rest so the rest of the arc is
			// still exercised - and never silently, because a skipped Cycle I is a finding.
			UE_LOG(LogTemp, Error,
				TEXT("GloamAutoPlay: Cycle I never accepted a lantern restoration; opening the first "
					 "rest so the remaining cycles still run. The tutorial path is NOT verified by this run."));
			DayNight->UnlockFirstRest();
		}
		break;

	case EGloamsteadDayPhase::Dusk:
	case EGloamsteadDayPhase::Dawn:
	default:
		// Rest drives Day->Dusk, Dusk->Night and Dawn->Day alike. RequestRest refuses when the
		// cycle is not ready, and that refusal is the harness's honest stopping condition rather
		// than something to work around.
		if (!DayNight->RequestRest() && (AutoPlayBeats % 25) == 0)
		{
			// Periodic rather than per-beat: a refused rest is the normal state while a cycle's
			// work is outstanding, so saying it every second would bury the run in noise - but
			// saying it never is how a stuck arc looks identical to a working one.
			UE_LOG(LogTemp, Warning,
				TEXT("GloamAutoPlay: rest still refused at beat %d; this cycle has work outstanding."),
				AutoPlayBeats);
			GloamStatus();
		}
		break;
	}

	// One status line per beat is too noisy for a 600-beat ceiling; one per phase change is what a
	// reader actually wants, and the phase authority already logs its own transitions.
	if ((AutoPlayBeats % 10) == 0)
	{
		GloamStatus();
	}
}

