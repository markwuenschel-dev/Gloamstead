// Copyright Epic Games, Inc. All Rights Reserved.

#include "GloamsteadGameMode.h"
#include "GloamsteadPlayerController.h"
#include "UI/GloamsteadHUD.h"

AGloamsteadGameMode::AGloamsteadGameMode()
{
	// Default to the Gloamstead player controller. The default pawn is left to the concrete
	// BP_GloamsteadGameMode (a Blueprint subclass of AGloamsteadCharacter), since C++ must not
	// reference content assets directly.
	PlayerControllerClass = AGloamsteadPlayerController::StaticClass();

	// The sanctuary readout. Set here rather than on a Blueprint game mode because the shipped game
	// mode Blueprint overrides only its default pawn - so a HUD chosen in C++ is a HUD every player
	// actually gets, including the one the redirected template Blueprint spawns.
	HUDClass = AGloamsteadHUD::StaticClass();
}
