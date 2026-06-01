#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/VeilHeart.h"
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
		if (UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			Runtime->EndNight();
		}

		TArray<AActor*> Hearts;
		UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
		if (Hearts.Num() > 0)
		{
			if (AVeilHeart* Heart = Cast<AVeilHeart>(Hearts[0]))
			{
				Heart->ProcessDawnReflection();
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("DayNight: No AVeilHeart found for dawn reflection."));
		}
	}
}