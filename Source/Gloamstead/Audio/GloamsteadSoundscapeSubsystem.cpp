#include "Audio/GloamsteadSoundscapeSubsystem.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"

#include "Audio/GloamsteadSanctuarySynth.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/VeilHeart.h"

namespace GloamsteadSoundscapeConfig
{
	// Named, not anonymous - adaptive unity again.
	static TAutoConsoleVariable<bool> CVarAudioEnabled(
		TEXT("gloam.Audio.Enable"),
		true,
		TEXT("Generate the sanctuary's synthesised soundscape (default on)."),
		ECVF_Default);
}

FGloamPhaseVoicing UGloamsteadSoundscapeSubsystem::VoicingFor(EGloamsteadDayPhase Phase)
{
	FGloamPhaseVoicing Voicing;
	switch (Phase)
	{
	case EGloamsteadDayPhase::Day:
		// Cold and legible. Open enough to explore in, quiet enough not to be company.
		Voicing.RootHz = 130.81f;   // C3
		Voicing.Brightness = 0.55f;
		Voicing.Level = 0.22f;
		Voicing.TremoloHz = 0.f;
		break;

	case EGloamsteadDayPhase::Dusk:
		// A step down and a slow breath in it. This is the phase that is supposed to feel like a
		// decision you have not finished making.
		Voicing.RootHz = 110.f;     // A2
		Voicing.Brightness = 0.38f;
		Voicing.Level = 0.28f;
		Voicing.TremoloHz = 0.28f;
		break;

	case EGloamsteadDayPhase::Night:
		// Lowest and darkest, and the only phase that moves on its own. The tremolo is what makes
		// the room feel occupied when nothing is visible yet.
		Voicing.RootHz = 73.42f;    // D2
		Voicing.Brightness = 0.16f;
		Voicing.Level = 0.34f;
		Voicing.TremoloHz = 1.15f;
		break;

	case EGloamsteadDayPhase::Dawn:
		// Release: the brightest voicing in the game, and the only one that opens rather than sits.
		Voicing.RootHz = 146.83f;   // D3
		Voicing.Brightness = 0.82f;
		Voicing.Level = 0.24f;
		Voicing.TremoloHz = 0.f;
		break;

	default:
		break;
	}
	return Voicing;
}

void UGloamsteadSoundscapeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// A synthesised bed belongs to a live game world only. Automation runs -nullrhi with no audio
	// device, and an editor preview world should stay silent.
	if (!InWorld.IsGameWorld() || !GloamsteadSoundscapeConfig::CVarAudioEnabled.GetValueOnGameThread())
	{
		return;
	}

	CachedDayNight = InWorld.GetSubsystem<UGloamsteadDayNightSubsystem>();
	CachedPCG = InWorld.GetSubsystem<UGloamsteadPCGSubsystem>();

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	SoundscapeHolder = InWorld.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!SoundscapeHolder)
	{
		return;
	}
	SoundscapeHolder->SetRootComponent(
		NewObject<USceneComponent>(SoundscapeHolder, TEXT("SoundscapeRoot")));
	SoundscapeHolder->GetRootComponent()->RegisterComponent();

	Synth = NewObject<UGloamsteadSanctuarySynth>(SoundscapeHolder, TEXT("SanctuaryVoice"));
	Synth->SetupAttachment(SoundscapeHolder->GetRootComponent());
	Synth->RegisterComponent();
	Synth->Start();

	if (CachedDayNight)
	{
		CachedDayNight->OnPhaseChanged.AddDynamic(this, &UGloamsteadSoundscapeSubsystem::HandlePhaseChanged);
		ApplyPhase(CachedDayNight->GetCurrentPhase());
	}
	else
	{
		ApplyPhase(EGloamsteadDayPhase::Day);
	}

	if (CachedPCG)
	{
		CorruptionChangedHandle = CachedPCG->OnCorruptionChanged.AddUObject(
			this, &UGloamsteadSoundscapeSubsystem::HandleCorruptionChanged);
	}

	// The Heart may spawn after this subsystem. Bind if it is already here; the phase handler retries.
	for (TActorIterator<AVeilHeart> It(&InWorld); It; ++It)
	{
		CachedHeart = *It;
		It->OnWarningEmittedDelegate.AddDynamic(this, &UGloamsteadSoundscapeSubsystem::HandleHeartWarning);
		break;
	}

	bBound = true;
	UE_LOG(LogTemp, Log, TEXT("Soundscape: the sanctuary has a voice (synth + forged beds)."));
}

void UGloamsteadSoundscapeSubsystem::Deinitialize()
{
	if (bBound)
	{
		if (IsValid(CachedDayNight))
		{
			CachedDayNight->OnPhaseChanged.RemoveDynamic(this, &UGloamsteadSoundscapeSubsystem::HandlePhaseChanged);
		}
		if (IsValid(CachedPCG))
		{
			CachedPCG->OnCorruptionChanged.Remove(CorruptionChangedHandle);
		}
		if (CachedHeart.IsValid())
		{
			CachedHeart->OnWarningEmittedDelegate.RemoveDynamic(
				this, &UGloamsteadSoundscapeSubsystem::HandleHeartWarning);
		}
	}
	bBound = false;
	CorruptionChangedHandle.Reset();

	if (Synth)
	{
		Synth->Stop();
		Synth = nullptr;
	}
	if (SoundscapeHolder)
	{
		SoundscapeHolder->Destroy();
		SoundscapeHolder = nullptr;
	}
	CachedDayNight = nullptr;
	CachedPCG = nullptr;

	Super::Deinitialize();
}

