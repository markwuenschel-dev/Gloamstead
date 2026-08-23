#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Save/GloamsteadSaveGame.h"
#include "Data/NightConsequenceTypes.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

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

const FExperienceCyclePlan* UGloamsteadDayNightSubsystem::GetUpcomingPlan() const
{
	if (const UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem())
	{
		const FExperienceCyclePlan& Plan = Experience->GetActivePlan();
		return Plan.IsAuthoredPlan() ? &Plan : nullptr;
	}
	return nullptr;
}

bool UGloamsteadDayNightSubsystem::PrepareUpcomingCycle()
{
	UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem();
	if (!Experience || !Experience->EnsureUpcomingPlan())
	{
		return false;
	}

	const FExperienceCyclePlan& Plan = Experience->GetActivePlan();
	if (!Plan.IsAuthoredPlan())
	{
		return false;
	}
	if (Plan.Slot == 1 && !bFirstRestUnlocked)
	{
		// Cycle I's warning remains behind the FirstNightDirector's existing
		// lantern gate; only that route can open the first authored rest.
		return false;
	}
	if (PresentedPlanId == Plan.PlanId)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<AActor*> Hearts;
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	for (AActor* Actor : Hearts)
	{
		if (AVeilHeart* Heart = Cast<AVeilHeart>(Actor))
		{
			const bool bEmitted = Heart->EmitWarningById(Plan.WarningId, Plan.NightType);
			if (bEmitted)
			{
				PresentedPlanId = Plan.PlanId;
			}
			return bEmitted;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("DayNight: authored plan %s armed but no VeilHeart was available to present warning %s."),
		*Plan.PlanId.ToString(), *Plan.WarningId.ToString());
	return false;
}

bool UGloamsteadDayNightSubsystem::SaveProgressionToSlot()
{
	return SaveProgressionToSlot(UGloamsteadPCGSubsystem::DefaultSaveSlot);
}

bool UGloamsteadDayNightSubsystem::SaveProgressionToSlot(const FString& SlotName, int32 UserIndex) const
{
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

	PCG->CaptureToSaveGame(SaveGame);
	FExperienceCyclePersistentState CycleState = Experience->CapturePersistentState();
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
	UWorld* World = GetWorld();
	UGloamsteadPCGSubsystem* PCG = World ? World->GetSubsystem<UGloamsteadPCGSubsystem>() : nullptr;
	UGloamsteadExperienceCycleSubsystem* Experience = GetExperienceCycleSubsystem();
	if (!PCG || !Experience || SlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return false;
	}

	UGloamsteadSaveGame* SaveGame = Cast<UGloamsteadSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (!SaveGame || !PCG->RestoreFromSaveGame(SaveGame))
	{
		return false;
	}

	const FExperienceCyclePersistentState& CycleState = SaveGame->GetExperienceCycleState();
	if (!Experience->RestorePersistentState(CycleState))
	{
		// A legacy payload has already restored its PCG state above, but it remains
		// deliberately ineligible for authored Cycle II progression.
		NightCount = 0;
		bFirstRestUnlocked = false;
		bDuskPlanPrepared = false;
		PresentedPlanId = NAME_None;
		return false;
	}

	if (CycleState.SavedPhaseOrdinal < static_cast<int32>(EGloamsteadDayPhase::Day)
		|| CycleState.SavedPhaseOrdinal > static_cast<int32>(EGloamsteadDayPhase::Dawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: save has no safe day phase; authored rest remains unavailable."));
		bFirstRestUnlocked = false;
		bDuskPlanPrepared = false;
		PresentedPlanId = NAME_None;
		return false;
	}

	CurrentPhase = static_cast<EGloamsteadDayPhase>(CycleState.SavedPhaseOrdinal);
	NightCount = FMath::Max(0, CycleState.CompletedCycleSlot);
	if (CurrentPhase == EGloamsteadDayPhase::Dawn && NightCount > 0)
	{
		// The phase counter advances only when the player wakes into Day; a
		// completed dawn is durable, but that wrap has not happened yet.
		--NightCount;
	}
	bFirstRestUnlocked = CycleState.bFirstRestCompleted
		|| (CurrentPhase == EGloamsteadDayPhase::Day && CycleState.ArmedPlanId == FName(TEXT("Cycle1_Tutorial")));
	bDuskPlanPrepared = false;
	PresentedPlanId = NAME_None;

	// A persisted armed plan is validated by RestorePersistentState. An unarmed
	// later Day may now arm exactly one next plan; the first tutorial day stays
	// silent until UnlockFirstRest() supplies its existing lantern gate.
	if (CurrentPhase == EGloamsteadDayPhase::Day
		&& (CycleState.bFirstRestCompleted || CycleState.ArmedPlanId != NAME_None)
		&& !PrepareUpcomingCycle())
	{
		return false;
	}

	return true;
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
	case EGloamsteadDayPhase::Day:   Next = EGloamsteadDayPhase::Dusk; break;
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
	SetPhase(Next);
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
		UE_LOG(LogTemp, Warning, TEXT("DayNight: first rest unlocked but Tutorial plan/warning could not be armed."));
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
}

void UGloamsteadDayNightSubsystem::HandleEnterDay()
{
	if (GetExperienceCycleSubsystem() && !PrepareUpcomingCycle())
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: Day began without a safe authored plan."));
	}
}

void UGloamsteadDayNightSubsystem::HandleEnterDusk()
{
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
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: NightConsequenceManager missing at dusk."));
	}
}

void UGloamsteadDayNightSubsystem::HandleEnterNight()
{
	if (!bDuskPlanPrepared)
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: Night blocked because Dusk did not prepare an exact authored plan."));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			Runtime->BeginNight();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("DayNight: NightConsequenceRuntime missing at night."));
		}
	}
}

void UGloamsteadDayNightSubsystem::HandleEnterDawn()
{
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

		bDuskPlanPrepared = false;
	}
}
