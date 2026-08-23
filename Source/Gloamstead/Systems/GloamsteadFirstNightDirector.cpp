#include "Systems/GloamsteadFirstNightDirector.h"

#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

AGloamsteadFirstNightDirector::AGloamsteadFirstNightDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGloamsteadFirstNightDirector::BeginPlay()
{
	Super::BeginPlay();

	ResolveWorldSystems();
	CapturePresentationActors();
	BindDelegates();

	if (!CachedDayNight)
	{
		UE_LOG(LogTemp, Warning, TEXT("FirstNightDirector '%s': no UGloamsteadDayNightSubsystem; cannot drive the first-night loop."), *GetName());
		return;
	}

	// First night only: drive the loop when the world opens on Day before any night has passed.
	if (CachedDayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day && CachedDayNight->GetNightCount() == 0)
	{
		BeginIntro();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("FirstNightDirector '%s': not first-night Day (phase=%d night=%d); staying dormant."),
			*GetName(), static_cast<int32>(CachedDayNight->GetCurrentPhase()), CachedDayNight->GetNightCount());
	}
}

void AGloamsteadFirstNightDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDuskTransitionActive || bDawnTransitionActive)
	{
		TransitionElapsed += DeltaSeconds;
		const float Alpha = TransitionDuration > 0.0f
			? FMath::Clamp(TransitionElapsed / TransitionDuration, 0.0f, 1.0f)
			: 1.0f;
		const float SmoothedAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);

		ApplyPresentationValues(
			FMath::Lerp(TransitionStartSunIntensity, TransitionTargetSunIntensity, SmoothedAlpha),
			FMath::Lerp(TransitionStartSkyIntensity, TransitionTargetSkyIntensity, SmoothedAlpha),
			FMath::Lerp(TransitionStartSunColor, TransitionTargetSunColor, SmoothedAlpha),
			FMath::Lerp(TransitionStartSkyColor, TransitionTargetSkyColor, SmoothedAlpha),
			FMath::Lerp(TransitionStartFogDensity, TransitionTargetFogDensity, SmoothedAlpha));

		if (Alpha >= 1.0f)
		{
			const bool bFinishedDawn = bDawnTransitionActive;
			bDuskTransitionActive = false;
			bDawnTransitionActive = false;
			if (bFinishedDawn)
			{
				CompleteDawn();
			}
		}
	}

	if (bMarkerVisible)
	{
		MarkerPulseElapsed += DeltaSeconds;
		const float Pulse = 0.82f + 0.18f * FMath::Sin(MarkerPulseElapsed * 3.25f);
		for (const TWeakObjectPtr<UPointLightComponent>& MarkerLight : MarkerLights)
		{
			if (UPointLightComponent* Light = MarkerLight.Get())
			{
				Light->SetIntensity(140.0f * Pulse);
			}
		}
		const FLinearColor Amber = FLinearColor(1.1f, 0.48f, 0.12f) * Pulse;
		for (UMaterialInstanceDynamic* Material : MarkerMaterials)
		{
			if (Material)
			{
				Material->SetVectorParameterValue(TEXT("Color"), Amber);
			}
		}
	}
}

void AGloamsteadFirstNightDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DuskToNightTimer);
		World->GetTimerManager().ClearTimer(NightDurationTimer);
	}
	RestoreCapturedPresentation();
	UnbindDelegates();

	Super::EndPlay(EndPlayReason);
}

