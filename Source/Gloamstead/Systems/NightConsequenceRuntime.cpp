#include "Systems/NightConsequenceRuntime.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/NightStrategy.h"
#include "Systems/NightPressureActor.h"
#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Engine/GameInstance.h"
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
	AbortNightForRestore();
	if (UWorld* World = GetWorld())
	{
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

const FExperienceCyclePlan* UNightConsequenceRuntime::ResolveActiveAuthoredPlan() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UGloamsteadExperienceCycleSubsystem* Experience = GameInstance
		? GameInstance->GetSubsystem<UGloamsteadExperienceCycleSubsystem>()
		: nullptr;
	if (!Experience)
	{
		return nullptr;
	}

	const FExperienceCyclePlan& Plan = Experience->GetActivePlan();
	return Plan.IsAuthoredPlan() ? &Plan : nullptr;
}

int32 UNightConsequenceRuntime::ResolveSemanticSubjectToPoint(FName SemanticSubject, const UGloamsteadPCGSubsystem* PCG) const
{
	if (!PCG || SemanticSubject == NAME_None)
	{
		return INDEX_NONE;
	}

	int32 ResolvedIndex = INDEX_NONE;
	for (int32 PointIndex = 0; PointIndex < PCG->GetRitualPointCount(); ++PointIndex)
	{
		FPCGPoint Point;
		if (!PCG->GetPointByIndex(PointIndex, Point)
			|| PCG->GetNameAttribute(Point, TEXT("SemanticSubject"), NAME_None) != SemanticSubject)
		{
			continue;
		}

		if (ResolvedIndex != INDEX_NONE)
		{
			UE_LOG(LogTemp, Error, TEXT("NightRuntime: authored subject %s maps to multiple PCG points; Corruption will not target a substitute bloom."),
				*SemanticSubject.ToString());
			return INDEX_NONE;
		}
		ResolvedIndex = PointIndex;
	}

	if (ResolvedIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("NightRuntime: authored subject %s has no PCG mapping; Corruption will remain untargeted."),
			*SemanticSubject.ToString());
	}
	return ResolvedIndex;
}

int32 UNightConsequenceRuntime::ResolvePlanTargetToPoint(
	const FExperienceCyclePlan& Plan,
	const UGloamsteadPCGSubsystem* PCG,
	bool bRequireRestored) const
{
	if (!PCG || !Plan.IsAuthoredPlan())
	{
		return INDEX_NONE;
	}

	int32 ResolvedIndex = INDEX_NONE;
	for (int32 PointIndex = 0; PointIndex < PCG->GetRitualPointCount(); ++PointIndex)
	{
		if (!PCG->PointMatchesExperiencePlan(PointIndex, Plan)
			|| (bRequireRestored && !PCG->IsPointRestored(PointIndex)))
		{
			continue;
		}

		if (ResolvedIndex != INDEX_NONE)
		{
			UE_LOG(LogTemp, Error, TEXT("NightRuntime: full authored target contract for %s maps to multiple PCG points; the night will remain quiet."),
				*Plan.PlanId.ToString());
			return INDEX_NONE;
		}
		ResolvedIndex = PointIndex;
	}

	if (ResolvedIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("NightRuntime: no PCG point satisfies the full authored target contract for %s; the night will remain quiet."),
			*Plan.PlanId.ToString());
	}
	return ResolvedIndex;
}

