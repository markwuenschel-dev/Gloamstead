#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Save/GloamsteadSaveGame.h"
#include "Data/NightConsequenceTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float WarningPresentationRetrySeconds = 0.25f;
}

UGloamsteadExperienceCycleSubsystem* UGloamsteadDayNightSubsystem::GetExperienceCycleSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UGloamsteadExperienceCycleSubsystem>();
		}
	}
	return nullptr;
}

void UGloamsteadDayNightSubsystem::Test_BindCadenceRuntime(UNightConsequenceRuntime* InRuntime)
{
	BindCadenceRuntime(InRuntime);
}

void UGloamsteadDayNightSubsystem::BindCadenceRuntime(UNightConsequenceRuntime* InRuntime)
{
	if (CadenceRuntime.Get() == InRuntime)
	{
		return;
	}

	UnbindCadenceRuntime();
	if (!InRuntime)
	{
		return;
	}

	CadenceRuntime = InRuntime;
	CadenceRuntime->OnNightShouldEnd.AddDynamic(this, &UGloamsteadDayNightSubsystem::HandleNightShouldEnd);
}

void UGloamsteadDayNightSubsystem::UnbindCadenceRuntime()
{
	if (CadenceRuntime)
	{
		CadenceRuntime->OnNightShouldEnd.RemoveDynamic(this, &UGloamsteadDayNightSubsystem::HandleNightShouldEnd);
		CadenceRuntime = nullptr;
	}
}

void UGloamsteadDayNightSubsystem::QueueNightRuntimeStart(UNightConsequenceRuntime* Runtime)
{
	ClearQueuedNightRuntimeStart();
	if (!Runtime || CurrentPhase != EGloamsteadDayPhase::Night || !bDuskPlanPrepared)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// Night is already authoritative, but players and every phase presenter must
		// observe that fact before its first pressure beat can ask for Dawn. A timer
		// keeps the start out of both the Night handler and its phase delegate stack.
		PendingNightRuntime = Runtime;
		bNightRuntimeStartQueued = true;
		World->GetTimerManager().SetTimer(
			NightRuntimeStartTimer,
			this,
			&UGloamsteadDayNightSubsystem::StartNightRuntimeAfterPhasePresentation,
			KINDA_SMALL_NUMBER,
			false);
	}
}

void UGloamsteadDayNightSubsystem::StartNightRuntimeAfterPhasePresentation()
{
	bNightRuntimeStartQueued = false;
	UNightConsequenceRuntime* Runtime = PendingNightRuntime;
	PendingNightRuntime = nullptr;

	if (!Runtime || CurrentPhase != EGloamsteadDayPhase::Night || !bDuskPlanPrepared)
	{
		return;
	}

	// An objective may resolve in BeginNight's immediate pressure beat. Keep that
	// callback queued until BeginNight has returned so its EndNight path cannot
	// re-enter the runtime and leave a pressure timer behind.
	bNightRuntimeStartupInProgress = true;
	Runtime->BeginNight();
	bNightRuntimeStartupInProgress = false;

	if (CurrentPhase == EGloamsteadDayPhase::Night
		&& Runtime->IsNightActive()
		&& !Runtime->HasRequestedEarlyDawn())
	{
		ScheduleNightToDawnCadence();
	}

	DrainQueuedDawnTransition();
}

void UGloamsteadDayNightSubsystem::ClearQueuedNightRuntimeStart()
{
	bNightRuntimeStartQueued = false;
	PendingNightRuntime = nullptr;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NightRuntimeStartTimer);
	}
}

void UGloamsteadDayNightSubsystem::ClearDuskToNightCadence()
{
	bDuskToNightCadenceScheduled = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DuskToNightCadenceTimer);
	}
}

void UGloamsteadDayNightSubsystem::ClearNightToDawnCadence()
{
	bNightToDawnCadenceScheduled = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NightToDawnCadenceTimer);
	}
}

void UGloamsteadDayNightSubsystem::ClearCadenceTimers()
{
	ClearDuskToNightCadence();
	ClearNightToDawnCadence();
}

void UGloamsteadDayNightSubsystem::ScheduleDuskToNightCadence()
{
	ClearDuskToNightCadence();
	if (CurrentPhase != EGloamsteadDayPhase::Dusk || !bDuskPlanPrepared)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// A zero authoring value still advances on the next timer tick rather than
		// re-entering ApplyPhaseChange while its Dusk event is broadcasting.
		const float Delay = FMath::Max(DuskToNightDelaySeconds, KINDA_SMALL_NUMBER);
		World->GetTimerManager().SetTimer(
			DuskToNightCadenceTimer,
			this,
			&UGloamsteadDayNightSubsystem::AdvanceFromDuskCadence,
			Delay,
			false);
		bDuskToNightCadenceScheduled = true;
	}
}