void AGloamsteadFirstNightDirector::CapturePresentationActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsWithTag(World, TEXT("Gloamstead.FirstNight.Sun"), Actors);
	if (Actors.Num() > 0)
	{
		if (const ADirectionalLight* Sun = Cast<ADirectionalLight>(Actors[0]))
		{
			CachedSun = Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
		}
	}

	Actors.Reset();
	UGameplayStatics::GetAllActorsWithTag(World, TEXT("Gloamstead.FirstNight.SkyLight"), Actors);
	if (Actors.Num() > 0)
	{
		if (const ASkyLight* Sky = Cast<ASkyLight>(Actors[0]))
		{
			CachedSkyLight = Sky->GetLightComponent();
		}
	}

	Actors.Reset();
	UGameplayStatics::GetAllActorsWithTag(World, TEXT("Gloamstead.FirstNight.Fog"), Actors);
	if (Actors.Num() > 0)
	{
		if (const AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(Actors[0]))
		{
			CachedFog = Fog->GetComponent();
		}
	}

	bPresentationCaptured = CachedSun && CachedSkyLight && CachedFog;
	if (bPresentationCaptured)
	{
		CapturedSunIntensity = CachedSun->Intensity;
		CapturedSkyIntensity = CachedSkyLight->Intensity;
		CapturedFogDensity = CachedFog->FogDensity;
		CapturedSunColor = CachedSun->LightColor;
		CapturedSkyColor = CachedSkyLight->LightColor;
	}

	Actors.Reset();
	UGameplayStatics::GetAllActorsWithTag(World, TEXT("Gloamstead.FirstLantern.Anchor"), Actors);
	for (AActor* Anchor : Actors)
	{
		TArray<UActorComponent*> Components;
		Anchor->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (!Component || !Component->ComponentHasTag(TEXT("Gloamstead.FirstLantern.Marker")))
			{
				continue;
			}
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				MarkerComponents.Add(SceneComponent);
			}
			if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(Component))
			{
				MarkerLights.Add(PointLight);
			}
			if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component))
			{
				if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
				{
					MarkerMaterials.Add(Material);
				}
			}
		}
	}
	SetLanternMarkerVisible(false);
}

void AGloamsteadFirstNightDirector::RestoreCapturedPresentation()
{
	if (bPresentationCaptured)
	{
		ApplyPresentationValues(CapturedSunIntensity, CapturedSkyIntensity, CapturedSunColor,
			CapturedSkyColor, CapturedFogDensity);
	}
}

void AGloamsteadFirstNightDirector::SetLanternMarkerVisible(bool bVisible)
{
	bMarkerVisible = bVisible;
	MarkerPulseElapsed = 0.0f;
	for (const TWeakObjectPtr<USceneComponent>& MarkerComponent : MarkerComponents)
	{
		if (USceneComponent* Component = MarkerComponent.Get())
		{
			Component->SetVisibility(bVisible, true);
		}
	}
}

void AGloamsteadFirstNightDirector::ApplyPresentationValues(float SunIntensity, float SkyIntensity,
	const FLinearColor& SunColor, const FLinearColor& SkyColor, float FogDensity)
{
	if (!bPresentationCaptured)
	{
		return;
	}
	CachedSun->SetIntensity(SunIntensity);
	CachedSun->SetLightColor(SunColor, false);
	CachedSkyLight->SetIntensity(SkyIntensity);
	CachedSkyLight->SetLightColor(SkyColor);
	CachedFog->SetFogDensity(FogDensity);
}

void AGloamsteadFirstNightDirector::BeginDuskPresentation()
{
	if (!bPresentationCaptured)
	{
		return;
	}
	bDawnTransitionActive = false;
	bDuskTransitionActive = true;
	TransitionElapsed = 0.0f;
	TransitionDuration = DuskToNightDelaySeconds;
	TransitionStartSunIntensity = CachedSun->Intensity;
	TransitionStartSkyIntensity = CachedSkyLight->Intensity;
	TransitionStartFogDensity = CachedFog->FogDensity;
	TransitionStartSunColor = CachedSun->LightColor;
	TransitionStartSkyColor = CachedSkyLight->LightColor;
	TransitionTargetSunIntensity = CapturedSunIntensity * 0.35f;
	TransitionTargetSkyIntensity = CapturedSkyIntensity * 0.55f;
	TransitionTargetFogDensity = CapturedFogDensity;
	TransitionTargetSunColor = DuskSunTint;
	TransitionTargetSkyColor = CapturedSkyColor;
}

void AGloamsteadFirstNightDirector::ApplyNightPresentation()
{
	bDuskTransitionActive = false;
	bDawnTransitionActive = false;
	ApplyPresentationValues(CapturedSunIntensity * 0.08f, CapturedSkyIntensity * 0.25f,
		NightAmbientTint, NightAmbientTint, CapturedFogDensity + 0.015f);
}

