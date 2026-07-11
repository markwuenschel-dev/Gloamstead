#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/RitualTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "NightStrategy.generated.h"

class UGloamsteadPCGSubsystem;

/**
 * Per-night-type behavior for the night consequence runtime (Corrected Wave 2).
 *
 * The runtime owns the night lifecycle (begin / pressure cadence / restoration events / end);
 * a strategy owns *what this night does*. Blueprintable + BlueprintNativeEvent so designers can
 * subclass and override any beat without touching the loop, and future night types are new
 * strategies rather than new branches in the runtime.
 */
// Non-abstract: the base is also the benign "quiet night" strategy used for unsupported night types.
UCLASS(Blueprintable)
class GLOAMSTEAD_API UNightStrategy : public UObject
{
	GENERATED_BODY()

public:
	/** Called once at night start with the captured context. Base builds a benign (no-op) objective. */
	UFUNCTION(BlueprintNativeEvent, Category = "Night")
	void EnterNight(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG);
	virtual void EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG);

	/** Called on a repeating cadence during the night to apply escalating pressure. */
	UFUNCTION(BlueprintNativeEvent, Category = "Night")
	void ApplyPressureStep(UGloamsteadPCGSubsystem* PCG);
	virtual void ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG);

	/** Called whenever a structure is restored during the night; may resolve the objective. */
	UFUNCTION(BlueprintNativeEvent, Category = "Night")
	void NotifyRestoration(const FRestorationEventPayload& Payload, UGloamsteadPCGSubsystem* PCG);
	virtual void NotifyRestoration_Implementation(const FRestorationEventPayload& Payload, UGloamsteadPCGSubsystem* PCG);

	/** Called once at night end to compute the outcome from final state. */
	UFUNCTION(BlueprintNativeEvent, Category = "Night")
	FNightRuntimeOutcome ResolveNight(UGloamsteadPCGSubsystem* PCG);
	virtual FNightRuntimeOutcome ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG);

	UFUNCTION(BlueprintPure, Category = "Night")
	bool IsObjectiveResolved() const { return Objective.bResolved; }

	UFUNCTION(BlueprintPure, Category = "Night")
	FNightObjective GetObjective() const { return Objective; }

	UFUNCTION(BlueprintPure, Category = "Night")
	FNightRuntimeContext GetStrategyContext() const { return Context; }

protected:
	/** Current sanctuary-average corruption (0 if no PCG). */
	static float SafeAvgCorruption(UGloamsteadPCGSubsystem* PCG);

	/** Fills the common fields of an outcome (type/objective/warning/sanctuary delta). Caller sets Result + tags. */
	FNightRuntimeOutcome MakeBaseOutcome(UGloamsteadPCGSubsystem* PCG) const;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	FNightRuntimeContext Context;

	UPROPERTY(BlueprintReadOnly, Category = "Night")
	FNightObjective Objective;

	/** Sanctuary-average corruption captured at EnterNight (for outcome delta). */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	float StartAvgCorruption = 0.f;
};

/**
 * Corruption night: an escalating corruption bloom the player must cleanse (restore the target point)
 * before dawn. Success = cleansed; Partial = reduced but not cleared; Failure = untouched/worsened.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API UNightCorruptionStrategy : public UNightStrategy
{
	GENERATED_BODY()

public:
	virtual void EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG) override;
	virtual void ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual void NotifyRestoration_Implementation(const FRestorationEventPayload& Payload, UGloamsteadPCGSubsystem* PCG) override;
	virtual FNightRuntimeOutcome ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG) override;

	/** Corruption added to the bloom each pressure step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float PressureStepDelta = 0.08f;

	/** Extra corruption spread to nearby points each step (the bloom growing). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float SpreadStepDelta = 0.03f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "1"))
	int32 SpreadStepPoints = 3;
};

/**
 * Tutorial night: a bounded, always-winnable teaching beat proving that night reacts to the sanctuary.
 * Applies a single gentle spread and always resolves Success.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API UNightTutorialStrategy : public UNightStrategy
{
	GENERATED_BODY()

public:
	virtual void EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG) override;
	virtual void ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual FNightRuntimeOutcome ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float TeachingSpreadDelta = 0.06f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "1"))
	int32 TeachingSpreadPoints = 4;

private:
	bool bTeachingSpreadApplied = false;
};

/**
 * Omen night (Night Types II): information/vulnerability consequence. An omen marks a vulnerable point;
 * the player must interpret the sign and restore that point before dawn. Non-combat — the pressure is
 * interpretive, and an ignored omen deepens into a corruption seed rather than an attack.
 *   Success (OmenHeeded)  — the omen target is restored and its corruption reduced (the sign understood).
 *   Partial (OmenClouded) — the player acted (restored some point) but not the omen target (read the region, not the sign).
 *   Failure (OmenIgnored) — no action; the omen deepens into a corruption seed on the marked point.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API UNightOmenStrategy : public UNightStrategy
{
	GENERATED_BODY()

public:
	virtual void EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG) override;
	virtual void ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual void NotifyRestoration_Implementation(const FRestorationEventPayload& Payload, UGloamsteadPCGSubsystem* PCG) override;
	virtual FNightRuntimeOutcome ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG) override;

	/** Corruption the omen accretes on its marked point each step (the sign deepening if unheeded). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float OmenDeepenDelta = 0.05f;

private:
	/** Any restoration seen this night (used to distinguish "acted, but not the sign" partials). */
	bool bSawRestoration = false;
};

/**
 * Retrieval night (Night Types II): the night reacts to what the player restored — it targets a restored
 * point and tries to reclaim it. The player must re-stabilize (cleanse) the point before dawn.
 *   Success (RetrievalRepelled) — the target is re-stabilized (corruption driven back below its start); stays restored.
 *   Partial (RetrievalSeam)     — the player intervened but the point survives scarred (still restored, more corrupt).
 *   Failure (RetrievalReclaimed)— no intervention; the night reclaims the point (restoration reverted, fail-forward).
 * If nothing is restored there is nothing to reclaim: an honest quiet no-target fallback (RetrievalNoTarget).
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API UNightRetrievalStrategy : public UNightStrategy
{
	GENERATED_BODY()

public:
	virtual void EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG) override;
	virtual void ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual void NotifyRestoration_Implementation(const FRestorationEventPayload& Payload, UGloamsteadPCGSubsystem* PCG) override;
	virtual FNightRuntimeOutcome ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG) override;

	/** Corruption the retrieval pressure gnaws onto the restored target each step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float RetrievalPressureDelta = 0.1f;

	/** True when no restored point exists to reclaim — the night falls back to a benign quiet night. */
	UPROPERTY(BlueprintReadOnly, Category = "Night")
	bool bNoTargetFallback = false;

private:
	/** The player intervened on the retrieval target itself (restored it) at least once. */
	bool bSawTargetIntervention = false;
};
