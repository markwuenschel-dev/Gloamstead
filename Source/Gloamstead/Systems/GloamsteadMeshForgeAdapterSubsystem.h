#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "Data/GloamsteadMeshForgeTypes.h"
#include "Data/RitualTypes.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "GloamsteadMeshForgeAdapterSubsystem.generated.h"

class UGloamsteadMeshForgeProvider;
class UGloamsteadPCGSubsystem;
class AVeilHeart;

/**
 * Gloamstead MeshForge Adapter (Corrected Wave 6A).
 *
 * Observes the real gameplay sources (Veil Heart, PCG ritual points, day/night phase, restoration events),
 * binds read-only proxy specs to them, and drives a replaceable PROVIDER to render visible proxies so the
 * loop can be seen and hand-played in PIE. It NEVER takes authority: it only reads source state and reacts to
 * source delegates — it never restores points, advances phases, or begins nights. It emits an auditable
 * visibility/provenance report. The current provider is engine-primitive (code-owned, no generated assets).
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadMeshForgeAdapterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Build (or rebuild) visible proxies for the current sanctuary sources in this subsystem's world. */
	void BuildProxies();

	/** Aggregate the current proxies into a visibility/provenance report. */
	FGloamsteadMeshForgeVisibilityReport BuildVisibilityReport() const;

	/** Build + write the JSON reports under procedural/reports/gloamstead_meshforge. */
	bool EmitReport(FString& OutPrimaryPath) const;

	const TArray<FGloamsteadMeshForgeProxyInstance>& GetProxies() const { return Proxies; }
	int32 CountProxiesOfType(EGMFProxyType Type) const;
	UGloamsteadMeshForgeProvider* GetProvider() const { return Provider; }

	/** Test seam: run the full build against an explicit world without OnWorldBeginPlay. */
	void Test_BuildFor(UWorld* World);

private:
	void BuildFor(UWorld* World);
	void EnsureProvider();
	void ClearProxies();
	void BindSourceEvents(UWorld* World);
	void UnbindSourceEvents();

	AVeilHeart* FindHeart(UWorld* World) const;

	void BuildHeartProxy(UWorld* World, AVeilHeart* Heart);
	void BuildRitualPointProxies(UWorld* World, UGloamsteadPCGSubsystem* PCG);
	void BuildInteractionRadiusProxy(UWorld* World, AVeilHeart* Heart);
	void BuildNightFeedbackProxy(UWorld* World, AVeilHeart* Heart);

	UFUNCTION()
	void HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase);

	UFUNCTION()
	void HandleStructureRestored(const FRestorationEventPayload& Payload);

	/** Deferred retry: PCG points may generate after BeginPlay; rebuild until ritual proxies appear. */
	void RetryRitualProxies();

	UPROPERTY()
	TObjectPtr<UGloamsteadMeshForgeProvider> Provider;

	UPROPERTY()
	TArray<FGloamsteadMeshForgeProxyInstance> Proxies;

	int32 NightFeedbackProxyIndex = -1;
	int32 RebuildAttempts = 0;
	bool bBound = false;

	FTimerHandle RebuildTimer;
};