bool AGloamsteadFirstNightDirector::BeginDawnPresentation()
{
	if (!bPresentationCaptured || DawnTransitionSeconds <= 0.0f)
	{
		RestoreCapturedPresentation();
		return false;
	}
	bDuskTransitionActive = false;
	bDawnTransitionActive = true;
	TransitionElapsed = 0.0f;
	TransitionDuration = DawnTransitionSeconds;
	TransitionStartSunIntensity = CachedSun->Intensity;
	TransitionStartSkyIntensity = CachedSkyLight->Intensity;
	TransitionStartFogDensity = CachedFog->FogDensity;
	TransitionStartSunColor = CachedSun->LightColor;
	TransitionStartSkyColor = CachedSkyLight->LightColor;
	TransitionTargetSunIntensity = CapturedSunIntensity;
	TransitionTargetSkyIntensity = CapturedSkyIntensity;
	TransitionTargetFogDensity = CapturedFogDensity;
	TransitionTargetSunColor = CapturedSunColor;
	TransitionTargetSkyColor = CapturedSkyColor;
	return true;
}

void AGloamsteadFirstNightDirector::ResolveWorldSystems()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CachedPCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	CachedDayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>();
	CachedRuntime = World->GetSubsystem<UNightConsequenceRuntime>();

	TArray<AActor*> Hearts;
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	if (Hearts.Num() > 0)
	{
		CachedHeart = Cast<AVeilHeart>(Hearts[0]);
	}
}

void AGloamsteadFirstNightDirector::BindDelegates()
{
	if (bDelegatesBound)
	{
		return;
	}

	if (CachedPCG)
	{
		CachedPCG->OnStructureRestored.AddDynamic(this, &AGloamsteadFirstNightDirector::HandleStructureRestored);
	}
	if (CachedDayNight)
	{
		CachedDayNight->OnPhaseChanged.AddDynamic(this, &AGloamsteadFirstNightDirector::HandlePhaseChanged);
	}
	if (CachedRuntime)
	{
		CachedRuntime->OnNightStarted.AddDynamic(this, &AGloamsteadFirstNightDirector::HandleNightStarted);
		CachedRuntime->OnNightShouldEnd.AddDynamic(this, &AGloamsteadFirstNightDirector::HandleNightShouldEnd);
	}
	if (AVeilHeart* Heart = CachedHeart.Get())
	{
		// The Heart speaks; the director captions. Without these the dusk warning and the dawn
		// reflection existed only as log lines.
		Heart->OnWarningEmittedDelegate.AddDynamic(this, &AGloamsteadFirstNightDirector::HandleHeartWarning);
		if (!Heart->RegisterWarningPresenter(this, GET_FUNCTION_NAME_CHECKED(AGloamsteadFirstNightDirector, HandleHeartWarning)))
		{
			UE_LOG(LogTemp, Warning, TEXT("FirstNightDirector '%s': Heart warning presenter registration failed; rest remains safely gated."), *GetName());
		}
		Heart->OnDawnReflectionDelegate.AddDynamic(this, &AGloamsteadFirstNightDirector::HandleHeartDawnReflection);
	}

	bDelegatesBound = true;
}

void AGloamsteadFirstNightDirector::UnbindDelegates()
{
	if (!bDelegatesBound)
	{
		return;
	}

	if (CachedPCG)
	{
		CachedPCG->OnStructureRestored.RemoveDynamic(this, &AGloamsteadFirstNightDirector::HandleStructureRestored);
	}
	if (CachedDayNight)
	{
		CachedDayNight->OnPhaseChanged.RemoveDynamic(this, &AGloamsteadFirstNightDirector::HandlePhaseChanged);
	}
	if (CachedRuntime)
	{
		CachedRuntime->OnNightStarted.RemoveDynamic(this, &AGloamsteadFirstNightDirector::HandleNightStarted);
		CachedRuntime->OnNightShouldEnd.RemoveDynamic(this, &AGloamsteadFirstNightDirector::HandleNightShouldEnd);
	}
	if (AVeilHeart* Heart = CachedHeart.Get())
	{
		Heart->OnWarningEmittedDelegate.RemoveDynamic(this, &AGloamsteadFirstNightDirector::HandleHeartWarning);
		Heart->UnregisterWarningPresenter(this);
		Heart->OnDawnReflectionDelegate.RemoveDynamic(this, &AGloamsteadFirstNightDirector::HandleHeartDawnReflection);
	}

	bDelegatesBound = false;
}

void AGloamsteadFirstNightDirector::Test_BindTo(UGloamsteadPCGSubsystem* InPCG, UGloamsteadDayNightSubsystem* InDayNight, UNightConsequenceRuntime* InRuntime)
{
	CachedPCG = InPCG;
	CachedDayNight = InDayNight;
	CachedRuntime = InRuntime;
	BindDelegates();
}

