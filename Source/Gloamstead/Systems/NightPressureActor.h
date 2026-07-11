#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightPressureActor.generated.h"

class UGloamsteadPCGSubsystem;

/**
 * Optional "feel" layer for the hybrid night (Corrected Wave 2): a minimal, light-reactive pressure
 * presence spawned during a threat night. It has NO combat/AI/navmesh — its menace scales inversely
 * with sanctuary light, so restoring/holding the light visibly weakens it. Purely cosmetic pressure:
 * the objective + outcome live in the runtime/strategy and are proven headlessly. This actor is only
 * spawned in a game world (never during automation tests).
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API ANightPressureActor : public AActor
{
	GENERATED_BODY()

public:
	ANightPressureActor();

	virtual void Tick(float DeltaSeconds) override;

	/** Pure: menace scales inversely with sanctuary light (bright sanctuary = weak menace). */
	UFUNCTION(BlueprintPure, Category = "Night")
	static float ComputeMenaceFromLight(float AverageLight);

	UFUNCTION(BlueprintPure, Category = "Night")
	float GetCurrentMenace() const { return CurrentMenace; }

	/** BP hook for VFX/audio when menace changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Night")
	void OnMenaceChanged(float NewMenace);

	/** The bloom point this pressure is bound to (-1 if unbound). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night")
	int32 BoundPointIndex = -1;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadPCGSubsystem> CachedPCG;

	float CurrentMenace = -1.f;
};
