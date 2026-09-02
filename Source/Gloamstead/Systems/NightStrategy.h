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

	/** Called when the player deliberately wards the active threat with their light. */
	UFUNCTION(BlueprintNativeEvent, Category = "Night")
	bool NotifyLightWard(UGloamsteadPCGSubsystem* PCG);
	virtual bool NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG);

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

	/**
	 * Corruption removed by one light ward. Deliberately larger than PressureStepDelta: a ward that
	 * cannot out-pace a single beat of pressure is not a lever, and Corruption was the only threatened
	 * night with no ward at all - leaving the once-per-point restoration as its sole answer.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float WardCorruptionDelta = 0.12f;

	/** Tending the bloom is repeatable, but not free: each ward past the first gives a little less. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "0", ClampMax = "1"))
	float WardFalloffPerUse = 0.02f;

	virtual bool NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG) override;

private:
	int32 WardsUsed = 0;
};

/**
 * Tutorial night: a bounded teaching beat proving that night reacts to the sanctuary — and that the
 * lantern the player just restored is what makes the dark survivable.
 *
 * The lesson is a single readable objective: reach the restored lantern's light before dawn. The
 * lantern the player themselves raised is the shelter, so the restoration has a mechanical payoff on
 * the same night it happened rather than only a presentational one. Reaching it resolves the objective
 * and ends the night early (via the runtime's existing OnNightShouldEnd path); running out the clock
 * outside the light is a Partial, not a Failure — this is still a tutorial, and it stays winnable.
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

	/** How close to the restored lantern counts as "in its light". Matches the lantern's visible falloff. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night", meta = (ClampMin = "1"))
	float ShelterRadius = 900.0f;

	/** Actor tag the restoration stamps on the lantern it spawns (RitualPlacementComponent). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night")
	FName RestoredLanternTag = FName(TEXT("Gloamstead.RestoredLantern"));

	/** True once a restored lantern was found to shelter under; false means there is nothing to reach. */
	UFUNCTION(BlueprintPure, Category = "Night")
	bool HasShelter() const { return bHasShelter; }

	/** World position of the shelter the player must reach (only meaningful when HasShelter()). */
	UFUNCTION(BlueprintPure, Category = "Night")
	FVector GetShelterLocation() const { return ShelterLocation; }

	/** True once the player has stood in the restored lantern's light this night. */
	UFUNCTION(BlueprintPure, Category = "Night")
	bool IsPlayerSheltered() const { return bPlayerSheltered; }

	/** Evaluates player-in-light once; exposed so tests can drive it without a pressure timer. */
	bool EvaluateShelter();

private:
	bool bTeachingSpreadApplied = false;
	bool bHasShelter = false;
	bool bPlayerSheltered = false;
	FVector ShelterLocation = FVector::ZeroVector;
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

	/** Once pressure reaches this seam, the restoration is visibly reclaimed so the player can re-light it. */
	UPROPERTY(EditDefaultsOnly, Category = "Night|Retrieval")
	float RetrievalReclaimThreshold = 0.3f;

	bool bTargetReclaimed = false;
};

/**
 * Silence-possession night: a restored place becomes occupied instead of being attacked by a wave.
 * The player reads the stillness, brings light to the place, and performs two deliberate wards:
 * first to disrupt the hold, then to purify it. Ignoring it leaves a scar on the restored place.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API UNightPossessionStrategy : public UNightStrategy
{
	GENERATED_BODY()

public:
	virtual void EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG) override;
	virtual void ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual bool NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual FNightRuntimeOutcome ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG) override;

	/** Corruption added while the possessed place is still holding its silence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Possession", meta = (ClampMin = "0", ClampMax = "1"))
	float PossessionPressureDelta = 0.10f;

	/** Corruption removed when the first ward breaks the hold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Possession", meta = (ClampMin = "0", ClampMax = "1"))
	float DisruptionCorruptionDelta = 0.08f;

	/** Corruption removed by the second ward, which completes purification. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Possession", meta = (ClampMin = "0", ClampMax = "1"))
	float PurificationCorruptionDelta = 0.18f;

	/** Extra fail-forward scar when the player never brings light to the hold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Possession", meta = (ClampMin = "0", ClampMax = "1"))
	float PossessionScarDelta = 0.12f;

	/** True after the night has begun pressing on a real restored target. */
	UFUNCTION(BlueprintPure, Category = "Night|Possession")
	bool IsPossessionActive() const { return bPossessionActive; }

	/** True after the first light ward has disrupted the hold but before purification. */
	UFUNCTION(BlueprintPure, Category = "Night|Possession")
	bool IsPossessionDisrupted() const { return bPossessionDisrupted; }

	/** True when the authored target was absent, so the night intentionally stayed quiet. */
	UPROPERTY(BlueprintReadOnly, Category = "Night|Possession")
	bool bNoTargetFallback = false;

private:
	bool bPossessionActive = false;
	bool bPossessionDisrupted = false;
};