void UGloamsteadDayNightSubsystem::ScheduleNightToDawnCadence()
{
	ClearNightToDawnCadence();
	if (CurrentPhase != EGloamsteadDayPhase::Night)
	{
		return;
	}
	// A live runtime owns the duration only once it has survived its initial
	// pressure beat. The no-runtime fallback deliberately remains cadence-driven.
	if (CadenceRuntime && (!CadenceRuntime->IsNightActive() || CadenceRuntime->HasRequestedEarlyDawn()))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const float Delay = FMath::Max(NightDurationSeconds, KINDA_SMALL_NUMBER);
		World->GetTimerManager().SetTimer(
			NightToDawnCadenceTimer,
			this,
			&UGloamsteadDayNightSubsystem::RequestDawnFromCadence,
			Delay,
			false);
		bNightToDawnCadenceScheduled = true;
	}
}

void UGloamsteadDayNightSubsystem::AdvanceFromDuskCadence()
{
	bDuskToNightCadenceScheduled = false;
	if (CurrentPhase == EGloamsteadDayPhase::Dusk && bDuskPlanPrepared)
	{
		AdvanceToNextPhase();
	}
}

void UGloamsteadDayNightSubsystem::HandleNightShouldEnd()
{
	RequestDawnFromCadence();
}

void UGloamsteadDayNightSubsystem::RequestDawnFromCadence()
{
	if (CurrentPhase != EGloamsteadDayPhase::Night || bDawnTransitionRequested)
	{
		return;
	}

	bDawnTransitionRequested = true;
	ClearNightToDawnCadence();
	if (PhaseTransitionDepth > 0 || bNightRuntimeStartupInProgress)
	{
		// Do not let a synchronous first-pressure completion make Dawn observable
		// before the complete Dusk -> Night presentation has returned. The same
		// queue also serializes an early objective raised by a phase listener.
		bQueuedDawnTransition = true;
		return;
	}

	CommitDawnFromCadence();
}

void UGloamsteadDayNightSubsystem::CommitDawnFromCadence()
{
	if (CurrentPhase != EGloamsteadDayPhase::Night || !bDawnTransitionRequested)
	{
		return;
	}

	++CadenceDawnRequestCount;
	AdvanceToNextPhase();
}

void UGloamsteadDayNightSubsystem::DrainQueuedDawnTransition()
{
	if (!bQueuedDawnTransition || PhaseTransitionDepth > 0 || bNightRuntimeStartupInProgress)
	{
		return;
	}

	bQueuedDawnTransition = false;
	CommitDawnFromCadence();
}

const FExperienceCyclePlan* UGloamsteadDayNightSubsystem::GetUpcomingPlan() const
{
	if (const UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem())
	{
		const FExperienceCyclePlan& Plan = Experience->GetActivePlan();
		return Plan.IsAuthoredPlan() ? &Plan : nullptr;
	}
	return nullptr;
}

void UGloamsteadDayNightSubsystem::NotifyWarningPresenterChanged()
{
	if (bRepresentingForNewPresenter)
	{
		return;
	}

	const FExperienceCyclePlan* Plan = GetUpcomingPlan();
	if (!Plan || !Plan->IsAuthoredPlan())
	{
		return;
	}

	TGuardValue<bool> Guard(bRepresentingForNewPresenter, true);

	// Clearing the presented id first is what makes this a RE-presentation rather than a no-op: the plan
	// was already shown, but to a presenter that is going away. PrepareUpcomingCycle restores the id when
	// the new presenter accepts it, so rest eligibility is unchanged for anyone who was already able to rest.
	const FName PreviouslyPresented = PresentedPlanId;
	PresentedPlanId = NAME_None;
	if (!PrepareUpcomingCycle())
	{
		// The new presenter could not take it. Leave the previous state alone rather than silently
		// downgrading a player who had already been warned.
		PresentedPlanId = PreviouslyPresented;
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("DayNight: re-presented the armed warning to a newly registered presenter."));
}

