#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "GloamsteadFirstNightDirector.generated.h"

class UGloamsteadPCGSubsystem;
class UNightConsequenceRuntime;
class AVeilHeart;

/** Ordered beats of the scripted first-night proof-of-loop. */
UENUM(BlueprintType)
enum class EFirstNightBeat : uint8
{
	/** Day: warning shown + lantern target readable, awaiting restoration. */
	Intro,
	/** Lantern restored; advancing into dusk. */
	LanternRestored,
	/** Dusk readability/preparation cue. */
	Dusk,
	/** Night: scripted encroachment underway, tested by lantern influence. */
	Night,
	/** Dawn payoff + Heart confirmation. */
	Dawn,
	/** First-night loop finished; director is dormant. */
	Complete,
};

/**
 * Thin first-night sequencer for the proof-of-loop vertical slice.
 *
 * Owns ONLY first-night beat sequencing and presentation triggers. It listens to existing
 * systems (PCG restoration, day/night phase, night runtime) and drives the phase authority
 * forward via AdvanceToNextPhase. Dusk is gated behind lantern restoration: if the lantern
 * is never restored, night never begins.
 *
 * It does NOT replace UGloamsteadDayNightSubsystem / UNightConsequenceManager, does NOT mutate
 * PCG point state, and does NOT own generic night selection, combat, resources, or progression.
 * Presentation (caption, prompts, cues, VFX, dawn payoff) lives in the Blueprint child via the
 * On* implementable events below.
 */
UCLASS(Blueprintable, BlueprintType)
class GLOAMSTEAD_API AGloamsteadFirstNightDirector : public AActor
{
	GENERATED_BODY()

public:
	AGloamsteadFirstNightDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// === Designer config ===

	/** Restoration that satisfies the first-night lesson and unlocks dusk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Night")
	ERitualType RequiredRestorationType = ERitualType::LanternPost;

	/** Night type the slice expects (kept Tutorial for the proof-of-loop). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Night")
	ENightConsequenceType FirstNightType = ENightConsequenceType::Tutorial;

	/** Seconds the dusk readability cue holds before night begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Night", meta = (ClampMin = "0.0"))
	float DuskToNightDelaySeconds = 4.0f;

	/** Seconds the scripted encroachment runs before the director calls dawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Night", meta = (ClampMin = "0.0"))
	float NightDurationSeconds = 8.0f;

	// === State queries ===

	UFUNCTION(BlueprintPure, Category = "First Night")
	EFirstNightBeat GetCurrentBeat() const { return CurrentBeat; }

	UFUNCTION(BlueprintPure, Category = "First Night")
	bool IsLanternRestored() const { return bLanternRestored; }

	UFUNCTION(BlueprintPure, Category = "First Night")
	ENightConsequenceType GetObservedNightType() const { return ObservedNightType; }

	// === Presentation hooks (implemented in the Blueprint child) ===

	/** Day: the Heart's warning about the lost path should be shown/captioned. */
	UFUNCTION(BlueprintImplementableEvent, Category = "First Night")
	void OnFirstNightWarning();

	/** Day: the lantern restoration target should become readable to the player. */
	UFUNCTION(BlueprintImplementableEvent, Category = "First Night")
	void OnLanternTargetReadable();

	/** The player restored the lantern; play the confirmation beat. */
	UFUNCTION(BlueprintImplementableEvent, Category = "First Night")
	void OnLanternRestored(const FVector& Location);

	/** Dusk: short preparation/readability cue. */
	UFUNCTION(BlueprintImplementableEvent, Category = "First Night")
	void OnDuskCue();

	/** Night: scripted encroachment begins; LanternInfluence (0..1) is the path's restored light. */
	UFUNCTION(BlueprintImplementableEvent, Category = "First Night")
	void OnEncroachmentBegan(float LanternInfluence);

	/** Dawn: world payoff and Heart confirmation of the warning/restoration relationship. */
	UFUNCTION(BlueprintImplementableEvent, Category = "First Night")
	void OnDawnPayoff();

	// === Beat advances (timer-driven in play; also callable by presentation BP) ===

	/** Day -> Dusk. No-op unless the lantern has been restored (the dusk gate). */
	UFUNCTION(BlueprintCallable, Category = "First Night")
	void RequestAdvanceToDusk();

	/** Dusk -> Night. */
	UFUNCTION(BlueprintCallable, Category = "First Night")
	void RequestAdvanceToNight();

	/** Night -> Dawn. */
	UFUNCTION(BlueprintCallable, Category = "First Night")
	void RequestAdvanceToDawn();

	// === Delegate handlers (UFUNCTION for AddDynamic; also exercised directly by tests) ===

	UFUNCTION()
	void HandleStructureRestored(const FRestorationEventPayload& Payload);

	UFUNCTION()
	void HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase);

	UFUNCTION()
	void HandleNightStarted(ENightConsequenceType NightType);

	// === Test seams (unconditional inline; unused in shipping → linker drops them) ===

	/** Inject already-constructed subsystems and run the real binding path. */
	void Test_BindTo(UGloamsteadPCGSubsystem* InPCG, UGloamsteadDayNightSubsystem* InDayNight, UNightConsequenceRuntime* InRuntime);

	/** Present the Day intro beat (warning + lantern target) without a live world. */
	void Test_BeginIntro() { BeginIntro(); }

	int32 Test_WarningCount = 0;
	int32 Test_LanternTargetCount = 0;
	int32 Test_LanternRestoredCount = 0;
	int32 Test_DuskCueCount = 0;
	int32 Test_EncroachmentCount = 0;
	int32 Test_DawnPayoffCount = 0;

protected:
	void ResolveWorldSystems();
	void BindDelegates();
	void UnbindDelegates();

	void BeginIntro();

	void PresentWarning();
	void PresentLanternTarget();
	void PresentDuskCue();
	void PresentEncroachment();
	void PresentDawnPayoff();

	/** Read-only sanctuary light level used to scale the encroachment cue. */
	float ComputeLanternInfluence() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadPCGSubsystem> CachedPCG;

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadDayNightSubsystem> CachedDayNight;

	UPROPERTY(Transient)
	TObjectPtr<UNightConsequenceRuntime> CachedRuntime;

	UPROPERTY(Transient)
	TWeakObjectPtr<AVeilHeart> CachedHeart;

	EFirstNightBeat CurrentBeat = EFirstNightBeat::Intro;
	bool bLanternRestored = false;
	bool bIntroPresented = false;
	bool bDelegatesBound = false;
	ENightConsequenceType ObservedNightType = ENightConsequenceType::Invalid;

	FTimerHandle DuskToNightTimer;
	FTimerHandle NightDurationTimer;
};