/**
 * Mirror/Bargain night: the restored garden reflects a tempting shortcut.
 * The player must read the evidence, choose to refuse the false path or accept
 * the bargain, and then use light to hold an accepted truth. There is no wave:
 * pressure is the mirror's growing corruption and the consequence of the choice.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API UNightMirrorStrategy : public UNightStrategy
{
	GENERATED_BODY()

public:
	virtual void EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG) override;
	virtual void ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual bool NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual FNightRuntimeOutcome ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG) override;

	/** Make the deliberate mirror choice. Returns false when no choice is pending. */
	UFUNCTION(BlueprintCallable, Category = "Night|Mirror")
	bool ChooseBargain(bool bAccept);

	UFUNCTION(BlueprintPure, Category = "Night|Mirror")
	bool IsChoicePending() const { return !bChoiceMade && !bNoTargetFallback; }

	UFUNCTION(BlueprintPure, Category = "Night|Mirror")
	bool HasChosenBargain() const { return bChoiceMade && bBargainAccepted; }

	UFUNCTION(BlueprintPure, Category = "Night|Mirror")
	bool IsBargainHeld() const { return bBargainHeld; }

	/** Corruption added while the player leaves the reflection unanswered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Mirror", meta = (ClampMin = "0", ClampMax = "1"))
	float UnansweredPressureDelta = 0.06f;

	/** Corruption added when the player accepts the bargain but has not held it with light. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Mirror", meta = (ClampMin = "0", ClampMax = "1"))
	float BargainPressureDelta = 0.04f;

	/** Corruption removed by the light ward that proves the accepted bargain is held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Mirror", meta = (ClampMin = "0", ClampMax = "1"))
	float BargainWardDelta = 0.18f;

	/** Fail-forward scar when the player accepts but never brings light to the mirror. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Mirror", meta = (ClampMin = "0", ClampMax = "1"))
	float BargainScarDelta = 0.12f;

	/** True when the authored target is absent, so the mirror stays quiet rather than selecting a substitute. */
	UPROPERTY(BlueprintReadOnly, Category = "Night|Mirror")
	bool bNoTargetFallback = false;

private:
	bool bChoiceMade = false;
	bool bBargainAccepted = false;
	bool bBargainHeld = false;
};


/**
 * Bargain night (Cycle V). The Bargainer stands at the edge of the light and offers a shortcut; the
 * restored bell is how the player refuses it.
 *
 * This is the first night whose answer is an ACTIVE tool rather than preparation, which is why the
 * warning's second clause is about restraint - "One answer frees; three answers invite company."
 * Ringing once on the answering beat (the Insight reading) makes a single resonance pulse enough and
 * relights what the night put out. Ringing three times (the Overreach) called something else, and
 * the night now needs answering twice.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API UNightBargainStrategy : public UNightStrategy
{
	GENERATED_BODY()

public:
	virtual void EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG) override;
	virtual void ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual bool NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual FNightRuntimeOutcome ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG) override;

	/** How many resonance answers this night still needs. Zero means the bargain is broken. */
	UFUNCTION(BlueprintPure, Category = "Night|Bargain")
	int32 GetAnswersRemaining() const { return FMath::Max(0, RequiredAnswers - AnswersGiven); }

	UFUNCTION(BlueprintPure, Category = "Night|Bargain")
	int32 GetRequiredAnswers() const { return RequiredAnswers; }

	/** Corruption added per step while the bargain stands unanswered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Bargain", meta = (ClampMin = "0", ClampMax = "1"))
	float BargainPressureDelta = 0.07f;

	/** Corruption removed by one resonance answer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Bargain", meta = (ClampMin = "0", ClampMax = "1"))
	float ResonanceWardDelta = 0.16f;

	/** Extra light the Insight reading's single clean answer restores across the sanctuary. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Bargain", meta = (ClampMin = "0", ClampMax = "1"))
	float ResonanceRelightDelta = 0.10f;

	/** Fail-forward scar when the bell is never answered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Bargain", meta = (ClampMin = "0", ClampMax = "1"))
	float UnansweredScarDelta = 0.14f;

	/** True when the authored bell shrine is absent or unrestored, so the night stays quiet. */
	UPROPERTY(BlueprintReadOnly, Category = "Night|Bargain")
	bool bNoTargetFallback = false;

private:
	int32 RequiredAnswers = 1;
	int32 AnswersGiven = 0;
};

/**
 * Fracture into True Siege (Cycle VI). The whole sanctuary is the objective.
 *
 * Nothing new is introduced here on purpose. The night pulls at the seams between everything the
 * player restored, and the only thing that decides how badly it goes is the shape they bound their
 * three anchors into the day before: a closed ring links their light and holds the seams shut, an
 * arc holds one flank, and a crown around the Heart collapses the outer sanctuary and funnels every
 * threat inward.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API UNightSiegeStrategy : public UNightStrategy
{
	GENERATED_BODY()

public:
	virtual void EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG) override;
	virtual void ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual bool NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG) override;
	virtual FNightRuntimeOutcome ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG) override;

	/** How many seams the siege has opened that the player has not yet closed. */
	UFUNCTION(BlueprintPure, Category = "Night|Siege")
	int32 GetOpenSeamCount() const { return OpenSeams; }

	/** Corruption added at the bound anchor per step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Siege", meta = (ClampMin = "0", ClampMax = "1"))
	float SeamPressureDelta = 0.09f;

	/** Corruption spread across the wider sanctuary per step when the ring does not hold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Siege", meta = (ClampMin = "0", ClampMax = "1"))
	float SanctuarySpreadDelta = 0.05f;

	/** How many points the spread touches per step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Siege", meta = (ClampMin = "0"))
	int32 SanctuarySpreadPoints = 4;

	/** Corruption removed at the anchor by one light ward, closing one seam. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Night|Siege", meta = (ClampMin = "0", ClampMax = "1"))
	float SeamWardDelta = 0.15f;

	/** True when no bound anchor exists, so the siege stays quiet rather than punishing a guess. */
	UPROPERTY(BlueprintReadOnly, Category = "Night|Siege")
	bool bNoTargetFallback = false;

private:
	/** Set from the second reading: a closed ring links the outer lights and suppresses the seams. */
	bool bRingHolds = false;
	/** Set from the second reading: a crown around the Heart funnels everything inward. */
	bool bCrownCollapsed = false;
	int32 OpenSeams = 0;
	int32 SeamsClosed = 0;
};