void AGloamsteadFirstNightDirector::BeginIntro()
{
	if (bIntroPresented)
	{
		return;
	}
	bIntroPresented = true;
	CurrentBeat = EFirstNightBeat::Intro;

	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: Day intro — presenting warning and lantern target."));

	PresentWarning();
	PresentLanternTarget();
}

void AGloamsteadFirstNightDirector::PresentWarning()
{
	// Deliberately does NOT call EmitWarningForNight here. The phase authority emits the warning at
	// dusk with the night the catalog actually selected (GloamsteadDayNightSubsystem::HandleEnterDusk);
	// emitting again on the Day intro produced the warning twice, the first time for a hardcoded night
	// type that may not be the one that runs. The Day intro keeps its own caption beat instead, and the
	// Heart's real words arrive at dusk through HandleHeartWarning.
	++Test_WarningCount;
	OnFirstNightWarning();
}

void AGloamsteadFirstNightDirector::HandleHeartWarning(const FVeilHeartWarningFragment& WarningFragment)
{
	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: captioning Heart warning [%s]."), *WarningFragment.WarningId.ToString());
	OnHeartWarning(WarningFragment.Fragment);
}

FText AGloamsteadFirstNightDirector::BuildDawnReflectionText(const FNightRuntimeOutcome& Outcome) const
{
	// Beat 13: the reflection has to close the loop the player just lived — they were warned, they
	// restored the lantern, and the night answered. Each branch names all three.
	if (!bLanternRestored)
	{
		return NSLOCTEXT("Gloamstead", "DawnNoLantern",
			"Dawn. The Heart's warning went unanswered, and the dark kept what it took.");
	}

	switch (Outcome.Result)
	{
	case ENightOutcomeResult::Success:
		if (Outcome.ResultTag == FName(TEXT("TutorialSheltered")))
		{
			return NSLOCTEXT("Gloamstead", "DawnSheltered",
				"Dawn. The Heart warned of the dark; you raised the lantern and stood in its light, and the night broke against it.");
		}
		return NSLOCTEXT("Gloamstead", "DawnHeld",
			"Dawn. The Heart warned of the dark; the lantern you raised held the sanctuary through it.");

	case ENightOutcomeResult::Partial:
		return NSLOCTEXT("Gloamstead", "DawnExposed",
			"Dawn. The lantern you raised burned all night — but you spent it in the dark beyond its reach. The sanctuary held; you did not.");

	case ENightOutcomeResult::Failure:
		return NSLOCTEXT("Gloamstead", "DawnScarred",
			"Dawn. The warning was plain and the lantern was lit, yet the dark took ground. A scar remains.");

	case ENightOutcomeResult::None:
	default:
		return NSLOCTEXT("Gloamstead", "DawnQuiet",
			"Dawn. The lantern you raised still burns, and the night passed without answer.");
	}
}

void AGloamsteadFirstNightDirector::HandleHeartDawnReflection(const FNightRuntimeOutcome& Outcome)
{
	const FText Reflection = BuildDawnReflectionText(Outcome);
	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: dawn reflection — %s"), *Reflection.ToString());
	OnHeartReflection(Reflection);
}

void AGloamsteadFirstNightDirector::PresentLanternTarget()
{
	SetLanternMarkerVisible(true);
	++Test_LanternTargetCount;
	OnLanternTargetReadable();
}

void AGloamsteadFirstNightDirector::PresentDuskCue()
{
	++Test_DuskCueCount;
	OnDuskCue();
}

void AGloamsteadFirstNightDirector::PresentEncroachment()
{
	++Test_EncroachmentCount;
	OnEncroachmentBegan(ComputeLanternInfluence());
}

void AGloamsteadFirstNightDirector::PresentDawnPayoff()
{
	++Test_DawnPayoffCount;
	OnDawnPayoff();
}

void AGloamsteadFirstNightDirector::CompleteDawn()
{
	RestoreCapturedPresentation();
	PresentDawnPayoff();
	CurrentBeat = EFirstNightBeat::Complete;
	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: first-night loop complete."));
}

