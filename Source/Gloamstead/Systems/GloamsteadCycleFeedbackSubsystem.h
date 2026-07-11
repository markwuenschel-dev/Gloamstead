#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "GloamsteadCycleFeedbackSubsystem.generated.h"

/**
 * Playable Cycle I (Wave 5A): a minimal, content-free player-facing feedback layer.
 *
 * The whole day->dusk->night->dawn cycle already emits Blueprint hooks + UE_LOG, but in PIE a player with
 * no bound HUD would see nothing legible. This world subsystem binds the existing runtime delegates and
 * surfaces each beat as on-screen debug text so the loop is readable WITHOUT authoring any widget/content:
 * phase changes, the dusk warning's night type, the night starting, and — the payoff — the dawn outcome
 * (Success / Partial / Failure), colour-coded. It is deliberately a debug surface, not a HUD; a real widget
 * can later bind the same delegates. The message formatting is pure and unit-tested; the on-screen draw is a
 * no-op outside a game world (and where GEngine is unavailable), so automation stays clean.
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadCycleFeedbackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// ---- Pure formatters (unit-tested; no engine state) ----
	static FString FormatPhase(EGloamsteadDayPhase NewPhase);
	static FString FormatNightStart(ENightConsequenceType NightType);
	static FString FormatOutcome(const FNightRuntimeOutcome& Outcome);
	static FColor OutcomeColor(ENightOutcomeResult Result);

private:
	UFUNCTION()
	void HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase);

	UFUNCTION()
	void HandleNightStarted(ENightConsequenceType NightType);

	UFUNCTION()
	void HandleNightEnded(ENightConsequenceType NightType);

	UFUNCTION()
	void HandleOmenClueReady(FName ClueTag);

	/** Draw one on-screen debug line (no-op outside a game world / when GEngine is null). */
	void Show(int32 Key, float Duration, const FColor& Color, const FString& Text) const;

	bool bBound = false;
};
