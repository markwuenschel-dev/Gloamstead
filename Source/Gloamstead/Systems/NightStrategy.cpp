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

// ===== UNightOmenStrategy (Night Types II) =====

void UNightOmenStrategy::EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG)
{
	Context = InContext;
	StartAvgCorruption = SafeAvgCorruption(PCG);
	bSawRestoration = false;

	int32 TargetIndex = InContext.TargetPointIndex;
	if (TargetIndex < 0 && PCG)
	{
		TargetIndex = PCG->FindMostCorruptedPointIndex(/*bOnlyUnrestored*/ true);
	}

	Objective = FNightObjective();
	Objective.Kind = ENightObjectiveKind::HeedOmen;
	Objective.TargetPointIndex = TargetIndex;

	if (TargetIndex < 0 || !PCG)
	{
		// No vulnerable point for the omen to mark: nothing to interpret — a quiet night.
		Objective.Kind = ENightObjectiveKind::None;
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Omen]: no vulnerable point to mark — quiet omen night."));
		return;
	}

	Objective.StartCorruption = PCG->GetCorruptionLevel(TargetIndex);
	// "Understood" = the marked point restored and its corruption driven down like a cleanse.
	Objective.ResolveAtOrBelow = FMath::Min(Objective.StartCorruption * 0.5f, 0.2f);
	Objective.bResolved = false;

	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Omen]: omen marks point %d (corruption %.2f, heed<=%.2f)."),
		TargetIndex, Objective.StartCorruption, Objective.ResolveAtOrBelow);
}

void UNightOmenStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return;
	}
	// The omen deepens while unheeded — the sign accretes corruption on its marked point.
	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, OmenDeepenDelta);
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Omen]: omen deepens — point %d now %.2f."),
		Objective.TargetPointIndex, NewLevel);
}

void UNightOmenStrategy::NotifyRestoration_Implementation(const FRestorationEventPayload& Payload, UGloamsteadPCGSubsystem* PCG)
{
	bSawRestoration = true;
	if (Objective.bResolved || Objective.TargetPointIndex < 0 || !PCG)
	{
		return;
	}
	// The omen is heeded only when the player acts on the MARKED point and drives its corruption down.
	if (Payload.PointIndex == Objective.TargetPointIndex)
	{
		const float TargetCorruption = PCG->GetCorruptionLevel(Objective.TargetPointIndex);
		if (TargetCorruption <= Objective.ResolveAtOrBelow)
		{
			Objective.bResolved = true;
			UE_LOG(LogTemp, Log, TEXT("NightStrategy[Omen]: omen heeded (point %d, corruption %.2f)."),
				Objective.TargetPointIndex, TargetCorruption);
		}
	}
}

FNightRuntimeOutcome UNightOmenStrategy::ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.bObjectiveResolved = Objective.bResolved;

	if (Objective.Kind == ENightObjectiveKind::None)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("QuietNight"));
		return Out;
	}

	float FinalCorruption = (PCG && Objective.TargetPointIndex >= 0)
		? PCG->GetCorruptionLevel(Objective.TargetPointIndex)
		: Objective.StartCorruption;

	if (Objective.bResolved)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("OmenHeeded"));
	}
	else if (bSawRestoration)
	{
		// The player read the region but not the sign — the omen remains unresolved but useful.
		Out.Result = ENightOutcomeResult::Partial;
		Out.ResultTag = FName(TEXT("OmenClouded"));
	}
	else
	{
		// Ignored: the omen festers into a corruption seed on the marked point (guaranteed mark).
		Out.Result = ENightOutcomeResult::Failure;
		Out.ResultTag = FName(TEXT("OmenIgnored"));
		if (PCG && Objective.TargetPointIndex >= 0 && FinalCorruption <= Objective.StartCorruption + KINDA_SMALL_NUMBER)
		{
			FinalCorruption = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, OmenDeepenDelta);
		}
	}

	Out.TargetCorruptionDelta = FinalCorruption - Objective.StartCorruption;
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Omen]: outcome %s (omen delta %.2f, sanctuary delta %.2f)."),
		*GetNightOutcomeResultDisplayName(Out.Result), Out.TargetCorruptionDelta, Out.SanctuaryCorruptionDelta);
	return Out;
}

