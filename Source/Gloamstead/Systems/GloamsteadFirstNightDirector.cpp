#include "Systems/GloamsteadFirstNightDirector.h"

#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

AGloamsteadFirstNightDirector::AGloamsteadFirstNightDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGloamsteadFirstNightDirector::BeginPlay()
{
	Super::BeginPlay();

	ResolveWorldSystems();
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

void AGloamsteadFirstNightDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DuskToNightTimer);
		World->GetTimerManager().ClearTimer(NightDurationTimer);
	}
	UnbindDelegates();

	Super::EndPlay(EndPlayReason);
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
	// The Heart owns warning selection/voice; the director only triggers it (step 2).
	if (AVeilHeart* Heart = CachedHeart.Get())
	{
		Heart->EmitWarningForNight(FirstNightType);
	}

	++Test_WarningCount;
	OnFirstNightWarning();
}

void AGloamsteadFirstNightDirector::PresentLanternTarget()
{
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
	CurrentBeat = EFirstNightBeat::LanternRestored;
	++Test_LanternRestoredCount;

	UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: lantern restored at %s — unlocking dusk."), *Payload.WorldLocation.ToString());

	OnLanternRestored(Payload.WorldLocation);
	RequestAdvanceToDusk();
}

void AGloamsteadFirstNightDirector::HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase)
{
	switch (NewPhase)
	{
	case EGloamsteadDayPhase::Dusk:
		CurrentBeat = EFirstNightBeat::Dusk;
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
		PresentDawnPayoff();
		CurrentBeat = EFirstNightBeat::Complete;
		UE_LOG(LogTemp, Log, TEXT("FirstNightDirector: first-night loop complete."));
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
