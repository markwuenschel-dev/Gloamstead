#include "Systems/GloamsteadCycleFeedbackSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	FString PhaseName(EGloamsteadDayPhase Phase)
	{
		switch (Phase)
		{
		case EGloamsteadDayPhase::Day:   return TEXT("Day");
		case EGloamsteadDayPhase::Dusk:  return TEXT("Dusk");
		case EGloamsteadDayPhase::Night: return TEXT("Night");
		case EGloamsteadDayPhase::Dawn:  return TEXT("Dawn");
		default:                         return TEXT("?");
		}
	}
}

// ---- Pure formatters ----

FString UGloamsteadCycleFeedbackSubsystem::FormatPhase(EGloamsteadDayPhase NewPhase)
{
	switch (NewPhase)
	{
	case EGloamsteadDayPhase::Day:   return TEXT("A new day. Rest at the Heart when you are ready.");
	case EGloamsteadDayPhase::Dusk:  return TEXT("Dusk gathers. Heed the Heart's warning.");
	case EGloamsteadDayPhase::Night: return TEXT("Night falls. Something moves against the sanctuary.");
	case EGloamsteadDayPhase::Dawn:  return TEXT("Dawn breaks.");
	default:                         return FString::Printf(TEXT("Phase: %s"), *PhaseName(NewPhase));
	}
}

FString UGloamsteadCycleFeedbackSubsystem::FormatNightStart(ENightConsequenceType NightType)
{
	return FString::Printf(TEXT("The night stirs: %s. Act before dawn."),
		*GetNightConsequenceTypeDisplayName(NightType));
}

FString UGloamsteadCycleFeedbackSubsystem::FormatOutcome(const FNightRuntimeOutcome& Outcome)
{
	const FString Night = GetNightConsequenceTypeDisplayName(Outcome.NightType);
	const FString Tag = Outcome.ResultTag.IsNone() ? FString() : FString::Printf(TEXT(" [%s]"), *Outcome.ResultTag.ToString());
	switch (Outcome.Result)
	{
	case ENightOutcomeResult::Success:
		return FString::Printf(TEXT("Dawn: the sanctuary held. %s resolved.%s"), *Night, *Tag);
	case ENightOutcomeResult::Partial:
		return FString::Printf(TEXT("Dawn: the dark receded but lingers. %s partial.%s"), *Night, *Tag);
	case ENightOutcomeResult::Failure:
		return FString::Printf(TEXT("Dawn: a scar remains. %s failed.%s"), *Night, *Tag);
	case ENightOutcomeResult::None:
	default:
		return FString::Printf(TEXT("Dawn: the night passed quietly. %s%s"), *Night, *Tag);
	}
}

FColor UGloamsteadCycleFeedbackSubsystem::OutcomeColor(ENightOutcomeResult Result)
{
	switch (Result)
	{
	case ENightOutcomeResult::Success: return FColor::Green;
	case ENightOutcomeResult::Partial: return FColor::Yellow;
	case ENightOutcomeResult::Failure: return FColor::Red;
	case ENightOutcomeResult::None:
	default:                           return FColor::Silver;
	}
}

// ---- Lifecycle / binding ----

void UGloamsteadCycleFeedbackSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UGloamsteadDayNightSubsystem* DayNight = InWorld.GetSubsystem<UGloamsteadDayNightSubsystem>())
	{
		DayNight->OnPhaseChanged.AddDynamic(this, &UGloamsteadCycleFeedbackSubsystem::HandlePhaseChanged);
	}
	if (UNightConsequenceRuntime* Runtime = InWorld.GetSubsystem<UNightConsequenceRuntime>())
	{
		Runtime->OnNightStarted.AddDynamic(this, &UGloamsteadCycleFeedbackSubsystem::HandleNightStarted);
		Runtime->OnNightEnded.AddDynamic(this, &UGloamsteadCycleFeedbackSubsystem::HandleNightEnded);
		Runtime->OnOmenClueReady.AddDynamic(this, &UGloamsteadCycleFeedbackSubsystem::HandleOmenClueReady);
	}
	bBound = true;
}

void UGloamsteadCycleFeedbackSubsystem::Deinitialize()
{
	if (bBound)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>())
			{
				DayNight->OnPhaseChanged.RemoveDynamic(this, &UGloamsteadCycleFeedbackSubsystem::HandlePhaseChanged);
			}
			if (UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>())
			{
				Runtime->OnNightStarted.RemoveDynamic(this, &UGloamsteadCycleFeedbackSubsystem::HandleNightStarted);
				Runtime->OnNightEnded.RemoveDynamic(this, &UGloamsteadCycleFeedbackSubsystem::HandleNightEnded);
				Runtime->OnOmenClueReady.RemoveDynamic(this, &UGloamsteadCycleFeedbackSubsystem::HandleOmenClueReady);
			}
		}
		bBound = false;
	}
	Super::Deinitialize();
}

// ---- Delegate handlers ----

void UGloamsteadCycleFeedbackSubsystem::HandlePhaseChanged(EGloamsteadDayPhase /*OldPhase*/, EGloamsteadDayPhase NewPhase)
{
	Show(/*Key*/ 1001, /*Duration*/ 5.f, FColor::Cyan, FormatPhase(NewPhase));
}

void UGloamsteadCycleFeedbackSubsystem::HandleNightStarted(ENightConsequenceType NightType)
{
	Show(1002, 6.f, FColor::Orange, FormatNightStart(NightType));
}

void UGloamsteadCycleFeedbackSubsystem::HandleNightEnded(ENightConsequenceType /*NightType*/)
{
	FNightRuntimeOutcome Outcome;
	if (const UWorld* World = GetWorld())
	{
		if (const UNightConsequenceRuntime* Runtime = World->GetSubsystem<UNightConsequenceRuntime>())
		{
			Outcome = Runtime->GetLastOutcome();
		}
	}
	Show(1003, 8.f, OutcomeColor(Outcome.Result), FormatOutcome(Outcome));
}

void UGloamsteadCycleFeedbackSubsystem::HandleOmenClueReady(FName ClueTag)
{
	Show(1004, 6.f, FColor::Purple, FString::Printf(TEXT("An omen marks the sanctuary: %s"), *ClueTag.ToString()));
}

void UGloamsteadCycleFeedbackSubsystem::Show(int32 Key, float Duration, const FColor& Color, const FString& Text) const
{
	const UWorld* World = GetWorld();
	if (!GEngine || !World || !World->IsGameWorld())
	{
		return; // debug HUD only in a live game world; automation/editor-preview stay clean
	}
	GEngine->AddOnScreenDebugMessage(Key, Duration, Color, Text);
	UE_LOG(LogTemp, Log, TEXT("CycleFeedback: %s"), *Text);
}
