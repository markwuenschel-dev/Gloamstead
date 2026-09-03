#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "GloamsteadSoundscapeSubsystem.generated.h"

class AActor;
class AVeilHeart;
class UGloamsteadPCGSubsystem;
class UGloamsteadSanctuarySynth;
class UAudioComponent;

/** One phase's worth of the sanctuary's voice. Presentation only; nothing here reads back. */
struct FGloamPhaseVoicing
{
	float RootHz = 110.f;
	float Brightness = 0.4f;
	float Level = 0.25f;
	float TremoloHz = 0.f;
};

/**
 * Gives the loop a voice.
 *
 * The project ships no audio assets at all, so this drives UGloamsteadSanctuarySynth rather than
 * playing anything: the bed drops and darkens into Night, opens at Dawn, and its hiss tracks
 * corruption. A restoration and a dawn each strike a tone, so the two moments the game most wants
 * the player to feel are the two it actually makes a sound for.
 *
 * A world subsystem, not an actor, for the same reason the corruption visualiser is one: it needs no
 * level placement, so it cannot be forgotten out of a map. It reads state and never writes it -
 * delete this file and the game plays identically, just silent again.
 *
 * Toggle with `gloam.Audio.Enable 0`.
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadSoundscapeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** The voicing this phase should settle into. Pure, so the table is testable without audio. */
	static FGloamPhaseVoicing VoicingFor(EGloamsteadDayPhase Phase);

	UFUNCTION()
	void HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase);

	UFUNCTION()
	void HandleHeartWarning(const FVeilHeartWarningFragment& WarningFragment);

	/** Test seam: the synth this subsystem is driving, or null when audio is off. */
	UGloamsteadSanctuarySynth* Test_GetSynth() const { return Synth; }

	/**
	 * The forged looping bed for a phase, or null.
	 *
	 * Public because it is the only place the phase->asset mapping exists, and a test that cannot
	 * read it can only assert that SOME audio loads, not that every phase has its own.
	 */
	static const TCHAR* BedPathFor(EGloamsteadDayPhase Phase);

private:
	void ApplyPhase(EGloamsteadDayPhase Phase);

	/** Start (or swap to) the phase's bed. The synth keeps its own voice on top. */
	void ApplyPhaseBed(EGloamsteadDayPhase Phase);

	/** Fire a forged one-shot for a loop event. */
	void PlayOneShot(const TCHAR* Path, float Volume);
	void HandleCorruptionChanged();

	UPROPERTY(Transient)
	TObjectPtr<AActor> SoundscapeHolder;

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadSanctuarySynth> Synth;

	/** The looping ambience for the current phase. Null until the first phase is applied. */
	UPROPERTY(Transient)
	TObjectPtr<class UAudioComponent> BedComponent;

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadDayNightSubsystem> CachedDayNight;

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadPCGSubsystem> CachedPCG;

	TWeakObjectPtr<AVeilHeart> CachedHeart;

	FDelegateHandle CorruptionChangedHandle;
	EGloamsteadDayPhase LastAppliedPhase = EGloamsteadDayPhase::Day;
	bool bBound = false;
};
