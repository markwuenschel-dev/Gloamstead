#include "Presentation/GloamsteadSkyPresenter.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	FGloamSkyPreset LerpPreset(const FGloamSkyPreset& A, const FGloamSkyPreset& B, float T)
	{
		FGloamSkyPreset R;
		R.SunPitch = FMath::Lerp(A.SunPitch, B.SunPitch, T);
		// Shortest-arc so a 350 -> 10 degree swing does not spin the sun the long way round.
		R.SunYaw = A.SunYaw + FMath::UnwindDegrees(B.SunYaw - A.SunYaw) * T;
		R.SunIntensity = FMath::Lerp(A.SunIntensity, B.SunIntensity, T);
		R.SunColor = FMath::Lerp(A.SunColor, B.SunColor, T);
		R.SkyLightIntensity = FMath::Lerp(A.SkyLightIntensity, B.SkyLightIntensity, T);
		R.SkyLightColor = FMath::Lerp(A.SkyLightColor, B.SkyLightColor, T);
		R.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, T);
		R.FogInscattering = FMath::Lerp(A.FogInscattering, B.FogInscattering, T);
		R.ExposureBias = FMath::Lerp(A.ExposureBias, B.ExposureBias, T);
		return R;
	}
}

AGloamsteadSkyPresenter::AGloamsteadSkyPresenter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Day: high raking key so the authored masonry and column profiles read.
	DayPreset = FGloamSkyPreset();

	// Dusk: sun dropped to the horizon and swung warm. This is the phase the player is
	// warned about, so it has to be unmistakable from inside the courtyard.
	DuskPreset.SunPitch = -3.f;
	DuskPreset.SunYaw = 232.f;
	DuskPreset.SunIntensity = 5.5f;
	DuskPreset.SunColor = FLinearColor(1.f, 0.505f, 0.243f);
	DuskPreset.SkyLightIntensity = 2.4f;
	DuskPreset.SkyLightColor = FLinearColor(0.361f, 0.373f, 0.545f);
	DuskPreset.FogDensity = 0.055f;
	DuskPreset.FogInscattering = FLinearColor(0.220f, 0.129f, 0.098f);
	DuskPreset.ExposureBias = 1.55f;

	// Night: sun below the horizon contributing nothing; a cold moon-ish sky fill is the
	// only ambient, so the restored lantern's own light is what makes the plaza legible.
	NightPreset.SunPitch = 14.f;
	NightPreset.SunYaw = 250.f;
	NightPreset.SunIntensity = 0.f;
	NightPreset.SunColor = FLinearColor(0.353f, 0.435f, 0.702f);
	NightPreset.SkyLightIntensity = 0.42f;
	NightPreset.SkyLightColor = FLinearColor(0.196f, 0.267f, 0.443f);
	NightPreset.FogDensity = 0.085f;
	NightPreset.FogInscattering = FLinearColor(0.020f, 0.031f, 0.055f);
	NightPreset.ExposureBias = 2.5f;

	// Dawn: cold pale light returning from the opposite side, so the payoff frame is
	// visibly not a repeat of the opening day.
	DawnPreset.SunPitch = -6.f;
	DawnPreset.SunYaw = 62.f;
	DawnPreset.SunIntensity = 7.f;
	DawnPreset.SunColor = FLinearColor(1.f, 0.812f, 0.702f);
	DawnPreset.SkyLightIntensity = 3.4f;
	DawnPreset.SkyLightColor = FLinearColor(0.541f, 0.627f, 0.729f);
	DawnPreset.FogDensity = 0.062f;
	DawnPreset.FogInscattering = FLinearColor(0.180f, 0.196f, 0.216f);
	DawnPreset.ExposureBias = 1.7f;
}

void AGloamsteadSkyPresenter::CacheTargets()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ADirectionalLight> It(World); It && !Sun; ++It) { Sun = *It; }
	for (TActorIterator<ASkyLight> It(World); It && !Sky; ++It) { Sky = *It; }
	for (TActorIterator<AExponentialHeightFog> It(World); It && !Fog; ++It) { Fog = *It; }
	// Only an unbound volume grades the whole level; a bounded one would apply nowhere
	// useful and silently swallow the exposure changes.
	for (TActorIterator<APostProcessVolume> It(World); It && !Grade; ++It)
	{
		if (It->bUnbound)
		{
			Grade = *It;
		}
	}
}

