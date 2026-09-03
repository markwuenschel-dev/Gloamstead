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
#include "Kismet/GameplayStatics.h"
#include "Systems/VeilHeart.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

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

	ReportBoundTargets();
}

void AGloamsteadSkyPresenter::ReportBoundTargets()
{
	const uint8 Mask =
		  (Sun   ? 1 : 0)
		| (Sky   ? 2 : 0)
		| (Fog   ? 4 : 0)
		| (Grade ? 8 : 0);

	if (Mask == ReportedTargetMask)
	{
		return;
	}
	ReportedTargetMask = Mask;

	// Named individually rather than as a count: "3 of 4" does not tell anyone WHICH blend is
	// running into nothing, and the four failures look completely different on screen.
	const bool bAll = (Mask == 0x0F);
	UE_LOG(LogTemp, Log, TEXT("GloamsteadSkyPresenter: sun=%s sky=%s fog=%s grade=%s."),
		Sun   ? TEXT("bound") : TEXT("MISSING"),
		Sky   ? TEXT("bound") : TEXT("MISSING"),
		Fog   ? TEXT("bound") : TEXT("MISSING"),
		Grade ? TEXT("bound") : TEXT("MISSING"));

	if (!bAll)
	{
		// Warning rather than Log for the incomplete case: every missing target is a phase blend
		// that silently does nothing, which is exactly the class of defect this project keeps
		// shipping past a green suite.
		UE_LOG(LogTemp, Warning,
			TEXT("GloamsteadSkyPresenter: the day/night blend is incomplete - place the missing actor(s) in the level. "
				 "A PostProcessVolume must have bUnbound set, or it grades nowhere and the exposure curve is swallowed."));
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
	Test_PresentedPhaseHistory.Reset();
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
	TryBindPostTutorialWarningPresenter();
}

void AGloamsteadSkyPresenter::EndPlay(const EEndPlayReason::Type Reason)
{
	UnbindPostTutorialWarningPresenter();
	if (CachedDayNight)
	{
		CachedDayNight->OnPhaseChanged.RemoveDynamic(this, &AGloamsteadSkyPresenter::HandlePhaseChanged);
		CachedDayNight = nullptr;
	}
	Super::EndPlay(Reason);
}

void AGloamsteadSkyPresenter::HandlePhaseChanged(EGloamsteadDayPhase /*OldPhase*/, EGloamsteadDayPhase NewPhase)
{
	Test_PresentedPhaseHistory.Add(NewPhase);
	// Blend from where the sky actually is, not from the previous phase's preset: a phase
	// change mid-blend would otherwise jump backwards before moving on.
	FromPreset = LerpPreset(FromPreset, ToPreset, BlendAlpha);
	ToPreset = PresetFor(NewPhase);
	BlendAlpha = 0.f;

	// A caption is a momentary thing and was treated as a permanent one: the widget is added with
	// AddToPlayerScreen and nothing ever cleared it, so the Heart's warning for the cycle stayed
	// pinned to the bottom of the screen for the rest of the game. By the ending, Cycle VI's
	// sentence was still there, drawn through the reckoning panel. Clearing on every phase change
	// is the smallest rule that matches what a caption is for.
	ClearFallbackCaption();

	// BP_FirstNightDirector captions with Print String nodes, which render as engine on-screen
	// messages rather than through any widget - so removing the caption widget above cannot touch
	// them, and they outlive their moment the same way. By the ending, Cycle I's "find the ruined
	// lantern" and its dawn line were still stacked over the reckoning panel.
	//
	// This clears what is already on screen at each phase change; it does not disable the feature,
	// so a later Print String still shows, and every one of these lines is in the log regardless.
	if (GEngine && GetWorld() && GetWorld()->IsGameWorld())
	{
		GEngine->ClearOnScreenDebugMessages();
	}

	UE_LOG(LogTemp, Log, TEXT("GloamSky: phase -> %d, blending over %.1fs"),
		static_cast<int32>(NewPhase), BlendSeconds);
}

void AGloamsteadSkyPresenter::ClearFallbackCaption()
{
	if (FallbackCaptionWidget)
	{
		FallbackCaptionWidget->RemoveFromParent();
		FallbackCaptionWidget = nullptr;
	}
}

void AGloamsteadSkyPresenter::TryBindPostTutorialWarningPresenter()
{
	if (bPostTutorialWarningPresenterBound)
	{
		if (AVeilHeart* Heart = CachedHeart.Get(); Heart
			&& Heart->HasValidWarningPresenter()
			&& Heart->OnWarningEmittedDelegate.Contains(this, GET_FUNCTION_NAME_CHECKED(AGloamsteadSkyPresenter, HandleHeartWarning)))
		{
			return;
		}
		UnbindPostTutorialWarningPresenter();
	}

	// Cycle I owns its own intentionally lantern-specific caption and must be
	// the only registered presenter until its dawn teardown. On later days the
	// sky actor provides the generic bridge while Task 6 materializes the full
	// caption/journal surface.
	if (CachedDayNight
		&& CachedDayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day
		&& CachedDayNight->GetNightCount() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> Hearts;
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	if (Hearts.Num() != 1)
	{
		return;
	}

	AVeilHeart* Heart = Cast<AVeilHeart>(Hearts[0]);
	if (!Heart)
	{
		return;
	}

	Heart->OnWarningEmittedDelegate.AddDynamic(this, &AGloamsteadSkyPresenter::HandleHeartWarning);
	if (Heart->RegisterWarningPresenter(this, GET_FUNCTION_NAME_CHECKED(AGloamsteadSkyPresenter, HandleHeartWarning)))
	{
		CachedHeart = Heart;
		bPostTutorialWarningPresenterBound = true;
	}
	else
	{
		Heart->OnWarningEmittedDelegate.RemoveDynamic(this, &AGloamsteadSkyPresenter::HandleHeartWarning);
	}
}

void AGloamsteadSkyPresenter::UnbindPostTutorialWarningPresenter()
{
	if (AVeilHeart* Heart = CachedHeart.Get())
	{
		Heart->OnWarningEmittedDelegate.RemoveDynamic(this, &AGloamsteadSkyPresenter::HandleHeartWarning);
		Heart->UnregisterWarningPresenter(this);
	}
	CachedHeart.Reset();
	bPostTutorialWarningPresenterBound = false;
}

void AGloamsteadSkyPresenter::HandleHeartWarning(const FVeilHeartWarningFragment& WarningFragment)
{
	LastPresentedWarningId = WarningFragment.WarningId;
	OnHeartWarning(WarningFragment.Fragment);
	PresentWarningCaption(WarningFragment);
}

bool AGloamsteadSkyPresenter::IsPresentationEventImplemented(FName EventName) const
{
	const UFunction* Function = GetClass()->FindFunctionByName(EventName);
	if (!Function)
	{
		return false;
	}
	return Function->GetOuter() != AGloamsteadSkyPresenter::StaticClass();
}

void AGloamsteadSkyPresenter::PresentWarningCaption(const FVeilHeartWarningFragment& WarningFragment)
{
	// A Blueprint child that implements OnHeartWarning owns presentation; do not caption twice.
	if (IsPresentationEventImplemented(TEXT("OnHeartWarning")))
	{
		return;
	}

	// DayNight clears its dedup key and re-broadcasts the armed warning whenever a presenter registers
	// (GloamsteadDayNightSubsystem.cpp:284), so the identical fragment arrives twice milliseconds apart.
	// That was harmless while nothing presented it. Now that it reaches a screen, it must caption once.
	if (!WarningFragment.WarningId.IsNone() && WarningFragment.WarningId == LastCaptionedWarningId)
	{
		return;
	}

	const FText& CaptionText = WarningFragment.Fragment;
	if (CaptionText.IsEmpty())
	{
		return;
	}

	// Latch here, not after the widget call: the dedup must hold even in a world with no screen, or a
	// re-broadcast would be "accepted" twice the moment one appears.
	LastCaptionedWarningId = WarningFragment.WarningId;
	++CaptionAcceptedCount;

	// A caption needs a screen. Synthetic automation worlds have no local player; that is not a defect.
	APlayerController* Viewer = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!Viewer)
	{
		return;
	}

	if (!FallbackCaptionWidget)
	{
		static const TCHAR* CaptionPath = TEXT("/Game/FirstNight/WBP_FirstNightCaption.WBP_FirstNightCaption_C");
		UClass* CaptionClass = LoadClass<UUserWidget>(nullptr, CaptionPath);
		if (!CaptionClass)
		{
			UE_LOG(LogTemp, Error,
				TEXT("SkyPresenter: no Blueprint implements OnHeartWarning and the caption widget at %s could not "
					 "be loaded, so the Heart's words reach no one from Cycle II on."), CaptionPath);
			return;
		}

		FallbackCaptionWidget = CreateWidget<UUserWidget>(Viewer, CaptionClass);
		if (!FallbackCaptionWidget)
		{
			UE_LOG(LogTemp, Error, TEXT("SkyPresenter: could not create the fallback caption widget."));
			return;
		}
		FallbackCaptionWidget->AddToPlayerScreen(20);
	}

	// Verify the widget's entry point rather than assuming its shape: exactly one FText parameter.
	UFunction* Display = FallbackCaptionWidget->FindFunction(TEXT("DisplayCaption"));
	FTextProperty* TextParam = nullptr;
	int32 ParamCount = 0;
	if (Display)
	{
		for (TFieldIterator<FProperty> It(Display); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			++ParamCount;
			TextParam = CastField<FTextProperty>(*It);
		}
	}

	if (!Display || ParamCount != 1 || !TextParam)
	{
		UE_LOG(LogTemp, Error,
			TEXT("SkyPresenter: the caption widget has no DisplayCaption(FText) entry point, so the Heart's warning "
				 "cannot be shown. Implement OnHeartWarning on a Blueprint child, or give the widget that entry point."));
		return;
	}

	void* Parms = FMemory::Malloc(Display->ParmsSize);
	FMemory::Memzero(Parms, Display->ParmsSize);
	TextParam->InitializeValue_InContainer(Parms);
	TextParam->SetPropertyValue_InContainer(Parms, CaptionText);
	FallbackCaptionWidget->ProcessEvent(Display, Parms);
	TextParam->DestroyValue_InContainer(Parms);
	FMemory::Free(Parms);

	UE_LOG(LogTemp, Log, TEXT("SkyPresenter: captioned Heart warning [%s] natively: \"%s\""),
		*WarningFragment.WarningId.ToString(), *CaptionText.ToString());
}

void AGloamsteadSkyPresenter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TryBindPostTutorialWarningPresenter();

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