bool UGloamsteadDayNightSubsystem::PrepareUpcomingCycle()
{
	if (bInProgressSaveReconciliation)
	{
		// An old in-progress payload has no resumable runtime/base snapshot. It
		// keeps its PCG aftermath in Day, but this public preparation seam must
		// not turn the same authored plan into a second pressure application.
		return false;
	}

	UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem();
	if (!Experience || !Experience->EnsureUpcomingPlan())
	{
		bWarningPresentationPending = false;
		PendingPresentationPlanId = NAME_None;
		bWarningPresentationDeferralLogged = false;
		ClearWarningPresentationRetry();
		return false;
	}

	const FExperienceCyclePlan& Plan = Experience->GetActivePlan();
	if (!Plan.IsAuthoredPlan())
	{
		bWarningPresentationPending = false;
		PendingPresentationPlanId = NAME_None;
		bWarningPresentationDeferralLogged = false;
		ClearWarningPresentationRetry();
		return false;
	}
	if (Plan.Slot == 1 && !bFirstRestUnlocked)
	{
		// Cycle I's warning remains behind the FirstNightDirector's existing
		// lantern gate; only that route can open the first authored rest.
		bWarningPresentationPending = false;
		PendingPresentationPlanId = NAME_None;
		bWarningPresentationDeferralLogged = false;
		ClearWarningPresentationRetry();
		return false;
	}
	if (PresentedPlanId == Plan.PlanId)
	{
		bWarningPresentationPending = false;
		PendingPresentationPlanId = NAME_None;
		bWarningPresentationDeferralLogged = false;
		ClearWarningPresentationRetry();
		return true;
	}

	// Presentation readiness is explicitly separate from plan validity. The
	// Experience subsystem keeps this exact plan armed while the Heart comes up.
	if (PendingPresentationPlanId != Plan.PlanId)
	{
		bWarningPresentationDeferralLogged = false;
	}
	bWarningPresentationPending = true;
	PendingPresentationPlanId = Plan.PlanId;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<AActor*> Hearts;
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	if (Hearts.Num() != 1)
	{
		if (!bWarningPresentationDeferralLogged)
		{
			UE_LOG(LogTemp, Log, TEXT("DayNight: authored plan %s remains armed while canonical Heart ownership is ambiguous (%d Hearts)."),
				*Plan.PlanId.ToString(), Hearts.Num());
			bWarningPresentationDeferralLogged = true;
		}
		QueueWarningPresentationRetry();
		return false;
	}

	AVeilHeart* Heart = Cast<AVeilHeart>(Hearts[0]);
	if (Heart && bHasPendingHeartInterpretationState)
	{
		const bool bRestored = RestorePendingHeartInterpretation(Heart);
		if (!bRestored && bHasPendingHeartInterpretationState)
		{
			QueueWarningPresentationRetry();
			return false;
		}
		if (bRestored && PresentedPlanId == Plan.PlanId)
		{
			return true;
		}
	}
	if (!Heart || !Heart->CanPresentWarningForPlan(Plan))
	{
		if (!bWarningPresentationDeferralLogged)
		{
			UE_LOG(LogTemp, Log, TEXT("DayNight: authored plan %s remains armed while the canonical Heart lacks its exact warning or registered live presenter."),
				*Plan.PlanId.ToString());
			bWarningPresentationDeferralLogged = true;
		}
		QueueWarningPresentationRetry();
		return false;
	}

	if (Heart->EmitWarningForPlan(Plan))
	{
		PresentedPlanId = Plan.PlanId;
		bWarningPresentationPending = false;
		PendingPresentationPlanId = NAME_None;
		bWarningPresentationDeferralLogged = false;
		ClearWarningPresentationRetry();
		return true;
	}

	if (!bWarningPresentationDeferralLogged)
	{
		UE_LOG(LogTemp, Log, TEXT("DayNight: authored plan %s remains armed while warning %s awaits a ready VeilHeart."),
			*Plan.PlanId.ToString(), *Plan.WarningId.ToString());
		bWarningPresentationDeferralLogged = true;
	}
	QueueWarningPresentationRetry();
	return false;
}

void UGloamsteadDayNightSubsystem::ResetHeartInterpretationForProgressionRestore()
{
	PendingHeartInterpretationState.Reset();
	bHasPendingHeartInterpretationState = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> Hearts;
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	for (AActor* Actor : Hearts)
	{
		if (AVeilHeart* Heart = Cast<AVeilHeart>(Actor))
		{
			Heart->ResetInterpretationPersistentState();
		}
	}
}

bool UGloamsteadDayNightSubsystem::RestorePendingHeartInterpretation(AVeilHeart* Heart)
{
	if (!bHasPendingHeartInterpretationState)
	{
		return true;
	}

	UWorld* World = GetWorld();
	TArray<AActor*> Hearts;
	if (!World || !Heart)
	{
		return false;
	}
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	// A deferred actor can call BeginPlay just before it becomes visible to the
	// actor iterator. That is not ambiguous ownership: preserve the validated
	// snapshot and let the normal one-shot presentation retry observe the actor
	// once registration settles. Any actual second Heart still clears closed
	// below rather than granting the receipt to an arbitrary actor.
	if (Hearts.IsEmpty() && Heart->GetWorld() == World)
	{
		UE_LOG(LogTemp, Verbose, TEXT("DayNight: waiting for the late-spawned Heart to enter the canonical actor set before restoring interpretation."));
		return false;
	}
	if (Hearts.Num() != 1 || Hearts[0] != Heart)
	{
		UE_LOG(LogTemp, Error, TEXT("DayNight: cannot restore Heart interpretation because canonical Heart ownership is ambiguous (%d Hearts)."), Hearts.Num());
		PendingHeartInterpretationState.Reset();
		bHasPendingHeartInterpretationState = false;
		return false;
	}

	if (!Heart->IsInterpretationCatalogReady())
	{
		// A fresh map may spawn the one real Heart before its data asset has
		// finished becoming available. Keep the already-loaded v3 facts pending;
		// PrepareUpcomingCycle will retry through the normal presentation cadence.
		UE_LOG(LogTemp, Log, TEXT("DayNight: waiting for the canonical Heart warning catalog before restoring persisted interpretation."));
		return false;
	}

	const FExperienceCyclePlan* Plan = GetUpcomingPlan();
	const FVeilHeartInterpretationPersistentState State = PendingHeartInterpretationState;
	if (!Plan || !Heart->RestoreInterpretationPersistentState(State))
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: persisted Heart interpretation did not match the restored authored plan; clearing it safely."));
		Heart->ResetInterpretationPersistentState();
		PendingHeartInterpretationState.Reset();
		bHasPendingHeartInterpretationState = false;
		PresentedPlanId = NAME_None;
		return false;
	}

	if (Heart->GetLastEmittedWarningId() == Plan->WarningId)
	{
		PresentedPlanId = Plan->PlanId;
		bWarningPresentationPending = false;
		PendingPresentationPlanId = NAME_None;
		bWarningPresentationDeferralLogged = false;
		ClearWarningPresentationRetry();
	}

	PendingHeartInterpretationState.Reset();
	bHasPendingHeartInterpretationState = false;
	return true;
}

