// Copyright Epic Games, Inc. All Rights Reserved.


#include "GloamsteadPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Gloamstead.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AGloamsteadPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogGloamstead, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	CreatePromptWidget();
}

void AGloamsteadPlayerController::CreatePromptWidget()
{
	if (!IsLocalPlayerController() || PromptWidget)
	{
		return;
	}

	UClass* WidgetClass = PromptWidgetClass;
	if (!WidgetClass && bUseProjectDefaultPromptWidget)
	{
		// Mirrors the lantern/preview fallback: the slice ships one project-owned prompt widget, so
		// the affordance survives a controller Blueprint that never assigned the slot.
		static const TCHAR* PromptPath = TEXT("/Game/Gloamstead/UI/WBP_GloamPrompt.WBP_GloamPrompt_C");
		WidgetClass = LoadClass<UUserWidget>(nullptr, PromptPath);
	}
	if (!WidgetClass)
	{
		UE_LOG(LogGloamstead, Warning, TEXT("No prompt widget class resolved; interaction prompts will be invisible."));
		return;
	}

	PromptWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	if (PromptWidget)
	{
		PromptWidget->AddToPlayerScreen(10);
	}
	else
	{
		UE_LOG(LogGloamstead, Warning, TEXT("Could not spawn the Gloamstead prompt widget."));
	}
}

void AGloamsteadPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}

	// Escape holds the sanctuary. Bound at key level rather than through an input-mapping asset for
	// the same reason the night verbs are: no action exists for it in IMC_Default, and a game the
	// player cannot stop is not finished.
	//
	// bExecuteWhenPaused is the whole trick. Without it the binding stops firing the instant it
	// succeeds, so the pause key can hold the sanctuary and then cannot release it.
	if (InputComponent)
	{
		FInputKeyBinding& PauseBinding = InputComponent->BindKey(
			EKeys::Escape, IE_Pressed, this, &AGloamsteadPlayerController::ToggleSanctuaryPause);
		PauseBinding.bExecuteWhenPaused = true;
		SetTickableWhenPaused(true);
	}
}

void AGloamsteadPlayerController::ToggleSanctuaryPause()
{
	bSanctuaryPaused = !bSanctuaryPaused;

	// SetGamePaused refuses in some contexts (a dedicated server, a world mid-teardown). Believe the
	// world rather than the request, so the overlay can never claim a pause that did not happen.
	UGameplayStatics::SetGamePaused(this, bSanctuaryPaused);
	bSanctuaryPaused = UGameplayStatics::IsGamePaused(this);

	bShowMouseCursor = bSanctuaryPaused;
	if (bSanctuaryPaused)
	{
		SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}

	UE_LOG(LogTemp, Log, TEXT("GloamInput: the sanctuary is %s."),
		bSanctuaryPaused ? TEXT("held") : TEXT("released"));
}

bool AGloamsteadPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
