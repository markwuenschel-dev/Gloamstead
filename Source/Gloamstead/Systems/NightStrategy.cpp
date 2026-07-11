#include "Systems/NightStrategy.h"
#include "PCG/GloamsteadPCGSubsystem.h"

// ===== UNightStrategy (base: benign quiet night) =====

float UNightStrategy::SafeAvgCorruption(UGloamsteadPCGSubsystem* PCG)
{
	return PCG ? PCG->GetSanctuaryAverageCorruptionLevel() : 0.f;
}

FNightRuntimeOutcome UNightStrategy::MakeBaseOutcome(UGloamsteadPCGSubsystem* PCG) const
{
	FNightRuntimeOutcome Out;
	Out.NightType = Context.NightType;
	Out.ObjectiveKind = Objective.Kind;
	Out.bObjectiveResolved = Objective.bResolved;
	Out.bWarningHeeded = Context.bWarningHeeded;
	Out.SanctuaryCorruptionDelta = SafeAvgCorruption(PCG) - StartAvgCorruption;
	return Out;
}

void UNightStrategy::EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG)
{
	Context = InContext;
	StartAvgCorruption = SafeAvgCorruption(PCG);

	Objective = FNightObjective();
	Objective.Kind = ENightObjectiveKind::None;
	Objective.bResolved = true; // a night with no objective is trivially "resolved"
}

void UNightStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* /*PCG*/)
{
	// Benign night: no pressure.
}

void UNightStrategy::NotifyRestoration_Implementation(const FRestorationEventPayload& /*Payload*/, UGloamsteadPCGSubsystem* /*PCG*/)
{
	// Benign night: restorations do not change the (already resolved) objective.
}

FNightRuntimeOutcome UNightStrategy::ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.Result = ENightOutcomeResult::Success; // nothing threatened the sanctuary
	Out.ResultTag = FName(TEXT("QuietNight"));
	return Out;
}

// ===== UNightCorruptionStrategy =====

void UNightCorruptionStrategy::EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG)
{
	Context = InContext;
	StartAvgCorruption = SafeAvgCorruption(PCG);

	int32 TargetIndex = InContext.TargetPointIndex;
	if (TargetIndex < 0 && PCG)
	{
		TargetIndex = PCG->FindMostCorruptedPointIndex(/*bOnlyUnrestored*/ true);
	}

	Objective = FNightObjective();
	Objective.Kind = ENightObjectiveKind::CleanseCorruptionBloom;
	Objective.TargetPointIndex = TargetIndex;

	if (TargetIndex < 0 || !PCG)
	{
		// No corrupted point to threaten the sanctuary: nothing to cleanse.
		Objective.Kind = ENightObjectiveKind::None;
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Corruption]: no bloom target — quiet corruption night."));
		return;
	}

	Objective.StartCorruption = PCG->GetCorruptionLevel(TargetIndex);
	// "Cleansed" = corruption at least halved and below a low floor.
	Objective.ResolveAtOrBelow = FMath::Min(Objective.StartCorruption * 0.5f, 0.2f);
	Objective.bResolved = false;

	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Corruption]: bloom at point %d (corruption %.2f, cleanse<=%.2f)."),
		TargetIndex, Objective.StartCorruption, Objective.ResolveAtOrBelow);
}

void UNightCorruptionStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return;
	}

	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, PressureStepDelta);
	const int32 Spread = PCG->ApplyCorruptionSpread(SpreadStepDelta, SpreadStepPoints);
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Corruption]: pressure step — bloom %d now %.2f (+%d spread)."),
		Objective.TargetPointIndex, NewLevel, Spread);
}

void UNightCorruptionStrategy::NotifyRestoration_Implementation(const FRestorationEventPayload& /*Payload*/, UGloamsteadPCGSubsystem* PCG)
{
	if (Objective.bResolved || Objective.TargetPointIndex < 0 || !PCG)
	{
		return;
	}

	// The bloom is cleansed only when the target's corruption has actually dropped to/below the
	// threshold — restoring the target index without clearing corruption is NOT a cleanse.
	const float TargetCorruption = PCG->GetCorruptionLevel(Objective.TargetPointIndex);
	if (TargetCorruption <= Objective.ResolveAtOrBelow)
	{
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Corruption]: bloom cleansed (point %d, corruption %.2f)."),
			Objective.TargetPointIndex, TargetCorruption);
	}
}

FNightRuntimeOutcome UNightCorruptionStrategy::ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.bObjectiveResolved = Objective.bResolved;

	if (Objective.Kind == ENightObjectiveKind::None)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("QuietNight"));
		return Out;
	}

	const float FinalCorruption = (PCG && Objective.TargetPointIndex >= 0)
		? PCG->GetCorruptionLevel(Objective.TargetPointIndex)
		: Objective.StartCorruption;
	Out.TargetCorruptionDelta = FinalCorruption - Objective.StartCorruption;

	if (Objective.bResolved)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("CorruptionCleansed"));
	}
	else if (Out.TargetCorruptionDelta < -KINDA_SMALL_NUMBER)
	{
		// Reduced but not below the cleanse threshold.
		Out.Result = ENightOutcomeResult::Partial;
		Out.ResultTag = FName(TEXT("CorruptionLingers"));
	}
	else
	{
		Out.Result = ENightOutcomeResult::Failure;
		Out.ResultTag = FName(TEXT("CorruptionScar"));
	}

	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Corruption]: outcome %s (bloom delta %.2f, sanctuary delta %.2f)."),
		*GetNightOutcomeResultDisplayName(Out.Result), Out.TargetCorruptionDelta, Out.SanctuaryCorruptionDelta);
	return Out;
}

// ===== UNightTutorialStrategy =====

void UNightTutorialStrategy::EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG)
{
	Context = InContext;
	StartAvgCorruption = SafeAvgCorruption(PCG);
	bTeachingSpreadApplied = false;

	Objective = FNightObjective();
	Objective.Kind = ENightObjectiveKind::TutorialTeach;
	Objective.TargetPointIndex = (InContext.TargetPointIndex >= 0 || !PCG)
		? InContext.TargetPointIndex
		: PCG->FindMostCorruptedPointIndex(false);
	Objective.bResolved = false; // resolves at dawn (always winnable)

	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Tutorial]: teaching beat — night reacts to the sanctuary."));
}

void UNightTutorialStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bTeachingSpreadApplied)
	{
		return; // bounded: teach once
	}
	bTeachingSpreadApplied = true;
	const int32 Spread = PCG->ApplyCorruptionSpread(TeachingSpreadDelta, TeachingSpreadPoints);
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Tutorial]: teaching spread over %d points."), Spread);
}

FNightRuntimeOutcome UNightTutorialStrategy::ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	Objective.bResolved = true; // teaching beat always completes
	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.bObjectiveResolved = true;
	Out.Result = ENightOutcomeResult::Success;
	Out.ResultTag = FName(TEXT("TutorialComplete"));

	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Tutorial]: outcome Success (sanctuary delta %.2f)."),
		Out.SanctuaryCorruptionDelta);
	return Out;
}