void UGloamsteadDayNightSubsystem::NotifyHeartReadyForProgressionRestore(AVeilHeart* Heart)
{
	if (!RestorePendingHeartInterpretation(Heart) && bHasPendingHeartInterpretationState)
	{
		QueueWarningPresentationRetry();
	}
}

bool UGloamsteadDayNightSubsystem::SaveProgressionToSlot()
{
	return SaveProgressionToSlot(UGloamsteadPCGSubsystem::DefaultSaveSlot);
}

bool UGloamsteadDayNightSubsystem::SaveProgressionToSlot(const FString& SlotName, int32 UserIndex) const
{
	// Dusk/Night runtime progress mutates PCG but is not resumable. Refuse the
	// write before capturing anything so an in-progress pressure snapshot can
	// never be replayed as a fresh authored consequence.
	if (CurrentPhase == EGloamsteadDayPhase::Dusk || CurrentPhase == EGloamsteadDayPhase::Night || bInProgressSaveReconciliation)
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: refusing progression save during in-progress/reconciliation phase %d."),
			static_cast<int32>(CurrentPhase));
		return false;
	}

	UWorld* World = GetWorld();
	UGloamsteadPCGSubsystem* PCG = World ? World->GetSubsystem<UGloamsteadPCGSubsystem>() : nullptr;
	UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem();
	if (!PCG || !Experience || SlotName.IsEmpty())
	{
		return false;
	}

	UGloamsteadSaveGame* SaveGame = Cast<UGloamsteadSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UGloamsteadSaveGame::StaticClass()));
	if (!SaveGame)
	{
		return false;
	}

	FExperienceCyclePersistentState CycleState = Experience->CapturePersistentState();
	TArray<AActor*> Hearts;
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	if (Hearts.Num() > 1)
	{
		UE_LOG(LogTemp, Error, TEXT("DayNight: refusing progression save because Heart interpretation ownership is ambiguous (%d Hearts)."), Hearts.Num());
		return false;
	}
	if (Hearts.Num() == 1)
	{
		AVeilHeart* Heart = Cast<AVeilHeart>(Hearts[0]);
		if (!Heart)
		{
			return false;
		}
		CycleState.HeartInterpretationState = Heart->CaptureInterpretationPersistentState();
	}
	else if (bHasPendingHeartInterpretationState)
	{
		// A fresh map can save before the delayed Heart actor is spawned. Preserve
		// the already-validated v3 snapshot rather than silently erasing it.
		CycleState.HeartInterpretationState = PendingHeartInterpretationState;
	}
	else
	{
		CycleState.HeartInterpretationState.Reset();
	}

	PCG->CaptureToSaveGame(SaveGame);
	CycleState.SavedPhaseOrdinal = static_cast<int32>(CurrentPhase);
	SaveGame->SetExperienceCycleState(CycleState);
	return UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex);
}

bool UGloamsteadDayNightSubsystem::LoadProgressionFromSlot()
{
	return LoadProgressionFromSlot(UGloamsteadPCGSubsystem::DefaultSaveSlot);
}

