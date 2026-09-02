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

private:
	void ApplyPhase(EGloamsteadDayPhase Phase);
	void HandleCorruptionChanged();

	UPROPERTY(Transient)
	TObjectPtr<AActor> SoundscapeHolder;

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadSanctuarySynth> Synth;

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadDayNightSubsystem> CachedDayNight;

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadPCGSubsystem> CachedPCG;

	TWeakObjectPtr<AVeilHeart> CachedHeart;

	FDelegateHandle CorruptionChangedHandle;
	EGloamsteadDayPhase LastAppliedPhase = EGloamsteadDayPhase::Day;
	bool bBound = false;
};
