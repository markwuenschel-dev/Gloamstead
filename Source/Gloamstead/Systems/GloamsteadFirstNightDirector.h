#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "GloamsteadFirstNightDirector.generated.h"

class UGloamsteadPCGSubsystem;
class UNightConsequenceRuntime;
class AVeilHeart;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USceneComponent;
class USkyLightComponent;

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
	virtual void Tick(float DeltaSeconds) override;
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
	float DuskToNightDelaySeconds = 6.0f;

	/**
	 * Seconds the scripted encroachment runs before the director calls dawn.
	 *
	 * This is the tutorial night's deadline, so it must be long enough for the player to actually cross
	 * the plaza to the lantern they restored — the old 8s was a log-driven timing, not a walkable one.
	 * Reaching the light resolves the objective and ends the night early, so this is an upper bound.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Night", meta = (ClampMin = "0.0"))
	float NightDurationSeconds = 45.0f;

	/** Seconds used to restore every captured sky value before the completion caption. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Night|Presentation", meta = (ClampMin = "0.0"))
	float DawnTransitionSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Night|Presentation")
	FLinearColor DuskSunTint = FLinearColor(1.0f, 0.72f, 0.46f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Night|Presentation")
	FLinearColor NightAmbientTint = FLinearColor(0.24f, 0.34f, 0.58f);

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

	/**
	 * Dusk: the Heart's actual warning text, ready to caption.
	 *
	 * The Heart chooses the words from its catalog but owns no UI; the director owns the caption widget
	 * but not the words. This carries one to the other, so the dusk warning is finally readable on
	 * screen instead of only in the log.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "First Night")
	void OnHeartWarning(const FText& WarningText);

	/** Dawn: the reflection tying the warning, the restoration, and the night's outcome together. */
	UFUNCTION(BlueprintImplementableEvent, Category = "First Night")
	void OnHeartReflection(const FText& ReflectionText);

	/** The text OnHeartReflection is given; exposed so the wording is testable without a widget. */
	UFUNCTION(BlueprintPure, Category = "First Night")
	FText BuildDawnReflectionText(const FNightRuntimeOutcome& Outcome) const;

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

	/** The night objective resolved before the duration elapsed — advance to dawn now. */
	UFUNCTION()
	void HandleNightShouldEnd();

	UFUNCTION()
	void HandleHeartWarning(const FVeilHeartWarningFragment& WarningFragment);

	UFUNCTION()
	void HandleHeartDawnReflection(const FNightRuntimeOutcome& Outcome);

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
	void CompleteDawn();

	void CapturePresentationActors();
	void RestoreCapturedPresentation();
	void SetLanternMarkerVisible(bool bVisible);
	void BeginDuskPresentation();
	void ApplyNightPresentation();
	bool BeginDawnPresentation();
	void ApplyPresentationValues(float SunIntensity, float SkyIntensity, const FLinearColor& SunColor,
		const FLinearColor& SkyColor, float FogDensity);

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

	UPROPERTY(Transient)
	TObjectPtr<UDirectionalLightComponent> CachedSun;

	UPROPERTY(Transient)
	TObjectPtr<USkyLightComponent> CachedSkyLight;

	UPROPERTY(Transient)
	TObjectPtr<UExponentialHeightFogComponent> CachedFog;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MarkerMaterials;

	TArray<TWeakObjectPtr<USceneComponent>> MarkerComponents;
	TArray<TWeakObjectPtr<UPointLightComponent>> MarkerLights;

	EFirstNightBeat CurrentBeat = EFirstNightBeat::Intro;
	bool bLanternRestored = false;
	bool bIntroPresented = false;
	bool bDelegatesBound = false;
	bool bPresentationCaptured = false;
	bool bMarkerVisible = false;
	ENightConsequenceType ObservedNightType = ENightConsequenceType::Invalid;

	float CapturedSunIntensity = 0.0f;
	float CapturedSkyIntensity = 0.0f;
	float CapturedFogDensity = 0.0f;
	FLinearColor CapturedSunColor = FLinearColor::White;
	FLinearColor CapturedSkyColor = FLinearColor::White;

	float TransitionElapsed = 0.0f;
	float TransitionDuration = 0.0f;
	float TransitionStartSunIntensity = 0.0f;
	float TransitionStartSkyIntensity = 0.0f;
	float TransitionStartFogDensity = 0.0f;
	float TransitionTargetSunIntensity = 0.0f;
	float TransitionTargetSkyIntensity = 0.0f;
	float TransitionTargetFogDensity = 0.0f;
	FLinearColor TransitionStartSunColor = FLinearColor::White;
	FLinearColor TransitionStartSkyColor = FLinearColor::White;
	FLinearColor TransitionTargetSunColor = FLinearColor::White;
	FLinearColor TransitionTargetSkyColor = FLinearColor::White;
	bool bDawnTransitionActive = false;
	bool bDuskTransitionActive = false;
	float MarkerPulseElapsed = 0.0f;

	FTimerHandle DuskToNightTimer;
	FTimerHandle NightDurationTimer;
};