// ===== UNightRetrievalStrategy (Night Types II) =====

void UNightRetrievalStrategy::EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG)
{
	Context = InContext;
	StartAvgCorruption = SafeAvgCorruption(PCG);
	bSawTargetIntervention = false;
	bNoTargetFallback = false;

	const int32 TargetIndex = PCG ? PCG->FindRestoredPointIndex(/*bMostLit*/ true) : -1;

	Objective = FNightObjective();
	Objective.TargetPointIndex = TargetIndex;

	if (TargetIndex < 0 || !PCG)
	{
		// Nothing was restored — the night has nothing to reclaim. Honest quiet fallback.
		bNoTargetFallback = true;
		Objective.Kind = ENightObjectiveKind::None;
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Retrieval]: no restored point to reclaim — quiet fallback."));
		return;
	}

	Objective.Kind = ENightObjectiveKind::HoldRestored;
	Objective.StartCorruption = PCG->GetCorruptionLevel(TargetIndex);
	// "Re-stabilized" = corruption driven back below its starting level (undoing the night's gnawing).
	Objective.ResolveAtOrBelow = FMath::Min(Objective.StartCorruption * 0.5f, 0.15f);
	Objective.bResolved = false;

	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Retrieval]: night reclaims restored point %d (corruption %.2f, hold<=%.2f)."),
		TargetIndex, Objective.StartCorruption, Objective.ResolveAtOrBelow);
}

void UNightRetrievalStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bNoTargetFallback || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return;
	}
	// The night gnaws at the restored point, corrupting what the player had mended.
	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, RetrievalPressureDelta);
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Retrieval]: reclaim pressure — point %d now %.2f."),
		Objective.TargetPointIndex, NewLevel);
}

void UNightRetrievalStrategy::NotifyRestoration_Implementation(const FRestorationEventPayload& Payload, UGloamsteadPCGSubsystem* PCG)
{
	if (bNoTargetFallback || Objective.bResolved || Objective.TargetPointIndex < 0 || !PCG)
	{
		return;
	}
	// The player defends the point they mended by re-stabilizing THIS target.
	if (Payload.PointIndex == Objective.TargetPointIndex)
	{
		bSawTargetIntervention = true;
		const float TargetCorruption = PCG->GetCorruptionLevel(Objective.TargetPointIndex);
		if (TargetCorruption <= Objective.ResolveAtOrBelow)
		{
			Objective.bResolved = true;
			UE_LOG(LogTemp, Log, TEXT("NightStrategy[Retrieval]: target re-stabilized (point %d, corruption %.2f)."),
				Objective.TargetPointIndex, TargetCorruption);
		}
	}
}

FNightRuntimeOutcome UNightRetrievalStrategy::ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.bObjectiveResolved = Objective.bResolved;

	if (bNoTargetFallback || Objective.Kind == ENightObjectiveKind::None)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("RetrievalNoTarget"));
		return Out;
	}

	const float FinalCorruption = (PCG && Objective.TargetPointIndex >= 0)
		? PCG->GetCorruptionLevel(Objective.TargetPointIndex)
		: Objective.StartCorruption;
	Out.TargetCorruptionDelta = FinalCorruption - Objective.StartCorruption;

	if (Objective.bResolved)
	{
		// Defended: the player drove the reclaim back and kept what they had mended.
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("RetrievalRepelled"));
	}
	else if (bSawTargetIntervention)
	{
		// The night found a seam: the point survives, still restored, but scarred.
		Out.Result = ENightOutcomeResult::Partial;
		Out.ResultTag = FName(TEXT("RetrievalSeam"));
	}
	else
	{
		// Reclaimed: the night takes the point back (fail-forward, no hard game-over).
		if (PCG && Objective.TargetPointIndex >= 0)
		{
			PCG->RevertRestoration(Objective.TargetPointIndex);
		}
		Out.Result = ENightOutcomeResult::Failure;
		Out.ResultTag = FName(TEXT("RetrievalReclaimed"));
	}

	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Retrieval]: outcome %s (target delta %.2f, sanctuary delta %.2f)."),
		*GetNightOutcomeResultDisplayName(Out.Result), Out.TargetCorruptionDelta, Out.SanctuaryCorruptionDelta);
	return Out;
}
