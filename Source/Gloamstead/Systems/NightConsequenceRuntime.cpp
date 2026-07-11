#include "Systems/NightConsequenceRuntime.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/NightStrategy.h"
#include "Systems/NightPressureActor.h"
#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

UNightConsequenceRuntime::UNightConsequenceRuntime()
{
	// Native strategy mapping (constructor so it exists for NewObject'd instances too, not only after
	// Initialize). Blueprintable strategies mean designers can subclass these; a future data asset can
	// drive this map without changing the loop.
	StrategyClasses.Add(ENightConsequenceType::Tutorial, UNightTutorialStrategy::StaticClass());
	StrategyClasses.Add(ENightConsequenceType::Corruption, UNightCorruptionStrategy::StaticClass());
	StrategyClasses.Add(ENightConsequenceType::Omen, UNightOmenStrategy::StaticClass());
	StrategyClasses.Add(ENightConsequenceType::Retrieval, UNightRetrievalStrategy::StaticClass());
}

void UNightConsequenceRuntime::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceManager* Manager = World->GetSubsystem<UNightConsequenceManager>())
		{
			Manager->OnNightPlanReady.AddDynamic(this, &UNightConsequenceRuntime::HandleNightPlanReady);
		}
		if (UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>())
		{
			PCG->OnStructureRestored.AddDynamic(this, &UNightConsequenceRuntime::HandleRestorationDuringNight);
		}
	}
}

void UNightConsequenceRuntime::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PressureTimer);

		if (UNightConsequenceManager* Manager = World->GetSubsystem<UNightConsequenceManager>())
		{
			Manager->OnNightPlanReady.RemoveDynamic(this, &UNightConsequenceRuntime::HandleNightPlanReady);
		}
		if (UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>())
		{
			PCG->OnStructureRestored.RemoveDynamic(this, &UNightConsequenceRuntime::HandleRestorationDuringNight);
		}
	}

	Super::Deinitialize();
}

void UNightConsequenceRuntime::HandleNightPlanReady(ENightConsequenceType SelectedNightType)
{
	PlannedNightType = SelectedNightType;
	UE_LOG(LogTemp, Log, TEXT("NightRuntime: Plan ready for %s"),
		*GetNightConsequenceTypeDisplayName(PlannedNightType));
}

bool UNightConsequenceRuntime::IsObjectiveResolved() const
{
	return ActiveStrategy ? ActiveStrategy->IsObjectiveResolved() : false;
}

FNightRuntimeContext UNightConsequenceRuntime::BuildContext(UGloamsteadPCGSubsystem* PCG) const
{
	FNightRuntimeContext Ctx;
	Ctx.NightType = PlannedNightType;

	if (PCG)
	{
		Ctx.DuskSnapshot = PCG->BuildSanctuarySnapshot();
		Ctx.TargetPointIndex = PCG->FindMostCorruptedPointIndex(/*bOnlyUnrestored*/ true);
		if (Ctx.TargetPointIndex >= 0)
		{
			Ctx.TargetStartCorruption = PCG->GetCorruptionLevel(Ctx.TargetPointIndex);
		}
	}

	// The player "heeded the warning" if the Veil Heart recorded a satisfied warning tag this cycle.
	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> Hearts;
		UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
		for (AActor* Actor : Hearts)
		{
			if (const AVeilHeart* Heart = Cast<AVeilHeart>(Actor))
			{
				Ctx.bWarningHeeded = Heart->GetSatisfiedWarningTagCount() > 0;
				break;
			}
		}
	}

	return Ctx;
}

TSubclassOf<UNightStrategy> UNightConsequenceRuntime::ResolveStrategyClass(ENightConsequenceType Type) const
{
	if (const TSubclassOf<UNightStrategy>* Found = StrategyClasses.Find(Type))
	{
		if (*Found)
		{
			return *Found;
		}
	}
	return UNightStrategy::StaticClass(); // benign quiet night for unsupported types
}

UNightStrategy* UNightConsequenceRuntime::Test_MakeStrategyFor(ENightConsequenceType Type)
{
	return NewObject<UNightStrategy>(this, ResolveStrategyClass(Type));
}

void UNightConsequenceRuntime::BeginNight()
{
	if (bNightActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("NightRuntime: BeginNight ignored — night already active."));
		return;
	}

	UWorld* World = GetWorld();
	UGloamsteadPCGSubsystem* PCG = World ? World->GetSubsystem<UGloamsteadPCGSubsystem>() : nullptr;

	if (PlannedNightType == ENightConsequenceType::Invalid)
	{
		if (World)
		{
			if (UNightConsequenceManager* Manager = World->GetSubsystem<UNightConsequenceManager>())
			{
				PlannedNightType = Manager->GetLastSelectedNightType();
			}
		}
	}

	ActiveNightType = PlannedNightType;
	bNightActive = true;
	LastOutcome = FNightRuntimeOutcome();

	ActiveContext = BuildContext(PCG);

	const TSubclassOf<UNightStrategy> StrategyClass = ResolveStrategyClass(ActiveNightType);
	ActiveStrategy = NewObject<UNightStrategy>(this, StrategyClass);
	ActiveStrategy->EnterNight(ActiveContext, PCG);

	UE_LOG(LogTemp, Log, TEXT("NightRuntime: Night started — type %s, strategy %s%s"),
		*GetNightConsequenceTypeDisplayName(ActiveNightType),
		*GetNameSafe(ActiveStrategy),
		ActiveContext.bWarningHeeded ? TEXT(" (warning heeded)") : TEXT(" (warning missed)"));

	OnNightStarted.Broadcast(ActiveNightType);
	BroadcastOmenClueIfNeeded();
	MaybeSpawnPressureActor(PCG);

	// Immediate first pressure beat, then a repeating cadence for the rest of the night.
	HandlePressureStep();
	if (World && PressureStepSeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(PressureTimer, this, &UNightConsequenceRuntime::HandlePressureStep,
			PressureStepSeconds, /*bLoop*/ true);
	}
}