bool UGloamsteadDayNightSubsystem::LoadProgressionFromSlot(const FString& SlotName, int32 UserIndex)
{
	const EGloamsteadDayPhase PhaseBeforeRestore = CurrentPhase;
	UWorld* World = GetWorld();
	UGloamsteadPCGSubsystem* PCG = World ? World->GetSubsystem<UGloamsteadPCGSubsystem>() : nullptr;
	UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem();
	if (!PCG || !Experience || SlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return false;
	}

	UGloamsteadSaveGame* SaveGame = Cast<UGloamsteadSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (!SaveGame)
	{
		ResetToSafeDayReconciliation();
		return false;
	}

	// Quiesce before the first PCG write. In particular, a live runtime must
	// never resolve or pressure the newly restored Day on its old timer tick.
	QuiesceLiveWorldForProgressionRestore();
	// A rollback must not leave future warning/evidence/receipt facts resident
	// in a live Heart while PCG/cycle state is being replaced below.
	ResetHeartInterpretationForProgressionRestore();
	if (!PCG->RestoreFromSaveGame(SaveGame))
	{
		ResetToSafeDayReconciliation();
		return false;
	}

	const FExperienceCyclePersistentState& CycleState = SaveGame->GetExperienceCycleState();
	if (!Experience->RestorePersistentState(CycleState))
	{
		// A legacy payload has already restored its PCG state above, but it remains
		// deliberately ineligible for authored Cycle II progression.
		ResetToSafeDayReconciliation();
		return false;
	}

	if (CycleState.SavedPhaseOrdinal < static_cast<int32>(EGloamsteadDayPhase::Day)
		|| CycleState.SavedPhaseOrdinal > static_cast<int32>(EGloamsteadDayPhase::Dawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: save has no safe day phase; authored rest remains unavailable."));
		ResetToSafeDayReconciliation();
		return false;
	}

	const EGloamsteadDayPhase SavedPhase = static_cast<EGloamsteadDayPhase>(CycleState.SavedPhaseOrdinal);
	if (SavedPhase == EGloamsteadDayPhase::Dusk || SavedPhase == EGloamsteadDayPhase::Night)
	{
		// Runtime timers/objectives/outcome state are intentionally not persisted.
		// Preserve the PCG snapshot already restored above, but clear the active
		// consequence instead of replaying its pressure from a fabricated Day.
		FExperienceCyclePersistentState ReconciledState = CycleState;
		ReconciledState.ArmedPlanId = NAME_None;
		if (!Experience->RestorePersistentState(ReconciledState))
		{
			ResetToSafeDayReconciliation();
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("DayNight: normalizing persisted in-progress phase %d to Day without replaying its armed consequence."),
			static_cast<int32>(SavedPhase));
		CurrentPhase = EGloamsteadDayPhase::Day;
		NightCount = FMath::Max(0, ReconciledState.CompletedCycleSlot);
		// The lantern tutorial gate is live-session authority, never save authority.
		bFirstRestUnlocked = false;
		bInProgressSaveReconciliation = true;
		bDuskPlanPrepared = false;
		PresentedPlanId = NAME_None;
		bWarningPresentationPending = false;
		PendingPresentationPlanId = NAME_None;
		bWarningPresentationDeferralLogged = false;
		ClearWarningPresentationRetry();
		SynchronizePhasePresentationAfterProgressionRestore(PhaseBeforeRestore);
		return true;
	}

	bInProgressSaveReconciliation = false;
	CurrentPhase = SavedPhase;
	NightCount = FMath::Max(0, CycleState.CompletedCycleSlot);
	if (SavedPhase == EGloamsteadDayPhase::Dawn && NightCount > 0)
	{
		// The phase counter advances only when the player wakes into Day; a
		// completed dawn is durable, but that wrap has not happened yet.
		--NightCount;
	}
	// The first-rest gate is intentionally ephemeral: a durable armed plan or
	// prior completed-cycle fact cannot establish that this live tutorial called
	// UnlockFirstRest(). Later cycles remain restable through NightCount instead.
	bFirstRestUnlocked = false;
	bDuskPlanPrepared = false;
	PresentedPlanId = NAME_None;
	bWarningPresentationPending = false;
	PendingPresentationPlanId = NAME_None;
	bWarningPresentationDeferralLogged = false;
	ClearWarningPresentationRetry();
	DetachStaleFirstNightDirectorsForLaterCycleResume();

	// V3 persists interpretation only with the same validated cycle state. If
	// the Heart is not spawned yet, retain it for BeginPlay; if the world has
	// ambiguous Heart ownership, discard rather than grant a receipt to an
	// arbitrary actor.
	PendingHeartInterpretationState = CycleState.HeartInterpretationState;
	bHasPendingHeartInterpretationState = PendingHeartInterpretationState.HasAnyFacts();
	if (bHasPendingHeartInterpretationState)
	{
		TArray<AActor*> Hearts;
		UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
		if (Hearts.Num() == 1)
		{
			RestorePendingHeartInterpretation(Cast<AVeilHeart>(Hearts[0]));
		}
		else if (Hearts.Num() > 1)
		{
			UE_LOG(LogTemp, Error, TEXT("DayNight: clearing persisted Heart interpretation because load found %d Hearts."), Hearts.Num());
			PendingHeartInterpretationState.Reset();
			bHasPendingHeartInterpretationState = false;
		}
	}

	// A persisted armed plan is validated by RestorePersistentState. An unarmed
	// later Day may now arm exactly one next plan; the first tutorial day stays
	// silent until UnlockFirstRest() supplies its existing lantern gate.
	if (CurrentPhase == EGloamsteadDayPhase::Day
		&& (CycleState.bFirstRestCompleted || CycleState.ArmedPlanId != NAME_None))
	{
		// A valid v3 payload succeeds even if exact warning presentation must wait
		// for a Heart/catalog created later in the bootstrap order. CanRestNow()
		// continues to gate on PresentedPlanId until that exact presentation lands.
		PrepareUpcomingCycle();
	}

	SynchronizePhasePresentationAfterProgressionRestore(PhaseBeforeRestore);
	return true;
}

void UGloamsteadDayNightSubsystem::ResetToSafeDayReconciliation()
{
	const EGloamsteadDayPhase PhaseBeforeRestore = CurrentPhase;
	// Rejected progression payloads leave PCG restored for a human-visible
	// reconciliation, but never leave the phase machine or rest gate carrying
	// authority from the pre-load world.
	QuiesceLiveWorldForProgressionRestore();
	ResetHeartInterpretationForProgressionRestore();
	CurrentPhase = EGloamsteadDayPhase::Day;
	NightCount = 0;
	bFirstRestUnlocked = false;
	bInProgressSaveReconciliation = false;
	bDuskPlanPrepared = false;
	PresentedPlanId = NAME_None;
	bWarningPresentationPending = false;
	PendingPresentationPlanId = NAME_None;
	bWarningPresentationDeferralLogged = false;
	ClearWarningPresentationRetry();

	if (UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem())
	{
		FExperienceCyclePersistentState SafeState;
		SafeState.ResetForLegacyReconciliation();
		Experience->RestorePersistentState(SafeState);
	}

	SynchronizePhasePresentationAfterProgressionRestore(PhaseBeforeRestore);
}

void UGloamsteadDayNightSubsystem::SynchronizePhasePresentationAfterProgressionRestore(EGloamsteadDayPhase PreviousPhase)
{
	// Load/reconciliation assigns CurrentPhase directly to avoid replaying cadence,
	// runtime pressure, reflection, autosave, or authored-plan work. Presentation
	// subscribers still need one authoritative event after that semantic state is
	// complete. Do not replace this with ApplyPhaseChange: a same-phase restore
	// must also repair visual drift without replaying gameplay entry behavior.
	OnPhaseChanged.Broadcast(PreviousPhase, CurrentPhase);
	UE_LOG(LogTemp, Log, TEXT("DayNight: synchronized restore presentation %d -> %d (night count=%d)"),
		static_cast<int32>(PreviousPhase), static_cast<int32>(CurrentPhase), NightCount);
}

void UGloamsteadDayNightSubsystem::QueueWarningPresentationRetry()
{
	if (!bWarningPresentationPending || bWarningPresentationRetryQueued)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bWarningPresentationRetryQueued = true;
	World->GetTimerManager().SetTimer(
		WarningPresentationRetryTimer,
		this,
		&UGloamsteadDayNightSubsystem::RetryPendingWarningPresentation,
		WarningPresentationRetrySeconds,
		false);
}

void UGloamsteadDayNightSubsystem::RetryPendingWarningPresentation()
{
	bWarningPresentationRetryQueued = false;
	if (bWarningPresentationPending)
	{
		PrepareUpcomingCycle();
	}
}

void UGloamsteadDayNightSubsystem::ClearWarningPresentationRetry()
{
	bWarningPresentationRetryQueued = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningPresentationRetryTimer);
	}
}

