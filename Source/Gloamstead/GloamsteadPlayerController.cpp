// Copyright Epic Games, Inc. All Rights Reserved.


#include "GloamsteadPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Gloamstead.h"
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
}

bool AGloamsteadPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