FNightRuntimeContext UNightConsequenceRuntime::BuildContext(UGloamsteadPCGSubsystem* PCG) const
{
	FNightRuntimeContext Ctx;
	Ctx.NightType = PlannedNightType;
	const FExperienceCyclePlan* ActivePlan = ResolveActiveAuthoredPlan();

	if (PCG)
	{
		Ctx.DuskSnapshot = PCG->BuildSanctuarySnapshot();

		// An authored Corruption or Retrieval plan must name one real place. It
		// may never fall back to a score-selected target: missing/multiple
		// metadata is a visible fail-closed quiet threat, not a different garden
		// being punished. Retrieval additionally requires the place to still be
		// restored at dusk, because that is what the night is testing.
		if ((PlannedNightType == ENightConsequenceType::Corruption
			|| PlannedNightType == ENightConsequenceType::Retrieval)
			&& ActivePlan)
		{
			Ctx.bRequiresExactSemanticTarget = true;
			Ctx.RequiredWarningId = ActivePlan->WarningId;
			Ctx.RequiredSemanticSubject = ActivePlan->SemanticSubject;
			Ctx.RequiredRitualType = ActivePlan->RequiredRitualType;
			Ctx.RequiredRestorationTag = ActivePlan->RequiredRestorationTags.Num() == 1
				? ActivePlan->RequiredRestorationTags[0]
				: NAME_None;

			if (ActivePlan->NightType != PlannedNightType
				|| Ctx.RequiredWarningId == NAME_None
				|| Ctx.RequiredSemanticSubject == NAME_None
				|| Ctx.RequiredRitualType == ERitualType::Invalid
				|| Ctx.RequiredRestorationTag == NAME_None)
			{
				UE_LOG(LogTemp, Error, TEXT("NightRuntime: active authored plan is not a complete %s target contract; no substitute target will be selected."),
					*GetNightConsequenceTypeDisplayName(PlannedNightType));
				Ctx.TargetPointIndex = INDEX_NONE;
			}
			else
			{
				Ctx.TargetPointIndex = ResolvePlanTargetToPoint(
					*ActivePlan,
					PCG,
					PlannedNightType == ENightConsequenceType::Retrieval);
			}
		}
		else
		{
			Ctx.TargetPointIndex = PCG->FindMostCorruptedPointIndex(/*bOnlyUnrestored*/ true);
		}

		if (Ctx.TargetPointIndex >= 0)
		{
			Ctx.TargetStartCorruption = PCG->GetCorruptionLevel(Ctx.TargetPointIndex);
		}
	}

	// The player heeded an authored warning only if a concrete exact receipt
	// proves it. Generic tags, clarity tiers, and other same-type warnings do
	// not substitute for the active plan.
	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> Hearts;
		UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
		if (Hearts.Num() == 1)
		{
			if (const AVeilHeart* Heart = Cast<AVeilHeart>(Hearts[0]))
			{
				Ctx.bWarningHeeded = ActivePlan && Heart->HasExactInterpretationForPlan(*ActivePlan);
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

bool UNightConsequenceRuntime::Test_IsPressureCadenceScheduled() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetTimerManager().IsTimerActive(PressureTimer);
	}
	return false;
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
	bEarlyDawnRequested = false;
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
	bInitialPressureBeatInProgress = true;
	HandlePressureStep();
	bInitialPressureBeatInProgress = false;
	// A synchronous early-dawn callback may have already ended this run (or be
	// queued by DayNight until this call returns). Never arm pressure after either
	// state: duration/pressure are valid only for a still-active Night.
	if (World && PressureStepSeconds > 0.f && bNightActive && !bEarlyDawnRequested)
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

	if (bInitialPressureBeatInProgress && bTestForceEarlyDawnDuringInitialPressureBeat)
	{
		bTestForceEarlyDawnDuringInitialPressureBeat = false;
		bEarlyDawnRequested = true;
		if (World)
		{
			World->GetTimerManager().ClearTimer(PressureTimer);
		}
		UE_LOG(LogTemp, Log, TEXT("NightRuntime: test initial pressure beat requesting early dawn."));
		OnNightShouldEnd.Broadcast();
		return;
	}

	// A pressure step can resolve the objective on its own — the tutorial night's shelter check rides
	// this cadence — so the early-dawn condition is evaluated here as well as on restoration. Without
	// this the only mid-night resolution path was NotifyRestoration, and objectives the player completes
	// by moving rather than by restoring could never end the night early.
	const bool bWasResolved = ActiveStrategy->IsObjectiveResolved();
	ActiveStrategy->ApplyPressureStep(PCG);

	if (!bWasResolved && ActiveStrategy->IsObjectiveResolved())
	{
		bEarlyDawnRequested = true;
		if (World)
		{
			World->GetTimerManager().ClearTimer(PressureTimer);
		}
		UE_LOG(LogTemp, Log, TEXT("NightRuntime: objective resolved during pressure step — requesting early dawn."));
		OnNightShouldEnd.Broadcast();
	}
}

void UNightConsequenceRuntime::HandleRestorationDuringNight(const FRestorationEventPayload& Payload)
{
	if (!bNightActive || !ActiveStrategy)
	{
		return;
	}

	UWorld* World = GetWorld();
	UGloamsteadPCGSubsystem* PCG = World ? World->GetSubsystem<UGloamsteadPCGSubsystem>() : nullptr;
	FRestorationEventPayload AuthoritativePayload = Payload;
	if (ActiveContext.bRequiresExactSemanticTarget)
	{
		const FExperienceCyclePlan* ActivePlan = ResolveActiveAuthoredPlan();
		if (!ActivePlan
			|| !PCG
			|| !PCG->IsPointRestored(Payload.PointIndex)
			|| !PCG->PointMatchesExperiencePlan(Payload.PointIndex, *ActivePlan, /*bRequireRestored*/ true)
			|| !PCG->PopulateAuthoritativeRestorationMetadata(Payload.PointIndex, AuthoritativePayload))
		{
			UE_LOG(LogTemp, Verbose, TEXT("NightRuntime: ignored restoration whose PCG point does not satisfy the active authored target contract."));
			return;
		}
	}

	const bool bWasResolved = ActiveStrategy->IsObjectiveResolved();
	ActiveStrategy->NotifyRestoration(AuthoritativePayload, PCG);

	if (!bWasResolved && ActiveStrategy->IsObjectiveResolved())
	{
		bEarlyDawnRequested = true;
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
	bEarlyDawnRequested = false;
	bInitialPressureBeatInProgress = false;
	bTestForceEarlyDawnDuringInitialPressureBeat = false;
	ActiveNightType = ENightConsequenceType::Invalid;
	ActiveStrategy = nullptr;
}

void UNightConsequenceRuntime::AbortNightForRestore()
{
	// Restore replaces the PCG baseline underneath this runtime. Do not use
	// EndNight here: resolving the strategy, broadcasting OnNightEnded, or
	// retaining LastOutcome would let an abandoned world alter the new Day.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PressureTimer);
	}

	// DayNight removes its own cadence callback before it aborts this runtime.
	// This is a public BlueprintAssignable delegate, so restore must preserve
	// every listener it does not own; clearing runtime state below prevents any
	// abandoned run from broadcasting another early-dawn event.
	DestroyPressureActor();
	bNightActive = false;
	bEarlyDawnRequested = false;
	bInitialPressureBeatInProgress = false;
	bTestForceEarlyDawnDuringInitialPressureBeat = false;
	PlannedNightType = ENightConsequenceType::Invalid;
	ActiveNightType = ENightConsequenceType::Invalid;
	ActiveStrategy = nullptr;
	ActiveContext = FNightRuntimeContext();
	LastOutcome = FNightRuntimeOutcome();
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

	if (BoundIndex < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("NightRuntime: no valid pressure-actor target for the active consequence; refusing to spawn at a substitute location."));
		return;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	if (PCG)
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