void UGloamsteadDayNightSubsystem::QuiesceLiveWorldForProgressionRestore()
{
	// A save payload owns the PCG baseline it is about to restore. Nothing from
	// the abandoned Dusk/Night may observe or mutate that baseline while it is
	// being replaced: clear the phase authority first, then abort the runtime
	// without resolving its old strategy or broadcasting an old outcome.
	ClearCadenceTimers();
	ClearQueuedNightRuntimeStart();
	UnbindCadenceRuntime();
	ClearWarningPresentationRetry();
	bDawnTransitionRequested = false;
	bQueuedDawnTransition = false;
	bNightRuntimeStartupInProgress = false;
	bDuskPlanPrepared = false;

	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			Runtime->AbortNightForRestore();
		}
	}
}

void UGloamsteadDayNightSubsystem::DetachStaleFirstNightDirectorsForLaterCycleResume()
{
	// NightCount is reconstructed from the validated completed-cycle slot. A
	// nonzero value on Day proves this is no longer the opening lantern lesson;
	// detach every stale tutorial actor before any Cycle II warning can retry.
	if (CurrentPhase != EGloamsteadDayPhase::Day || NightCount <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> Directors;
	UGameplayStatics::GetAllActorsOfClass(World, AGloamsteadFirstNightDirector::StaticClass(), Directors);
	for (AActor* Actor : Directors)
	{
		if (AGloamsteadFirstNightDirector* Director = Cast<AGloamsteadFirstNightDirector>(Actor))
		{
			Director->DetachForProgressionResume();
		}
	}
}

void UGloamsteadDayNightSubsystem::Deinitialize()
{
	QuiesceLiveWorldForProgressionRestore();
	PendingHeartInterpretationState.Reset();
	bHasPendingHeartInterpretationState = false;
	bWarningPresentationPending = false;
	bInProgressSaveReconciliation = false;
	PendingPresentationPlanId = NAME_None;
	bWarningPresentationDeferralLogged = false;
	Super::Deinitialize();
}

float UGloamsteadDayNightSubsystem::GetNormalizedTimeOfDay() const
{
	switch (CurrentPhase)
	{
	case EGloamsteadDayPhase::Dawn:  return 0.f;
	case EGloamsteadDayPhase::Day:   return 0.5f;
	case EGloamsteadDayPhase::Dusk:  return 1.f;
	case EGloamsteadDayPhase::Night: return 0.85f;
	default:                         return 0.5f;
	}
}

void UGloamsteadDayNightSubsystem::AdvanceToNextPhase()
{
	EGloamsteadDayPhase Next = CurrentPhase;
	switch (CurrentPhase)
	{
	case EGloamsteadDayPhase::Day:
		if (!CanAdvanceFromDayToDusk())
		{
			UE_LOG(LogTemp, Log, TEXT("DayNight: direct Day->Dusk advance refused until the exact authored warning is player-facing."));
			return;
		}
		Next = EGloamsteadDayPhase::Dusk;
		break;
	case EGloamsteadDayPhase::Dusk:  Next = EGloamsteadDayPhase::Night; break;
	case EGloamsteadDayPhase::Night: Next = EGloamsteadDayPhase::Dawn; break;
	case EGloamsteadDayPhase::Dawn:
		++NightCount;
		Next = EGloamsteadDayPhase::Day;
		break;
	default:
		Next = EGloamsteadDayPhase::Day;
		break;
	}
	ApplyPhaseChange(Next);
}

bool UGloamsteadDayNightSubsystem::CanAdvanceFromDayToDusk()
{
	if (CanRestNow())
	{
		return true;
	}

	// This is intentionally one immediate reconciliation attempt, not a loop.
	// The bounded timer owns listener/catalog startup retries after this returns.
	if (GetExperienceCycleSubsystem())
	{
		PrepareUpcomingCycle();
	}
	return CanRestNow();
}

bool UGloamsteadDayNightSubsystem::CanRestNow() const
{
	// Dawn is always wake-able (including the FIRST dawn — nothing else advances Dawn->Day in-game).
	if (CurrentPhase == EGloamsteadDayPhase::Dawn)
	{
		return true;
	}
	// Day is rest-able once the scripted first night has completed (NightCount>0), or on night one as soon
	// as the FirstNightDirector reports its lantern gate satisfied (bFirstRestUnlocked). Rest still cannot
	// bypass the tutorial: before the lantern is restored neither condition holds.
	if (CurrentPhase == EGloamsteadDayPhase::Day)
	{
		if (!(NightCount > 0 || bFirstRestUnlocked))
		{
			return false;
		}

		// Production worlds have a game-instance owner for the authored plan. Keep
		// worldless legacy tests from inventing one, while preventing a live Heart
		// from offering rest without an exact armed plan.
		if (const UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem())
		{
			const FExperienceCyclePlan& Plan = Experience->GetActivePlan();
			return Plan.IsAuthoredPlan() && PresentedPlanId == Plan.PlanId;
		}
		return true;
	}
	return false;
}

void UGloamsteadDayNightSubsystem::UnlockFirstRest()
{
	if (bFirstRestUnlocked)
	{
		return;
	}
	bFirstRestUnlocked = true;
	if (GetExperienceCycleSubsystem() && !PrepareUpcomingCycle())
	{
		UE_LOG(LogTemp, Log, TEXT("DayNight: first rest unlocked; the authored Tutorial plan remains armed while its warning is deferred."));
	}
	UE_LOG(LogTemp, Log, TEXT("DayNight: first rest unlocked — the Heart will now accept the player's rest."));
}

bool UGloamsteadDayNightSubsystem::RequestRest()
{
	// Only the resting phases are player-advanceable; Dusk/Night resolve on their own.
	if (!CanRestNow())
	{
		UE_LOG(LogTemp, Log, TEXT("DayNight: rest requested but the night is already upon us (phase=%d)."),
			static_cast<int32>(CurrentPhase));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("DayNight: player rests at the Heart (phase=%d)."), static_cast<int32>(CurrentPhase));
	AdvanceToNextPhase();
	return true;
}

void UGloamsteadDayNightSubsystem::SetPhase(EGloamsteadDayPhase NewPhase)
{
	if (NewPhase == CurrentPhase)
	{
		return;
	}
	ApplyPhaseChange(NewPhase);
}

void UGloamsteadDayNightSubsystem::ApplyPhaseChange(EGloamsteadDayPhase NewPhase)
{
	++PhaseTransitionDepth;
	const EGloamsteadDayPhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;

	if (NewPhase == EGloamsteadDayPhase::Day)
	{
		HandleEnterDay();
	}
	else if (NewPhase == EGloamsteadDayPhase::Dusk)
	{
		HandleEnterDusk();
	}
	else if (NewPhase == EGloamsteadDayPhase::Night)
	{
		HandleEnterNight();
	}
	else if (NewPhase == EGloamsteadDayPhase::Dawn)
	{
		HandleEnterDawn();
	}

	OnPhaseChanged.Broadcast(OldPhase, NewPhase);
	UE_LOG(LogTemp, Log, TEXT("DayNight: phase %d -> %d (night count=%d)"),
		static_cast<int32>(OldPhase), static_cast<int32>(NewPhase), NightCount);

	--PhaseTransitionDepth;
	DrainQueuedDawnTransition();
}

void UGloamsteadDayNightSubsystem::HandleEnterDay()
{
	// A resumed Dawn becomes a later Day only through this authority. Ensure a
	// stale opening-tutorial presenter cannot win the exact Cycle II warning
	// race before the generic post-tutorial bridge takes the Heart role.
	DetachStaleFirstNightDirectorsForLaterCycleResume();
	if (GetExperienceCycleSubsystem() && !PrepareUpcomingCycle())
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: Day began without a safe authored plan."));
	}
}