const FGloamSkyPreset& AGloamsteadSkyPresenter::PresetFor(EGloamsteadDayPhase Phase) const
{
	switch (Phase)
	{
	case EGloamsteadDayPhase::Dusk:  return DuskPreset;
	case EGloamsteadDayPhase::Night: return NightPreset;
	case EGloamsteadDayPhase::Dawn:  return DawnPreset;
	default:                         return DayPreset;
	}
}

void AGloamsteadSkyPresenter::BeginPlay()
{
	Super::BeginPlay();
	CacheTargets();

	if (UWorld* World = GetWorld())
	{
		CachedDayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>();
	}

	if (CachedDayNight)
	{
		CachedDayNight->OnPhaseChanged.AddDynamic(this, &AGloamsteadSkyPresenter::HandlePhaseChanged);
		// Snap to whatever phase is already live, so a mid-loop load does not open on the
		// wrong sky and then blend to the right one.
		FromPreset = ToPreset = PresetFor(CachedDayNight->GetCurrentPhase());
	}
	else
	{
		FromPreset = ToPreset = DayPreset;
	}

	BlendAlpha = 1.f;
	ApplyPreset(ToPreset);
}

void AGloamsteadSkyPresenter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (CachedDayNight)
	{
		CachedDayNight->OnPhaseChanged.RemoveDynamic(this, &AGloamsteadSkyPresenter::HandlePhaseChanged);
		CachedDayNight = nullptr;
	}
	Super::EndPlay(Reason);
}

void AGloamsteadSkyPresenter::HandlePhaseChanged(EGloamsteadDayPhase /*OldPhase*/, EGloamsteadDayPhase NewPhase)
{
	// Blend from where the sky actually is, not from the previous phase's preset: a phase
	// change mid-blend would otherwise jump backwards before moving on.
	FromPreset = LerpPreset(FromPreset, ToPreset, BlendAlpha);
	ToPreset = PresetFor(NewPhase);
	BlendAlpha = 0.f;

	UE_LOG(LogTemp, Log, TEXT("GloamSky: phase -> %d, blending over %.1fs"),
		static_cast<int32>(NewPhase), BlendSeconds);
}

void AGloamsteadSkyPresenter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (BlendAlpha >= 1.f)
	{
		return;
	}

	BlendAlpha = (BlendSeconds > KINDA_SMALL_NUMBER)
		? FMath::Min(1.f, BlendAlpha + DeltaSeconds / BlendSeconds)
		: 1.f;
	ApplyPreset(LerpPreset(FromPreset, ToPreset, BlendAlpha));
}

void AGloamsteadSkyPresenter::ApplyPreset(const FGloamSkyPreset& P)
{
	if (Sun)
	{
		Sun->SetActorRotation(FRotator(P.SunPitch, P.SunYaw, 0.f));
		if (UDirectionalLightComponent* C = Sun->FindComponentByClass<UDirectionalLightComponent>())
		{
			// Lights baked as Static refuse runtime edits, so force Movable once. Without
			// this the whole presenter silently no-ops on a lit level.
			if (C->Mobility != EComponentMobility::Movable)
			{
				C->SetMobility(EComponentMobility::Movable);
			}
			C->SetIntensity(P.SunIntensity);
			C->SetLightColor(P.SunColor);
		}
	}

	if (Sky)
	{
		if (USkyLightComponent* C = Sky->FindComponentByClass<USkyLightComponent>())
		{
			if (C->Mobility != EComponentMobility::Movable)
			{
				C->SetMobility(EComponentMobility::Movable);
			}
			C->SetIntensity(P.SkyLightIntensity);
			C->SetLightColor(P.SkyLightColor);
		}
	}

	if (Fog)
	{
		if (UExponentialHeightFogComponent* C = Fog->FindComponentByClass<UExponentialHeightFogComponent>())
		{
			C->SetFogDensity(P.FogDensity);
			C->SetFogInscatteringColor(P.FogInscattering);
		}
	}

	if (Grade)
	{
		Grade->Settings.bOverride_AutoExposureBias = true;
		Grade->Settings.AutoExposureBias = P.ExposureBias;
	}
}