void UNightConsequenceRuntime::BroadcastOmenClueIfNeeded()
{
	if (ActiveNightType != ENightConsequenceType::Omen)
	{
		return;
	}

	FName ClueTag = FName(TEXT("DefaultOmen"));
	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceManager* Manager = World->GetSubsystem<UNightConsequenceManager>())
		{
			const FName CatalogTag = Manager->GetOmenClueTagForNightType(ENightConsequenceType::Omen);
			if (CatalogTag != NAME_None)
			{
				ClueTag = CatalogTag;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("NightRuntime: Omen night — clue tag %s"), *ClueTag.ToString());
	OnOmenClueReady.Broadcast(ClueTag);
}

void UNightConsequenceRuntime::HandlePressureStep()
{
	if (!bNightActive || !ActiveStrategy)
	{
		return;
	}

	UWorld* World = GetWorld();
	UGloamsteadPCGSubsystem* PCG = World ? World->GetSubsystem<UGloamsteadPCGSubsystem>() : nullptr;
	ActiveStrategy->ApplyPressureStep(PCG);
}

void UNightConsequenceRuntime::HandleRestorationDuringNight(const FRestorationEventPayload& Payload)
{
	if (!bNightActive || !ActiveStrategy)
	{
		return;
	}

	UWorld* World = GetWorld();
	UGloamsteadPCGSubsystem* PCG = World ? World->GetSubsystem<UGloamsteadPCGSubsystem>() : nullptr;

	const bool bWasResolved = ActiveStrategy->IsObjectiveResolved();
	ActiveStrategy->NotifyRestoration(Payload, PCG);

	if (!bWasResolved && ActiveStrategy->IsObjectiveResolved())
	{
		// Intentional end condition: the player resolved the objective before dawn.
		if (World)
		{
			World->GetTimerManager().ClearTimer(PressureTimer);
		}
		UE_LOG(LogTemp, Log, TEXT("NightRuntime: objective resolved — requesting early dawn."));
		OnNightShouldEnd.Broadcast();
	}
}

void UNightConsequenceRuntime::EndNight()
{
	if (!bNightActive)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PressureTimer);
	}

	const ENightConsequenceType EndedType = ActiveNightType;

	UGloamsteadPCGSubsystem* PCG = nullptr;
	if (UWorld* World = GetWorld())
	{
		PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	}

	if (ActiveStrategy)
	{
		LastOutcome = ActiveStrategy->ResolveNight(PCG);
	}
	else
	{
		LastOutcome = FNightRuntimeOutcome();
		LastOutcome.NightType = EndedType;
	}

	UE_LOG(LogTemp, Log, TEXT("NightRuntime: Night ended — type %s, outcome %s (tag %s)"),
		*GetNightConsequenceTypeDisplayName(EndedType),
		*GetNightOutcomeResultDisplayName(LastOutcome.Result),
		*LastOutcome.ResultTag.ToString());

	OnNightEnded.Broadcast(EndedType);

	DestroyPressureActor();

	bNightActive = false;
	ActiveNightType = ENightConsequenceType::Invalid;
	ActiveStrategy = nullptr;
}

void UNightConsequenceRuntime::MaybeSpawnPressureActor(UGloamsteadPCGSubsystem* PCG)
{
	if (!bSpawnPressureActor)
	{
		return;
	}

	UWorld* World = GetWorld();
	// Cosmetic feel layer only — never spawn during automation/editor worlds.
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Only threat nights get a pressure presence (Corruption bloom, Retrieval reclaim).
	if (ActiveNightType != ENightConsequenceType::Corruption && ActiveNightType != ENightConsequenceType::Retrieval)
	{
		return;
	}

	// Bind to the active strategy's chosen target when it has one (Retrieval's target is a restored point,
	// not the context's most-corrupted point); fall back to the context target otherwise.
	int32 BoundIndex = ActiveContext.TargetPointIndex;
	if (ActiveStrategy)
	{
		const int32 ObjectiveTarget = ActiveStrategy->GetObjective().TargetPointIndex;
		if (ObjectiveTarget >= 0)
		{
			BoundIndex = ObjectiveTarget;
		}
	}

	FVector SpawnLocation = FVector::ZeroVector;
	if (PCG && BoundIndex >= 0)
	{
		FPCGPoint TargetPoint;
		if (PCG->GetPointByIndex(BoundIndex, TargetPoint))
		{
			SpawnLocation = TargetPoint.Transform.GetLocation();
		}
	}

	TSubclassOf<ANightPressureActor> SpawnClass = PressureActorClass;
	if (!SpawnClass)
	{
		SpawnClass = ANightPressureActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActivePressureActor = World->SpawnActor<ANightPressureActor>(SpawnClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (ActivePressureActor)
	{
		ActivePressureActor->BoundPointIndex = BoundIndex;
		UE_LOG(LogTemp, Log, TEXT("NightRuntime: spawned pressure actor at %s (target %d)."),
			*SpawnLocation.ToString(), BoundIndex);
	}
}

void UNightConsequenceRuntime::DestroyPressureActor()
{
	if (ActivePressureActor)
	{
		ActivePressureActor->Destroy();
		ActivePressureActor = nullptr;
	}
}