void UGloamsteadDayNightSubsystem::HandleEnterDusk()
{
	ClearDuskToNightCadence();
	ClearNightToDawnCadence();
	ClearQueuedNightRuntimeStart();
	UnbindCadenceRuntime();
	bDawnTransitionRequested = false;
	bQueuedDawnTransition = false;
	bDuskPlanPrepared = false;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem();
	if (!Experience)
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: no ExperienceCycleSubsystem at dusk; refusing generic night selection."));
		return;
	}

	const FExperienceCyclePlan& Plan = Experience->GetActivePlan();
	const FExperienceCyclePersistentState State = Experience->CapturePersistentState();
	if (!Plan.IsAuthoredPlan() || State.ArmedPlanId != Plan.PlanId)
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: dusk has no matching armed authored plan; refusing night prep."));
		return;
	}

	if (UNightConsequenceManager* NightManager = World->GetSubsystem<UNightConsequenceManager>())
	{
		bDuskPlanPrepared = NightManager->PrepareNightConsequencesForPlan(Plan);
		if (bDuskPlanPrepared)
		{
			ScheduleDuskToNightCadence();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: NightConsequenceManager missing at dusk."));
	}
}

void UGloamsteadDayNightSubsystem::HandleEnterNight()
{
	ClearDuskToNightCadence();
	bDawnTransitionRequested = false;
	bQueuedDawnTransition = false;
	if (!bDuskPlanPrepared)
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: Night blocked because Dusk did not prepare an exact authored plan."));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			// Bind before the deferred BeginNight: its first pressure beat can
			// resolve an objective synchronously, but it must do so only after the
			// complete Night phase/presentation is observable.
			BindCadenceRuntime(Runtime);
			QueueNightRuntimeStart(Runtime);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("DayNight: NightConsequenceRuntime missing at night."));
			ScheduleNightToDawnCadence();
		}
	}

	// Keep a zero-duration authoring value asynchronous when a world exists;
	// the deferred runtime start and ScheduleNightToDawnCadence both use
	// KINDA_SMALL_NUMBER for that case. Worldless automation still exercises the
	// same early-objective handler directly.
}