float AGloamsteadFirstNightDirector::ComputeLanternInfluence() const
{
	// Read-only: the lit path's average light is how strongly the lantern resists encroachment.
	return CachedPCG ? CachedPCG->GetSanctuaryAverageLightLevel() : 0.0f;
}

void AGloamsteadFirstNightDirector::HandleStructureRestored(const FRestorationEventPayload& Payload)
{
	// Only the first qualifying restoration during the Day intro unlocks the night.
	if (bLanternRestored || CurrentBeat != EFirstNightBeat::Intro)
	{
		return;
	}
	if (Payload.RitualType != RequiredRestorationType)
	{
		return;
	}

	bLanternRestored = true;
	SetLanternMarkerVisible(false);
	CurrentBeat = EFirstNightBeat::LanternRestored;
	++Test_LanternRestoredCount;

	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: lantern restored at %s — unlocking rest at the Heart."), *Payload.WorldLocation.ToString());

	OnLanternRestored(Payload.WorldLocation);

	// Hand the Day->Dusk transition to the player rather than taking it. The dusk gate is satisfied,
	// so the Heart becomes rest-able; resting there is what brings the night. RequestAdvanceToDusk()
	// remains available (and still gated on bLanternRestored) for tests and presentation Blueprints.
	if (CachedDayNight)
	{
		CachedDayNight->UnlockFirstRest();
	}
}

void AGloamsteadFirstNightDirector::HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase)
{
	switch (NewPhase)
	{
	case EGloamsteadDayPhase::Dusk:
		CurrentBeat = EFirstNightBeat::Dusk;
		BeginDuskPresentation();
		PresentDuskCue();
		if (UWorld* World = GetWorld())
		{
			if (DuskToNightDelaySeconds > 0.0f)
			{
				World->GetTimerManager().SetTimer(DuskToNightTimer, this, &AGloamsteadFirstNightDirector::RequestAdvanceToNight, DuskToNightDelaySeconds, false);
			}
			else
			{
				RequestAdvanceToNight();
			}
		}
		break;

	case EGloamsteadDayPhase::Dawn:
		CurrentBeat = EFirstNightBeat::Dawn;
		UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: dawn transition started."));
		if (!BeginDawnPresentation())
		{
			CompleteDawn();
		}
		break;

	default:
		// Night is keyed off the night runtime's OnNightStarted (see HandleNightStarted), and the
		// director never returns to Day during the first-night loop.
		break;
	}
}

void AGloamsteadFirstNightDirector::HandleNightStarted(ENightConsequenceType NightType)
{
	ObservedNightType = NightType;
	CurrentBeat = EFirstNightBeat::Night;
	ApplyNightPresentation();

	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: night started — type %s; beginning encroachment."),
		*GetNightConsequenceTypeDisplayName(NightType));

	PresentEncroachment();

	if (UWorld* World = GetWorld())
	{
		if (NightDurationSeconds > 0.0f)
		{
			World->GetTimerManager().SetTimer(NightDurationTimer, this, &AGloamsteadFirstNightDirector::RequestAdvanceToDawn, NightDurationSeconds, false);
		}
		else
		{
			RequestAdvanceToDawn();
		}
	}
}

void AGloamsteadFirstNightDirector::HandleNightShouldEnd()
{
	// Intentional early end: the player resolved the night objective before the deadline.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NightDurationTimer);
	}
	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: night objective resolved — advancing to dawn early."));
	RequestAdvanceToDawn();
}

void AGloamsteadFirstNightDirector::RequestAdvanceToDusk()
{
	// The dusk gate: night cannot begin until the lantern is restored.
	if (!bLanternRestored)
	{
		UE_LOG(LogTemp, Verbose, TEXT("FirstNightDirector: dusk request ignored — lantern not yet restored."));
		return;
	}
	if (CachedDayNight && CachedDayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day)
	{
		CachedDayNight->AdvanceToNextPhase();
	}
}

void AGloamsteadFirstNightDirector::RequestAdvanceToNight()
{
	if (CachedDayNight && CachedDayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dusk)
	{
		CachedDayNight->AdvanceToNextPhase();
	}
}

void AGloamsteadFirstNightDirector::RequestAdvanceToDawn()
{
	if (CachedDayNight && CachedDayNight->GetCurrentPhase() == EGloamsteadDayPhase::Night)
	{
		CachedDayNight->AdvanceToNextPhase();
	}
}
