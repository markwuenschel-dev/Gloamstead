#pragma once

#include "CoreMinimal.h"
#include "CombatEnemy.h"
#include "Data/NightThreatTypes.h"
#include "GloamsteadNightThreat.generated.h"

class UGloamsteadPCGSubsystem;

/** Where a threat is in its own night, independent of HP. */
UENUM(BlueprintType)
enum class ENightThreatState : uint8
{
	/** Walking toward what it wants. */
	Approaching = 0,
	/** Standing in enough light that it cannot advance. */
	Repelled    = 1,
	/** At its target and actively taking the light out of it. */
	Working     = 2,
	/** Interrupted; it must re-approach before it can work again. */
	Disrupted   = 3,
	/** Answered - cleansed, dismissed, or run out of night. It no longer applies pressure. */
	Resolved    = 4,
};

GLOAMSTEAD_API FString GetNightThreatStateDisplayName(ENightThreatState State);

/**
 * One light-vulnerable night threat.
 *
 * It builds on ACombatEnemy so a threat is a real damageable, strikeable thing rather than a second
 * parallel combat system - that reuse is the project's locked combat decision. What it deliberately
 * does NOT reuse is the Combat StateTree: these threats want restored structures, not the player,
 * and their whole behaviour is the few rules below. Driving that from C++ keeps it testable headless
 * and keeps the light relationship - the part that ties combat to the restoration fantasy - in one
 * readable place instead of spread across an asset graph.
 *
 * The rules, in full:
 *   - It walks toward what its archetype wants, at a speed scaled by how dark the ground is.
 *   - Light at or above its authored threshold stops it outright, and enough light pushes it back.
 *   - Reaching its target, it works: draining that point's light until the point goes dark.
 *   - Strike disrupts it. For the Borrowed, only Cleanse resolves it, and only once exposed.
 *
 * Nothing here is a damage race, and nothing here requires the player to win a fight. Every threat
 * is answerable with light the player already built.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API AGloamsteadNightThreat : public ACombatEnemy
{
	GENERATED_BODY()

public:
	AGloamsteadNightThreat();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Any damage interrupts the threat.
	 *
	 * This is how Strike enters the loop without inventing a second combat verb: the player already
	 * has a way to hit things, and hitting a threat buys the seconds needed to reach the light or the
	 * mirror. It deliberately does not resolve anything - Cleanse does that, and only Cleanse.
	 */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Called once by the night runtime immediately after spawn. */
	void ConfigureThreat(const FNightThreatSpec& InSpec, int32 InBoundPointIndex);

	UFUNCTION(BlueprintPure, Category = "Night|Threat")
	ENightThreatArchetype GetArchetype() const { return Spec.Archetype; }

	UFUNCTION(BlueprintPure, Category = "Night|Threat")
	ENightThreatState GetThreatState() const { return ThreatState; }

	UFUNCTION(BlueprintPure, Category = "Night|Threat")
	int32 GetBoundPointIndex() const { return BoundPointIndex; }

	/** How much light this threat has taken out of its bound point so far. */
	UFUNCTION(BlueprintPure, Category = "Night|Threat")
	float GetLightDrained() const { return LightDrained; }

	/**
	 * Exposes The Borrowed's tether. Set by the night when a correctly faced mirror is standing;
	 * without it, Cleanse cannot resolve a Borrowed at all.
	 */
	void SetTetherExposed(bool bExposed) { bTetherExposed = bExposed; }

	UFUNCTION(BlueprintPure, Category = "Night|Threat")
	bool IsTetherExposed() const { return bTetherExposed; }

	/** Interrupts the threat's work. Always available; never sufficient on its own. */
	UFUNCTION(BlueprintCallable, Category = "Night|Threat")
	bool Disrupt();

	/**
	 * The answer. What counts as an answer differs per archetype, which is the point of having
	 * archetypes: a Gatherer is pushed back by light, a Borrowed needs its tether exposed first, a
	 * Bargainer is dismissed by resonance, and an Echo simply stops being repeated at.
	 */
	UFUNCTION(BlueprintCallable, Category = "Night|Threat")
	bool Cleanse();

	/**
	 * The pure decision at the centre of every threat, kept free of world access so it can be
	 * exhaustively unit-tested: given how much light is on this threat and what holds it off, how
	 * fast does it get to move? 1.0 is full speed in the dark, 0.0 is pinned.
	 */
	static float ComputeAdvanceScale(float LightOnThreat, float RepelledAtLightLevel);

	/** True when this much light is enough to hold this threat off entirely. */
	static bool IsRepelledByLight(float LightOnThreat, float RepelledAtLightLevel);

	// === Test seams: drive the whole behaviour with no navmesh, mesh, or timer. ===
	void Test_SetThreatState(ENightThreatState InState) { ThreatState = InState; }
	void Test_StepBehaviour(float DeltaSeconds, float LightOnThreat, float DistanceToTarget)
	{
		StepBehaviour(DeltaSeconds, LightOnThreat, DistanceToTarget, /*PCG*/ nullptr);
	}

	/** How close the threat must be to its target before it can start working. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night|Threat", meta = (ClampMin = "1.0"))
	float WorkingRadius = 250.0f;

	/** Full-speed approach, in cm/s, before the light scale is applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night|Threat", meta = (ClampMin = "0.0"))
	float ApproachSpeed = 180.0f;

	/** Light removed from the bound point per second while working. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night|Threat", meta = (ClampMin = "0.0"))
	float LightDrainPerSecond = 0.06f;

	/** Seconds a disruption keeps the threat off its work. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night|Threat", meta = (ClampMin = "0.0"))
	float DisruptionSeconds = 3.0f;

protected:
	virtual void BeginPlay() override;

private:
	/** The whole behaviour, with every world read already done by the caller. */
	void StepBehaviour(float DeltaSeconds, float LightOnThreat, float DistanceToTarget, UGloamsteadPCGSubsystem* PCG);

	/** Resolves this threat's destination from its target preference, or false when it has none. */
	bool ResolveTargetLocation(const UGloamsteadPCGSubsystem* PCG, FVector& OutLocation, int32& OutPointIndex) const;

	UPROPERTY()
	FNightThreatSpec Spec;

	UPROPERTY()
	ENightThreatState ThreatState = ENightThreatState::Approaching;

	UPROPERTY()
	int32 BoundPointIndex = INDEX_NONE;

	UPROPERTY()
	float LightDrained = 0.0f;

	UPROPERTY()
	bool bTetherExposed = false;

	float DisruptionRemaining = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadPCGSubsystem> CachedPCG = nullptr;
};
