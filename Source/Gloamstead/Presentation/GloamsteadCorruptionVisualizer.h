#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GloamsteadCorruptionVisualizer.generated.h"

class UGloamsteadPCGSubsystem;
class UDecalComponent;
class AActor;

/**
 * Makes corruption visible.
 *
 * Corruption was fully simulated and completely invisible: NightStrategy drove FRitualPointState::
 * CorruptionLevel every pressure beat, the logs narrated a bloom climbing to 1.00, and the world looked
 * identical throughout. The only visual reader in the project (the MeshForge proxy adapter) is gated off
 * by default as a diagnostic overlay, and WorldForge's MPC render mirror is inert because the collection
 * asset it wants has never existed. So a player was asked to answer a threat with no presence.
 *
 * This binds the authored decal art that shipped with no consumer at all
 * (Content/Gloamstead/Kit/Decals) to the corruption state, one decal per corrupted ritual point, and
 * grows it as the bloom grows. It is presentation only: it reads state and never writes it.
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadCorruptionVisualizer : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Corruption at or below this leaves a point clean. Below it there is nothing to show. */
	UPROPERTY(EditAnywhere, Category = "Gloamstead|Corruption", meta = (ClampMin = "0", ClampMax = "1"))
	float VisibleThreshold = 0.15f;

	/** Decal footprint at the threshold, and at full corruption. The stain spreads as the bloom does. */
	UPROPERTY(EditAnywhere, Category = "Gloamstead|Corruption", meta = (ClampMin = "1"))
	float MinRadius = 260.f;

	UPROPERTY(EditAnywhere, Category = "Gloamstead|Corruption", meta = (ClampMin = "1"))
	float MaxRadius = 800.f;

	/** Test seam: how many decals are currently showing a bloom. */
	int32 Test_GetVisibleDecalCount() const;

private:
	UFUNCTION()
	void HandleCorruptionChanged();

	void RefreshAll();

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadPCGSubsystem> CachedPCG;

	/** One decal per ritual point index; entries stay allocated and are hidden when a point is clean. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> PointDecals;

	UPROPERTY(Transient)
	TObjectPtr<AActor> DecalHolder;

	FDelegateHandle CorruptionChangedHandle;

	/**
	 * PCG publishes its points from the sanctuary bootstrap's BeginPlay, which can land after this
	 * subsystem's OnWorldBeginPlay. Binding the rebuild notice makes the first paint order-independent
	 * instead of silently drawing nothing because the point list was still empty.
	 */
	FDelegateHandle StateRebuiltHandle;
	bool bBound = false;

	/** Last reported number of visibly-blooming points, so the log marks changes, not every beat. */
	int32 LastShownCount = -1;
};