const TCHAR* UGloamsteadSoundscapeSubsystem::BedPathFor(EGloamsteadDayPhase Phase)
{
	// Forged by procedural/audio/forge_sanctuary_audio.py, pitched from VoicingFor below so the bed
	// and the runtime synth share a key rather than beating against each other.
	switch (Phase)
	{
	case EGloamsteadDayPhase::Day:   return TEXT("/Game/Gloamstead/Audio/S_AMB_Sanctuary_Day.S_AMB_Sanctuary_Day");
	case EGloamsteadDayPhase::Dusk:  return TEXT("/Game/Gloamstead/Audio/S_AMB_Sanctuary_Dusk.S_AMB_Sanctuary_Dusk");
	case EGloamsteadDayPhase::Night: return TEXT("/Game/Gloamstead/Audio/S_AMB_Sanctuary_Night.S_AMB_Sanctuary_Night");
	case EGloamsteadDayPhase::Dawn:  return TEXT("/Game/Gloamstead/Audio/S_AMB_Sanctuary_Dawn.S_AMB_Sanctuary_Dawn");
	default:                         return nullptr;
	}
}

void UGloamsteadSoundscapeSubsystem::ApplyPhaseBed(EGloamsteadDayPhase Phase)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	const TCHAR* Path = BedPathFor(Phase);
	if (!Path)
	{
		return;
	}

	USoundBase* Bed = LoadObject<USoundBase>(nullptr, Path);
	if (!Bed)
	{
		// Loud, and it names the script: the project shipped with zero sound assets for its whole
		// life, so a missing one is far more likely to mean "the forge was never run on this
		// machine" than "someone deleted it".
		UE_LOG(LogTemp, Error,
			TEXT("Soundscape: could not load %s. Run procedural/audio/forge_sanctuary_audio.py and "
				 "its importer; the sanctuary keeps its synthesised voice but has no bed."), Path);
		return;
	}

	if (!BedComponent)
	{
		BedComponent = NewObject<UAudioComponent>(SoundscapeHolder ? SoundscapeHolder : World->GetWorldSettings());
		if (!BedComponent)
		{
			return;
		}
		// The bed describes the whole sanctuary, not a place inside it, so it is deliberately 2D:
		// a positioned bed would tell the player where to stand to feel safe, which is the lantern
		// light's job and not the ambience's.
		BedComponent->bAllowSpatialization = false;
		BedComponent->bAutoActivate = false;
		BedComponent->bIsUISound = false;
		BedComponent->RegisterComponentWithWorld(World);
	}

	if (BedComponent->Sound != Bed)
	{
		BedComponent->SetSound(Bed);
		// Under the synth rather than over it. The synth carries the phase's motion and reacts to
		// corruption; the bed is the room it moves in.
		BedComponent->SetVolumeMultiplier(0.45f);
		BedComponent->Play();
		UE_LOG(LogTemp, Log, TEXT("Soundscape: bed -> %s."), Path);
	}
}

void UGloamsteadSoundscapeSubsystem::PlayOneShot(const TCHAR* Path, float Volume)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !Path)
	{
		return;
	}

	if (USoundBase* Cue = LoadObject<USoundBase>(nullptr, Path))
	{
		// 2D for the same reason as the bed: these mark events in the loop, not places in the world.
		UGameplayStatics::PlaySound2D(World, Cue, Volume);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Soundscape: could not load one-shot %s."), Path);
	}
}

void UGloamsteadSoundscapeSubsystem::ApplyPhase(EGloamsteadDayPhase Phase)
{
	if (!Synth)
	{
		return;
	}

	const FGloamPhaseVoicing Voicing = VoicingFor(Phase);
	const float Unease = CachedPCG ? CachedPCG->GetSanctuaryAverageCorruptionLevel() : 0.f;
	Synth->SetVoicing(Voicing.RootHz, Voicing.Brightness, Voicing.Level, Voicing.TremoloHz, Unease);
	ApplyPhaseBed(Phase);
	LastAppliedPhase = Phase;
}

void UGloamsteadSoundscapeSubsystem::HandlePhaseChanged(
	EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase)
{
	// Late Heart binding: the Heart is spawned from the level and can arrive after this subsystem.
	if (!CachedHeart.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AVeilHeart> It(World); It; ++It)
			{
				CachedHeart = *It;
				It->OnWarningEmittedDelegate.AddDynamic(
					this, &UGloamsteadSoundscapeSubsystem::HandleHeartWarning);
				break;
			}
		}
	}

	ApplyPhase(NewPhase);

	// Dawn is the only phase that gets a struck tone of its own. It is the payoff beat, and the one
	// moment the design says must always answer something.
	if (NewPhase == EGloamsteadDayPhase::Dawn && Synth)
	{
		Synth->Strike(587.33f, 0.42f, 3.2f); // D5
	}
}

void UGloamsteadSoundscapeSubsystem::HandleHeartWarning(const FVeilHeartWarningFragment& WarningFragment)
{
	if (Synth)
	{
		// Low and long, under the bed rather than over it: the Heart is not a notification.
		Synth->Strike(196.f, 0.30f, 4.5f); // G3
	}

	// The forged tone doubles the synth strike at the same pitch rather than replacing it: the synth
	// carries the decay and reacts to corruption, the asset gives the strike a body the oscillator
	// alone does not have. Quiet, because the sentence is the point, not the sound of it arriving.
	PlayOneShot(TEXT("/Game/Gloamstead/Audio/S_SFX_Heart_Warning.S_SFX_Heart_Warning"), 0.55f);
}

void UGloamsteadSoundscapeSubsystem::HandleCorruptionChanged()
{
	// Re-voice at the current phase so the hiss tracks the bloom without changing the pitch bed.
	ApplyPhase(LastAppliedPhase);
}
