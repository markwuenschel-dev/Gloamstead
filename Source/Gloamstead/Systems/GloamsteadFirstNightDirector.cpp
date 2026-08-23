#include "Systems/GloamsteadFirstNightDirector.h"

#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

AGloamsteadFirstNightDirector::AGloamsteadFirstNightDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGloamsteadFirstNightDirector::BeginPlay()
{
	Super::BeginPlay();

	ResolveWorldSystems();

	if (!CachedDayNight)
	{
		UE_LOG(LogTemp, Warning, TEXT("FirstNightDirector '%s': no UGloamsteadDayNightSubsystem; cannot drive the first-night loop."), *GetName());
		DetachTutorial();
		return;
	}

	// First night only: drive the loop when the world opens on Day before any night has passed.
	if (CachedDayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day && CachedDayNight->GetNightCount() == 0)
	{
		CaptureLanternMarker();
		BindDelegates();
		BeginIntro();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("FirstNightDirector '%s': not first-night Day (phase=%d night=%d); staying dormant."),
			*GetName(), static_cast<int32>(CachedDayNight->GetCurrentPhase()), CachedDayNight->GetNightCount());
		DetachTutorial();
	}
}

void AGloamsteadFirstNightDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bTutorialDetached && bMarkerVisible)
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
	DetachTutorial();

	Super::EndPlay(EndPlayReason);
}

void AGloamsteadFirstNightDirector::CaptureLanternMarker()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> Actors;
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
	if (bTutorialDetached || bDelegatesBound)
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
	if (bTutorialDetached)
	{
		return;
	}
	CachedPCG = InPCG;
	CachedDayNight = InDayNight;
	CachedRuntime = InRuntime;
	BindDelegates();
}

void AGloamsteadFirstNightDirector::BeginIntro()
{
	if (bTutorialDetached || bIntroPresented)
	{
		return;
	}
	bIntroPresented = true;
	CurrentBeat = EFirstNightBeat::Intro;

	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: Day intro — presenting warning and lantern target."));

	PresentWarning();
	PresentLanternTarget();
}

void AGloamsteadFirstNightDirector::DetachTutorial()
{
	if (bTutorialDetached)
	{
		return;
	}

	// DayNight owns every Dusk/Night cadence timer. The tutorial actor owns no
	// remaining timer after this handoff, so detaching makes any later Cycle II
	// callback inert instead of trying to clean up a second phase authority.
	bTutorialDetached = true;
	SetLanternMarkerVisible(false);
	UnbindDelegates();
	CurrentBeat = EFirstNightBeat::Complete;
	SetActorTickEnabled(false);
}

void AGloamsteadFirstNightDirector::DetachForProgressionResume()
{
	// A validated later-cycle Day save is conclusive evidence that this live
	// Cycle I lesson is stale. Detach before DayNight retries the new warning so
	// the Heart never treats tutorial UI as the later-cycle player-facing surface.
	DetachTutorial();
}

void AGloamsteadFirstNightDirector::PresentWarning()
{
	// Deliberately does NOT emit a warning here. DayNight exposes the exact authored
	// fragment during Day only after the lantern gate and registered presenter are
	// ready. The intro keeps its own local caption beat without creating a duplicate
	// or hard-coded warning identity.
	++Test_WarningCount;
	OnFirstNightWarning();
}

void AGloamsteadFirstNightDirector::HandleHeartWarning(const FVeilHeartWarningFragment& WarningFragment)
{
	if (bTutorialDetached || WarningFragment.AssociatedNightType != FirstNightType)
	{
		return;
	}
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
	if (bTutorialDetached || CurrentBeat != EFirstNightBeat::Night || Outcome.NightType != FirstNightType)
	{
		return;
	}
	const FText Reflection = BuildDawnReflectionText(Outcome);
	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: dawn reflection — %s"), *Reflection.ToString());
	++Test_HeartReflectionCount;
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
	if (bTutorialDetached)
	{
		return;
	}
	PresentDawnPayoff();
	CurrentBeat = EFirstNightBeat::Complete;
	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: first-night loop complete."));
	DetachTutorial();
}

float AGloamsteadFirstNightDirector::ComputeLanternInfluence() const
{
	// Read-only: the lit path's average light is how strongly the lantern resists encroachment.
	return CachedPCG ? CachedPCG->GetSanctuaryAverageLightLevel() : 0.0f;
}

void AGloamsteadFirstNightDirector::HandleStructureRestored(const FRestorationEventPayload& Payload)
{
	// Only the first qualifying restoration during the Day intro unlocks the night.
	if (bTutorialDetached || bLanternRestored || CurrentBeat != EFirstNightBeat::Intro)
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
	if (bTutorialDetached)
	{
		return;
	}

	switch (NewPhase)
	{
	case EGloamsteadDayPhase::Dusk:
		if (OldPhase != EGloamsteadDayPhase::Day || CurrentBeat != EFirstNightBeat::LanternRestored)
		{
			return;
		}
		CurrentBeat = EFirstNightBeat::Dusk;
		PresentDuskCue();
		break;

	case EGloamsteadDayPhase::Dawn:
		// DayNight may reach a valid first dawn even if a degraded runtime did not
		// emit OnNightStarted. The completed lantern lesson, not the presentation
		// callback ordering, is what makes this actor permanently relinquish Cycle II.
		if (OldPhase != EGloamsteadDayPhase::Night || !bLanternRestored)
		{
			return;
		}
		CurrentBeat = EFirstNightBeat::Dawn;
		UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: Cycle I dawn payoff complete; relinquishing tutorial control."));
		CompleteDawn();
		break;

	default:
		// Night is keyed off the night runtime's OnNightStarted (see HandleNightStarted), and the
		// director never returns to Day during the first-night loop.
		break;
	}
}

void AGloamsteadFirstNightDirector::HandleNightStarted(ENightConsequenceType NightType)
{
	if (bTutorialDetached || CurrentBeat != EFirstNightBeat::Dusk || NightType != FirstNightType)
	{
		return;
	}
	ObservedNightType = NightType;
	CurrentBeat = EFirstNightBeat::Night;

	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: night started — type %s; beginning encroachment."),
		*GetNightConsequenceTypeDisplayName(NightType));

	PresentEncroachment();
}

void AGloamsteadFirstNightDirector::HandleNightShouldEnd()
{
	++Test_LegacyEarlyDawnCallbackCount;
	UE_LOG(LogTemp, Verbose, TEXT("FirstNightDirector: ignoring legacy early-objective callback; DayNight owns the dawn transition."));
}

void AGloamsteadFirstNightDirector::RequestAdvanceToDusk()
{
	// The dusk gate: night cannot begin until the lantern is restored.
	if (bTutorialDetached || !bLanternRestored)
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
	// Kept for Blueprint ABI compatibility only. DayNight's Dusk cadence is the
	// sole Dusk->Night authority; allowing the tutorial actor through here would
	// bypass the readable preparation interval on both Cycle I and later saves.
	UE_LOG(LogTemp, Verbose, TEXT("FirstNightDirector: ignoring legacy Dusk->Night request; DayNight owns cadence."));
}

void AGloamsteadFirstNightDirector::RequestAdvanceToDawn()
{
	// Kept for Blueprint ABI compatibility only. DayNight owns the deadline and
	// runtime early-objective path, including its exactly-once Dawn guard.
	UE_LOG(LogTemp, Verbose, TEXT("FirstNightDirector: ignoring legacy Night->Dawn request; DayNight owns cadence."));
}
