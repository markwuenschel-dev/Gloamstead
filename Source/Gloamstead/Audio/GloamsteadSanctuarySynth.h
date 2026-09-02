#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "GloamsteadSanctuarySynth.generated.h"

/**
 * The sanctuary's voice, generated sample by sample rather than played from an asset.
 *
 * Gloamstead ships zero audio content - not one wave, cue or MetaSound anywhere in Content/ - so the
 * game was completely silent, in a design whose entire subject is a place that speaks to you and a
 * night you are supposed to hear coming. Authoring audio assets needs a human, source material and
 * the editor; synthesising a bed does not, so this is the half that can actually ship from here.
 *
 * It is deliberately a drone and not a tune. Three partials over a root, a slow tremolo, and a noise
 * bed whose level tracks corruption: enough for the four phases to sound different from each other
 * and for a bad night to sound worse, and not so much that it pretends to be a score. Real music and
 * a recorded whisper for the Heart remain owed.
 *
 * Threading: OnGenerateAudio runs on the audio render thread and must not allocate, lock or touch
 * UObjects. The game thread only ever stores plain values into the atomics below; every one of them
 * is smoothed toward its target inside the callback, so a parameter change can never click.
 */
UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class GLOAMSTEAD_API UGloamsteadSanctuarySynth : public USynthComponent
{
	GENERATED_BODY()

public:
	// USynthComponent has no default constructor, so the initializer must be threaded through.
	UGloamsteadSanctuarySynth(const FObjectInitializer& ObjectInitializer);

	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

	/**
	 * Sets the voicing the bed should move toward. Safe from the game thread at any time.
	 *
	 * @param RootHz     fundamental; lower is heavier, and night sits lowest
	 * @param Brightness 0..1 weight of the upper partials - how open the chord sounds
	 * @param Level      0..1 overall bed gain
	 * @param TremoloHz  amplitude wobble; 0 is still, and a threat abroad is not still
	 * @param Unease     0..1 noise bed, driven by corruption
	 */
	void SetVoicing(float RootHz, float Brightness, float Level, float TremoloHz, float Unease);

	/** One-shot struck tone: a restoration, a warning, a dawn. Decays on its own. */
	void Strike(float Hz, float Amplitude, float DecaySeconds);

	/** Test seam: the voicing currently being driven toward. */
	float Test_GetTargetRootHz() const { return TargetRootHz.load(std::memory_order_relaxed); }
	float Test_GetTargetLevel() const { return TargetLevel.load(std::memory_order_relaxed); }
	/** Test seam: how many struck tones this synth has been asked for. */
	int32 Test_GetStrikeCount() const { return StrikeCount.load(std::memory_order_relaxed); }

private:
	/** One-pole smoothing toward a target, per sample. Keeps every change inaudible as a step. */
	static float Approach(float Current, float Target, float Coefficient);

	std::atomic<float> TargetRootHz{ 110.f };
	std::atomic<float> TargetBrightness{ 0.4f };
	std::atomic<float> TargetLevel{ 0.f };
	std::atomic<float> TargetTremoloHz{ 0.f };
	std::atomic<float> TargetUnease{ 0.f };

	/** A pending struck tone, handed over without a lock and consumed once by the render thread. */
	std::atomic<bool> bStrikePending{ false };
	std::atomic<float> PendingStrikeHz{ 440.f };
	std::atomic<float> PendingStrikeAmp{ 0.f };
	std::atomic<float> PendingStrikeDecay{ 1.5f };
	std::atomic<int32> StrikeCount{ 0 };

	// === Render-thread state. Touched ONLY inside OnGenerateAudio. ===
	float SampleRateHz = 48000.f;
	float RootHz = 110.f;
	float Brightness = 0.4f;
	float Level = 0.f;
	float TremoloHz = 0.f;
	float Unease = 0.f;

	float PhaseRoot = 0.f;
	float PhaseFifth = 0.f;
	float PhaseOctave = 0.f;
	float PhaseTremolo = 0.f;

	float StrikePhase = 0.f;
	float StrikeHz = 0.f;
	float StrikeEnvelope = 0.f;
	float StrikeDecayPerSample = 0.f;

	/** Cheap deterministic noise. A real PRNG is not worth the cost for a hiss under a drone. */
	uint32 NoiseSeed = 0x1234567u;
};