void UGloamsteadDayNightSubsystem::HandleEnterDawn()
{
	ClearCadenceTimers();
	ClearQueuedNightRuntimeStart();
	bQueuedDawnTransition = false;
	if (UWorld* World = GetWorld())
	{
		// End the night first, then hand its real outcome to dawn reflection.
		FNightRuntimeOutcome NightOutcome;
		if (bDuskPlanPrepared)
		{
			if (UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>())
			{
				Runtime->EndNight();
				NightOutcome = Runtime->GetLastOutcome();
			}
		}

		TArray<AActor*> Hearts;
		UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
		if (Hearts.Num() > 0)
		{
			if (AVeilHeart* Heart = Cast<AVeilHeart>(Hearts[0]))
			{
				Heart->ProcessDawnReflectionWithOutcome(NightOutcome);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("DayNight: No AVeilHeart found for dawn reflection."));
		}

		if (bDuskPlanPrepared)
		{
			if (UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem())
			{
				if (!Experience->RecordActivePlanOutcome(NightOutcome))
				{
					UE_LOG(LogTemp, Warning, TEXT("DayNight: dawn outcome did not match the active authored plan; progression was not advanced."));
				}
				else
				{
					// Dawn is the payoff: surviving the night EARNS the Heart's warning about the next one.
					// Arming it here rather than waiting for Day means the player wakes already holding the
					// question the coming day is meant to answer - investigate, interpret, prepare - instead
					// of wandering until something happens to fire. It is armed before the autosave below,
					// so a reload resumes holding the same warning.
					if (PrepareUpcomingCycle())
					{
						UE_LOG(LogTemp, Log,
							TEXT("DayNight: dawn earned the next warning; the coming night is already named."));
					}
					else
					{
						// Not an error on its own - the authored experience may simply be complete.
						UE_LOG(LogTemp, Log,
							TEXT("DayNight: dawn armed no further warning (the authored experience may be complete)."));
					}
				}
			}
		}

		// Autosave PCG and authored day/cycle state together after the night resolves.
		// Demo maps may disable this without changing phase progression or dawn reflection.
		if (!bDawnAutosaveEnabled)
		{
			UE_LOG(LogTemp, Log, TEXT("DayNight: dawn autosave disabled for this world."));
		}
		else
		{
			const bool bSaved = SaveProgressionToSlot();
			UE_LOG(LogTemp, Log, TEXT("DayNight: dawn autosave (slot=%s) -> %s"),
				*UGloamsteadPCGSubsystem::DefaultSaveSlot, bSaved ? TEXT("ok") : TEXT("FAILED"));
		}

	}

	bDuskPlanPrepared = false;
	UnbindCadenceRuntime();
}
