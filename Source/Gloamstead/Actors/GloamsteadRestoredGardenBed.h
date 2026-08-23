#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GloamsteadRestoredGardenBed.generated.h"

class UStaticMeshComponent;

/**
 * Code-owned fallback for the Cycle II GardenBed restoration.
 *
 * Designers may replace this class per character, but the authored GardenBed
 * must never consume its PCG point and leave the player with no visible change.
 * It deliberately references existing Gloamstead kit assets rather than
 * manufacturing an untracked binary asset during runtime development.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API AGloamsteadRestoredGardenBed : public AActor
{
	GENERATED_BODY()

public:
	AGloamsteadRestoredGardenBed();

	/** True when the fallback has a renderable project-owned garden surface. */
	UFUNCTION(BlueprintPure, Category = "Gloamstead|Restoration")
	bool HasVisibleGardenMesh() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gloamstead|Restoration")
	TObjectPtr<UStaticMeshComponent> GardenMesh;
};
