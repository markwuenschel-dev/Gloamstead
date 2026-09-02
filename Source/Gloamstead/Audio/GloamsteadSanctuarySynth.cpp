#include "Audio/GloamsteadSanctuarySynth.h"

namespace GloamsteadSynthVoice
{
	// Named, not anonymous: this module builds through UBT adaptive unity, where anonymous-namespace
	// helpers do not stay file-local and have already collided here (C2264).

	/** Two pi, at float precision, once. */
	constexpr float TwoPi = 6.28318530718f;

	/** Wraps a running phase without an fmod in the inner loop. */
	FORCEINLINE void Advance(float& Phase, float Hz, float SampleRateHz)
	{
		Phase += (TwoPi * Hz) / SampleRateHz;
		if (Phase >= TwoPi)
		{
			Phase -= TwoPi;
		}
	}
}

UGloamsteadSanctuarySynth::UGloamsteadSanctuarySynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Mono and unspatialised: this is the sanctuary itself, not a thing standing somewhere in it.
	NumChannels = 1;
	bAllowSpatialization = false;
	bAlwaysPlay = true;
	PrimaryComponentTick.bCanEverTick = false;
}

bool UGloamsteadSanctuarySynth::Init(int32& SampleRate)
{
	NumChannels = 1;
	SampleRateHz = FMath::Max(static_cast<float>(SampleRate), 1.f);
	return true;
}

void UGloamsteadSanctuarySynth::SetVoicing(
	float RootHzIn, float BrightnessIn, float LevelIn, float TremoloHzIn, float UneaseIn)
{
	TargetRootHz.store(FMath::Clamp(RootHzIn, 20.f, 2000.f), std::memory_order_relaxed);
	TargetBrightness.store(FMath::Clamp(BrightnessIn, 0.f, 1.f), std::memory_order_relaxed);
	TargetLevel.store(FMath::Clamp(LevelIn, 0.f, 1.f), std::memory_order_relaxed);
	TargetTremoloHz.store(FMath::Clamp(TremoloHzIn, 0.f, 12.f), std::memory_order_relaxed);
	TargetUnease.store(FMath::Clamp(UneaseIn, 0.f, 1.f), std::memory_order_relaxed);
}

void UGloamsteadSanctuarySynth::Strike(float Hz, float Amplitude, float DecaySeconds)
{
	PendingStrikeHz.store(FMath::Clamp(Hz, 40.f, 4000.f), std::memory_order_relaxed);
	PendingStrikeAmp.store(FMath::Clamp(Amplitude, 0.f, 1.f), std::memory_order_relaxed);
	PendingStrikeDecay.store(FMath::Clamp(DecaySeconds, 0.05f, 10.f), std::memory_order_relaxed);
	StrikeCount.fetch_add(1, std::memory_order_relaxed);
	// Released last, so the render thread never reads a half-written strike.
	bStrikePending.store(true, std::memory_order_release);
}

float UGloamsteadSanctuarySynth::Approach(float Current, float Target, float Coefficient)
{
	return Current + (Target - Current) * Coefficient;
}

int32 UGloamsteadSanctuarySynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	using namespace GloamsteadSynthVoice;

	// Consume a pending strike once, at buffer start. Retriggering mid-buffer would cost a branch
	// per sample to answer a question nobody can hear at this buffer size.
	if (bStrikePending.exchange(false, std::memory_order_acquire))
	{
		StrikeHz = PendingStrikeHz.load(std::memory_order_relaxed);
		StrikeEnvelope = PendingStrikeAmp.load(std::memory_order_relaxed);
		StrikePhase = 0.f;
		const float Decay = PendingStrikeDecay.load(std::memory_order_relaxed);
		// Exponential decay to roughly -60 dB over Decay seconds.
		StrikeDecayPerSample = FMath::Exp(-6.9078f / FMath::Max(Decay * SampleRateHz, 1.f));
	}

	const float RootTarget = TargetRootHz.load(std::memory_order_relaxed);
	const float BrightTarget = TargetBrightness.load(std::memory_order_relaxed);
	const float LevelTarget = TargetLevel.load(std::memory_order_relaxed);
	const float TremoloTarget = TargetTremoloHz.load(std::memory_order_relaxed);
	const float UneaseTarget = TargetUnease.load(std::memory_order_relaxed);

	// ~50 ms glide at 48 kHz. Slow enough that a phase change is heard as a change of weather rather
	// than as a cut, which is the difference between atmosphere and a UI sound.
	const float Glide = FMath::Clamp(1.f / (0.05f * SampleRateHz), 0.f, 1.f);

	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		RootHz = Approach(RootHz, RootTarget, Glide);
		Brightness = Approach(Brightness, BrightTarget, Glide);
		Level = Approach(Level, LevelTarget, Glide);
		TremoloHz = Approach(TremoloHz, TremoloTarget, Glide);
		Unease = Approach(Unease, UneaseTarget, Glide);

		// Root, fifth, octave. A bare fifth is deliberately hollow - it is the interval that sounds
		// like a place rather than a chord that sounds like a feeling.
		const float Root = FMath::Sin(PhaseRoot);
		const float Fifth = FMath::Sin(PhaseFifth) * (0.45f * Brightness);
		const float Octave = FMath::Sin(PhaseOctave) * (0.22f * Brightness * Brightness);

		Advance(PhaseRoot, RootHz, SampleRateHz);
		Advance(PhaseFifth, RootHz * 1.5f, SampleRateHz);
		Advance(PhaseOctave, RootHz * 2.f, SampleRateHz);

		// Tremolo never goes fully to silence: a bed that gates off reads as a dropout.
		float Tremolo = 1.f;
		if (TremoloHz > KINDA_SMALL_NUMBER)
		{
			Advance(PhaseTremolo, TremoloHz, SampleRateHz);
			Tremolo = 0.72f + 0.28f * FMath::Sin(PhaseTremolo);
		}

		// The rot, as hiss. Corruption is the one state the player must feel without looking at a
		// meter, and it is the only voice here that is not pitched.
		NoiseSeed = NoiseSeed * 1664525u + 1013904223u;
		const float Noise = (static_cast<float>((NoiseSeed >> 9) & 0x7FFFF) / 262143.f - 1.f);

		float Sample = (Root + Fifth + Octave) * 0.32f * Level * Tremolo;
		Sample += Noise * 0.055f * Unease * Level;

		if (StrikeEnvelope > 1e-5f)
		{
			Sample += FMath::Sin(StrikePhase) * StrikeEnvelope * 0.5f;
			Advance(StrikePhase, StrikeHz, SampleRateHz);
			StrikeEnvelope *= StrikeDecayPerSample;
		}

		// Hard-clip rather than let a sum overshoot into wrap-around distortion. With these gains it
		// should never engage; it is here because a silent game is better than a painful one.
		OutAudio[Index] = FMath::Clamp(Sample, -1.f, 1.f);
	}

	return NumSamples;
}
