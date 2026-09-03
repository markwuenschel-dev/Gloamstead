#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
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
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USceneComponent;

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
 * Thin Cycle I presentation sequencer for the proof-of-loop vertical slice.
 *
 * Owns ONLY the first night's local lantern lesson and presentation triggers. It listens to
 * existing systems (PCG restoration, phase, night runtime), opens the first rest after the
 * lantern restoration, then permanently detaches at Cycle I dawn. DayNight owns every
 * Dusk->Night, Night->Dawn, and early-objective cadence transition.
 *
 * It does NOT replace UGloamsteadDayNightSubsystem / UNightConsequenceManager, does NOT mutate
 * PCG point state, and does NOT own generic night selection, cadence, combat, resources, or progression.
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

	// === State queries ===

	UFUNCTION(BlueprintPure, Category = "First Night")
	EFirstNightBeat GetCurrentBeat() const { return CurrentBeat; }

	UFUNCTION(BlueprintPure, Category = "First Night")
	bool IsLanternRestored() const { return bLanternRestored; }

	UFUNCTION(BlueprintPure, Category = "First Night")
	ENightConsequenceType GetObservedNightType() const { return ObservedNightType; }

	/** True once Cycle I's dawn payoff has relinquished every tutorial-only binding and cue. */
	UFUNCTION(BlueprintPure, Category = "First Night")
	bool IsTutorialDetached() const { return bTutorialDetached; }

	/**
	 * Releases this Cycle I-only actor when a safe later-cycle progression payload
	 * replaces the live world. Called by DayNight before it retries the new warning.
	 */
	void DetachForProgressionResume();

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

private:
	/**
	 * Show one line of the Heart's own words when the Blueprint child does not.
	 *
	 * OnHeartWarning and OnHeartReflection are BlueprintImplementableEvents, which means an unimplemented
	 * one is a SILENT no-op: C++ logs "captioning Heart warning", calls into nothing, and the player sees
	 * no warning at all while every log and test looks healthy. That is exactly what shipped - the
	 * director Blueprint implements neither event.
	 *
	 * If the Blueprint does implement the event, this stands aside and lets it own presentation.
	 *
	 * @param EventName          the BlueprintImplementableEvent this is standing in for
	 * @param CaptionText        the Heart's words
	 */
	void PresentCaptionIfBlueprintDoesNot(FName EventName, const FText& CaptionText);

	/** Take the tutorial caption down. Bound to a timer so it expires like a caption should. */
	UFUNCTION()
	void DismissFallbackCaption();

	/** How long a first-night caption stays readable before it clears. */
	static constexpr float CaptionSeconds = 9.f;

	FTimerHandle CaptionExpiryTimer;

	/** True when a Blueprint subclass actually overrides this event, rather than inheriting the stub. */
	bool IsPresentationEventImplemented(FName EventName) const;

	/** Lazily created project-owned caption widget, used only when the Blueprint presents nothing. */
	UPROPERTY(Transient)
	TObjectPtr<class UUserWidget> FallbackCaptionWidget;

public:

	/** The text OnHeartReflection is given; exposed so the wording is testable without a widget. */
	UFUNCTION(BlueprintPure, Category = "First Night")
	FText BuildDawnReflectionText(const FNightRuntimeOutcome& Outcome) const;

	// === Compatibility advances (DayNight remains the phase/cadence authority) ===

	/** Day -> Dusk. No-op unless the lantern has been restored (the dusk gate). */
	UFUNCTION(BlueprintCallable, Category = "First Night")
	void RequestAdvanceToDusk();

	/** Dusk -> Night Blueprint compatibility entry point; inert because DayNight owns cadence. */
	UFUNCTION(BlueprintCallable, Category = "First Night")
	void RequestAdvanceToNight();

	/** Night -> Dawn Blueprint compatibility entry point; inert because DayNight owns cadence. */
	UFUNCTION(BlueprintCallable, Category = "First Night")
	void RequestAdvanceToDawn();

	// === Delegate handlers (UFUNCTION for AddDynamic; also exercised directly by tests) ===

	UFUNCTION()
	void HandleStructureRestored(const FRestorationEventPayload& Payload);

	UFUNCTION()
	void HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase);

	UFUNCTION()
	void HandleNightStarted(ENightConsequenceType NightType);

	/**
	 * Legacy callback retained only as a safe no-op for serialized delegate references.
	 * DayNight owns the runtime early-objective transition.
	 */
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
	int32 Test_HeartReflectionCount = 0;
	/** Receipt-only test telemetry for the legacy callback; it remains gameplay-inert. */
	int32 Test_LegacyEarlyDawnCallbackCount = 0;

protected:
	void ResolveWorldSystems();
	void BindDelegates();
	void UnbindDelegates();

	void BeginIntro();
	void DetachTutorial();

	void PresentWarning();
	void PresentLanternTarget();
	void PresentDuskCue();
	void PresentEncroachment();
	void PresentDawnPayoff();
	void CompleteDawn();

	void CaptureLanternMarker();
	void SetLanternMarkerVisible(bool bVisible);

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
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MarkerMaterials;

	TArray<TWeakObjectPtr<USceneComponent>> MarkerComponents;
	TArray<TWeakObjectPtr<UPointLightComponent>> MarkerLights;

	EFirstNightBeat CurrentBeat = EFirstNightBeat::Intro;
	bool bLanternRestored = false;
	bool bIntroPresented = false;
	bool bDelegatesBound = false;
	bool bTutorialDetached = false;
	bool bMarkerVisible = false;
	ENightConsequenceType ObservedNightType = ENightConsequenceType::Invalid;
	float MarkerPulseElapsed = 0.0f;
};
