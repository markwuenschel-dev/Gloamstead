#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/NightConsequenceTypes.h"
#include "Kismet/GameplayStatics.h"

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
		return NightCount > 0 || bFirstRestUnlocked;
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

	if (NewPhase == EGloamsteadDayPhase::Dusk)
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

void UGloamsteadDayNightSubsystem::HandleEnterDusk()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ENightConsequenceType SelectedNight = ENightConsequenceType::Invalid;
	if (UNightConsequenceManager* NightManager = World->GetSubsystem<UNightConsequenceManager>())
	{
		NightManager->PrepareNightConsequences();
		SelectedNight = NightManager->GetLastSelectedNightType();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DayNight: NightConsequenceManager missing at dusk."));
	}

	if (SelectedNight != ENightConsequenceType::Invalid)
	{
		TArray<AActor*> Hearts;
		UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
		for (AActor* Actor : Hearts)
		{
			if (AVeilHeart* Heart = Cast<AVeilHeart>(Actor))
			{
				Heart->EmitWarningForNight(SelectedNight);
				break;
			}
		}
	}
}

void UGloamsteadDayNightSubsystem::HandleEnterNight()
{
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
		if (UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			Runtime->EndNight();
			NightOutcome = Runtime->GetLastOutcome();
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

		// Autosave the sanctuary's full per-point state at dawn, once the night has been resolved.
		// Demo maps may disable this without changing phase progression or dawn reflection.
		if (!bDawnAutosaveEnabled)
		{
			UE_LOG(LogTemp, Log, TEXT("DayNight: dawn autosave disabled for this world."));
		}
		else if (UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>())
		{
			const bool bSaved = PCG->SaveToSlot(UGloamsteadPCGSubsystem::DefaultSaveSlot);
			UE_LOG(LogTemp, Log, TEXT("DayNight: dawn autosave (slot=%s) -> %s"),
				*UGloamsteadPCGSubsystem::DefaultSaveSlot, bSaved ? TEXT("ok") : TEXT("FAILED"));
		}
	}
}
