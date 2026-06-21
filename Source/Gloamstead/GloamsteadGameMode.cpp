// Copyright Epic Games, Inc. All Rights Reserved.

#include "GloamsteadGameMode.h"
#include "GloamsteadPlayerController.h"

AGloamsteadGameMode::AGloamsteadGameMode()
{
	// Default to the Gloamstead player controller. The default pawn is left to the concrete
	// BP_GloamsteadGameMode (a Blueprint subclass of AGloamsteadCharacter), since C++ must not
	// reference content assets directly.
	PlayerControllerClass = AGloamsteadPlayerController::StaticClass();
}
