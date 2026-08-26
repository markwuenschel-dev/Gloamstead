#include "Systems/NightStrategy.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

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
	// Carried here rather than in each strategy so a night can never report an outcome that has
	// silently forgotten which second reading shaped it.
	Out.SecondReadingGrade = Context.SecondReadingGrade;
	Out.SecondReadingTag = Context.SecondReadingTag;
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

bool UNightStrategy::NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* /*PCG*/)
{
	// Most nights are resolved by restoration or movement; a light ward only has meaning for a
	// strategy that explicitly owns a present threat.
	return false;
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
	if (TargetIndex < 0 && PCG && !InContext.bRequiresExactSemanticTarget)
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

void UNightCorruptionStrategy::NotifyRestoration_Implementation(const FRestorationEventPayload& Payload, UGloamsteadPCGSubsystem* PCG)
{
	if (Objective.bResolved || Objective.TargetPointIndex < 0 || !PCG)
	{
		return;
	}

	if (Context.bRequiresExactSemanticTarget
		&& (Payload.PointIndex != Objective.TargetPointIndex
			|| Payload.WarningId != Context.RequiredWarningId
			|| Payload.SemanticSubject != Context.RequiredSemanticSubject
			|| Payload.RitualType != Context.RequiredRitualType
			|| Payload.WarningTagSatisfied != Context.RequiredRestorationTag))
	{
		UE_LOG(LogTemp, Verbose, TEXT("NightStrategy[Corruption]: ignored restoration that did not exactly match the authored bloom subject."));
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
	bPlayerSheltered = false;
	bHasShelter = false;
	ShelterLocation = FVector::ZeroVector;

	Objective = FNightObjective();
	Objective.Kind = ENightObjectiveKind::TutorialTeach;
	Objective.TargetPointIndex = (InContext.TargetPointIndex >= 0 || !PCG)
		? InContext.TargetPointIndex
		: PCG->FindMostCorruptedPointIndex(false);
	Objective.bResolved = false; // resolved by reaching the restored lantern's light

	// The shelter is the lantern the player restored, located by the tag the restoration stamps on it.
	// Nearest-to-player is deliberate: with several restored lanterns the lesson points at the closest.
	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> Lanterns;
		UGameplayStatics::GetAllActorsWithTag(World, RestoredLanternTag, Lanterns);

		const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
		const FVector From = Player ? Player->GetActorLocation() : FVector::ZeroVector;

		float BestDistSq = TNumericLimits<float>::Max();
		for (const AActor* Lantern : Lanterns)
		{
			if (!IsValid(Lantern))
			{
				continue;
			}
			const float DistSq = static_cast<float>(FVector::DistSquared(From, Lantern->GetActorLocation()));
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				ShelterLocation = Lantern->GetActorLocation();
				bHasShelter = true;
			}
		}
	}

	if (bHasShelter)
	{
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Tutorial]: shelter is the restored lantern at %s (radius %.0f)."),
			*ShelterLocation.ToString(), ShelterRadius);
	}
	else
	{
		// Nothing was restored, so there is no light to reach. Do not strand the player against an
		// impossible objective: the beat degrades to the original always-winnable teaching night.
		Objective.bResolved = true;
		UE_LOG(LogTemp, Warning, TEXT("NightStrategy[Tutorial]: no restored lantern found (tag %s) — teaching beat only."),
			*RestoredLanternTag.ToString());
	}
}

bool UNightTutorialStrategy::EvaluateShelter()
{
	if (bPlayerSheltered || !bHasShelter)
	{
		return bPlayerSheltered;
	}

	const UWorld* World = GetWorld();
	const APawn* Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Player)
	{
		return false;
	}

	const float DistSq = static_cast<float>(FVector::DistSquared(Player->GetActorLocation(), ShelterLocation));
	if (DistSq > ShelterRadius * ShelterRadius)
	{
		return false;
	}

	bPlayerSheltered = true;
	Objective.bResolved = true;
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Tutorial]: player reached the lantern's light — lesson complete."));
	return true;
}

void UNightTutorialStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	// The shelter check rides the pressure cadence so the objective can resolve mid-night; the runtime
	// notices Objective.bResolved flipping and calls dawn early.
	EvaluateShelter();

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
	// One last look, so a player standing in the light as the clock runs out still gets the success.
	EvaluateShelter();

	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.bObjectiveResolved = Objective.bResolved;

	if (!bHasShelter)
	{
		// Degraded beat: nothing to reach, so the teaching night completes as it always did.
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("TutorialComplete"));
	}
	else if (bPlayerSheltered)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("TutorialSheltered"));
	}
	else
	{
		// Stayed out in the dark all night. Fail-forward: the lesson landed, the night was survived.
		Out.Result = ENightOutcomeResult::Partial;
		Out.ResultTag = FName(TEXT("TutorialExposed"));
	}

	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Tutorial]: outcome %s (tag %s, sanctuary delta %.2f)."),
		*GetNightOutcomeResultDisplayName(Out.Result), *Out.ResultTag.ToString(), Out.SanctuaryCorruptionDelta);
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
	bTargetReclaimed = false;

	int32 TargetIndex = -1;
	if (PCG)
	{
		if (InContext.bRequiresExactSemanticTarget)
		{
			// An authored Retrieval plan names the place being tested. A missing
			// or no-longer-restored target is an honest quiet fallback, never an
			// invitation to punish a different restored point.
			TargetIndex = (InContext.TargetPointIndex >= 0 && PCG->IsPointRestored(InContext.TargetPointIndex))
				? InContext.TargetPointIndex
				: -1;
		}
		else
		{
			TargetIndex = PCG->FindRestoredPointIndex(/*bMostLit*/ true);
		}
	}

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
	if (!bTargetReclaimed && NewLevel >= RetrievalReclaimThreshold)
	{
		// Make the consequence legible and actionable: once the seam tears open,
		// the normal placement authority can see the point as unrestored and the
		// player can deliberately re-light this exact place.
		bTargetReclaimed = PCG->RevertRestoration(Objective.TargetPointIndex);
		if (bTargetReclaimed)
		{
			UE_LOG(LogTemp, Log, TEXT("NightStrategy[Retrieval]: restoration reclaimed at point %d; re-stabilization is now possible."),
				Objective.TargetPointIndex);
		}
	}
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
		if (PCG && Objective.TargetPointIndex >= 0 && !bTargetReclaimed)
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

// ===== UNightPossessionStrategy (Night Types III) =====

void UNightPossessionStrategy::EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG)
{
	Context = InContext;
	StartAvgCorruption = SafeAvgCorruption(PCG);
	bPossessionActive = false;
	bPossessionDisrupted = false;
	bNoTargetFallback = false;

	int32 TargetIndex = -1;
	if (PCG)
	{
		if (InContext.bRequiresExactSemanticTarget)
		{
			// An authored possession can only occupy the exact place the Heart taught the player to
			// restore. If that place is gone, do not invent a different haunted target.
			TargetIndex = (InContext.TargetPointIndex >= 0 && PCG->IsPointRestored(InContext.TargetPointIndex))
				? InContext.TargetPointIndex
				: -1;
		}
		else
		{
			TargetIndex = PCG->FindRestoredPointIndex(/*bMostLit*/ true);
		}
	}

	Objective = FNightObjective();
	Objective.TargetPointIndex = TargetIndex;

	if (TargetIndex < 0 || !PCG)
	{
		bNoTargetFallback = true;
		Objective.Kind = ENightObjectiveKind::None;
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Possession]: no restored authored target — quiet fallback."));
		return;
	}

	Objective.Kind = ENightObjectiveKind::PurifyPossessed;
	Objective.StartCorruption = PCG->GetCorruptionLevel(TargetIndex);
	// Purification is deliberately a two-step read: the first ward can break the hold without
	// completing the objective, while the second must drive the place below a legible floor.
	Objective.ResolveAtOrBelow = FMath::Min(Objective.StartCorruption * 0.5f, 0.12f);
	Objective.bResolved = false;

	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Possession]: restored point %d is occupied (corruption %.2f, purify<=%.2f)."),
		TargetIndex, Objective.StartCorruption, Objective.ResolveAtOrBelow);
}

void UNightPossessionStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bNoTargetFallback || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return;
	}

	bPossessionActive = true;
	const float Delta = bPossessionDisrupted ? PossessionPressureDelta * 0.5f : PossessionPressureDelta;
	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, Delta);
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Possession]: silent hold presses on point %d (corruption %.2f, disrupted=%s)."),
		Objective.TargetPointIndex, NewLevel, bPossessionDisrupted ? TEXT("yes") : TEXT("no"));
}

bool UNightPossessionStrategy::NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bNoTargetFallback || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return false;
	}

	bPossessionActive = true;
	if (!bPossessionDisrupted)
	{
		bPossessionDisrupted = true;
		const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, -DisruptionCorruptionDelta);
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Possession]: first light ward disrupted the hold at point %d (corruption %.2f)."),
			Objective.TargetPointIndex, NewLevel);
		return true;
	}

	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, -PurificationCorruptionDelta);
	if (NewLevel <= Objective.ResolveAtOrBelow)
	{
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Possession]: second light ward purified point %d (corruption %.2f)."),
			Objective.TargetPointIndex, NewLevel);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Possession]: second ward weakened but did not purify point %d (corruption %.2f)."),
			Objective.TargetPointIndex, NewLevel);
	}
	return true;
}

FNightRuntimeOutcome UNightPossessionStrategy::ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.bObjectiveResolved = Objective.bResolved;

	if (bNoTargetFallback || Objective.Kind == ENightObjectiveKind::None)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("PossessionNoTarget"));
		return Out;
	}

	float FinalCorruption = (PCG && Objective.TargetPointIndex >= 0)
		? PCG->GetCorruptionLevel(Objective.TargetPointIndex)
		: Objective.StartCorruption;

	if (Objective.bResolved)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("PossessionPurified"));
	}
	else if (bPossessionDisrupted)
	{
		Out.Result = ENightOutcomeResult::Partial;
		Out.ResultTag = FName(TEXT("PossessionDisrupted"));
	}
	else
	{
		// Fail forward: the place remains restored, but the unopposed hold leaves a durable scar.
		if (PCG && Objective.TargetPointIndex >= 0 && FinalCorruption <= Objective.StartCorruption + KINDA_SMALL_NUMBER)
		{
			FinalCorruption = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, PossessionScarDelta);
		}
		Out.Result = ENightOutcomeResult::Failure;
		Out.ResultTag = FName(TEXT("PossessionScar"));
	}

	Out.TargetCorruptionDelta = FinalCorruption - Objective.StartCorruption;
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Possession]: outcome %s (target delta %.2f, sanctuary delta %.2f)."),
		*GetNightOutcomeResultDisplayName(Out.Result), Out.TargetCorruptionDelta, Out.SanctuaryCorruptionDelta);
	return Out;
}

// ===== UNightMirrorStrategy (Night Types III) =====

void UNightMirrorStrategy::EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG)
{
	Context = InContext;
	StartAvgCorruption = SafeAvgCorruption(PCG);
	bChoiceMade = false;
	bBargainAccepted = false;
	bBargainHeld = false;
	bNoTargetFallback = false;

	const int32 TargetIndex = (PCG && InContext.bRequiresExactSemanticTarget)
		? ((InContext.TargetPointIndex >= 0 && PCG->IsPointRestored(InContext.TargetPointIndex))
			? InContext.TargetPointIndex
			: INDEX_NONE)
		: (PCG ? PCG->FindRestoredPointIndex(/*bMostLit*/ true) : INDEX_NONE);

	Objective = FNightObjective();
	Objective.Kind = ENightObjectiveKind::MirrorBargain;
	Objective.TargetPointIndex = TargetIndex;

	if (TargetIndex < 0 || !PCG)
	{
		bNoTargetFallback = true;
		Objective.Kind = ENightObjectiveKind::None;
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Mirror]: no restored authored target — quiet fallback."));
		return;
	}

	Objective.StartCorruption = PCG->GetCorruptionLevel(TargetIndex);
	Objective.ResolveAtOrBelow = FMath::Min(Objective.StartCorruption * 0.5f, 0.12f);
	Objective.bResolved = false;
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Mirror]: reflection at point %d awaits a choice (corruption %.2f, hold<=%.2f)."),
		TargetIndex, Objective.StartCorruption, Objective.ResolveAtOrBelow);
}

void UNightMirrorStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bNoTargetFallback || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return;
	}

	const float Delta = bChoiceMade && bBargainAccepted ? BargainPressureDelta : UnansweredPressureDelta;
	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, Delta);
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Mirror]: reflection pressure at point %d now %.2f (choice=%s, accepted=%s)."),
		Objective.TargetPointIndex, NewLevel, bChoiceMade ? TEXT("made") : TEXT("pending"), bBargainAccepted ? TEXT("yes") : TEXT("no"));
}

bool UNightMirrorStrategy::ChooseBargain(bool bAccept)
{
	if (bNoTargetFallback || bChoiceMade || Objective.TargetPointIndex < 0)
	{
		return false;
	}

	bChoiceMade = true;
	bBargainAccepted = bAccept;
	if (!bAccept)
	{
		// Refusal is the clear reading: the reflection was a false path, not a gift.
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Mirror]: player refused the reflection bargain."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Mirror]: player accepted the reflection bargain; light must hold it."));
	}
	return true;
}

bool UNightMirrorStrategy::NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bNoTargetFallback || !bChoiceMade || !bBargainAccepted || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return false;
	}

	bBargainHeld = true;
	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, -BargainWardDelta);
	if (NewLevel <= Objective.ResolveAtOrBelow)
	{
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Mirror]: light held the accepted truth at point %d (corruption %.2f)."),
			Objective.TargetPointIndex, NewLevel);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Mirror]: light reached the mirror but did not hold it yet (corruption %.2f)."), NewLevel);
	}
	return true;
}

FNightRuntimeOutcome UNightMirrorStrategy::ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.bObjectiveResolved = Objective.bResolved;

	if (bNoTargetFallback || Objective.Kind == ENightObjectiveKind::None)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("MirrorNoTarget"));
		return Out;
	}

	float FinalCorruption = (PCG && Objective.TargetPointIndex >= 0)
		? PCG->GetCorruptionLevel(Objective.TargetPointIndex)
		: Objective.StartCorruption;

	if (!bChoiceMade)
	{
		if (PCG && Objective.TargetPointIndex >= 0 && FinalCorruption <= Objective.StartCorruption + KINDA_SMALL_NUMBER)
		{
			FinalCorruption = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, BargainScarDelta);
		}
		Out.Result = ENightOutcomeResult::Failure;
		Out.ResultTag = FName(TEXT("MirrorUnchosen"));
	}
	else if (!bBargainAccepted)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("MirrorRefused"));
	}
	else if (Objective.bResolved && bBargainHeld)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("MirrorTruthHeld"));
	}
	else
	{
		if (PCG && Objective.TargetPointIndex >= 0 && FinalCorruption <= Objective.StartCorruption + KINDA_SMALL_NUMBER)
		{
			FinalCorruption = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, BargainScarDelta);
		}
		Out.Result = ENightOutcomeResult::Partial;
		Out.ResultTag = FName(TEXT("MirrorBargainPaid"));
	}

	Out.TargetCorruptionDelta = FinalCorruption - Objective.StartCorruption;
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Mirror]: outcome %s (target delta %.2f, sanctuary delta %.2f)."),
		*GetNightOutcomeResultDisplayName(Out.Result), Out.TargetCorruptionDelta, Out.SanctuaryCorruptionDelta);
	return Out;
}


// ===== UNightBargainStrategy (Cycle V - the bell) =====

void UNightBargainStrategy::EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG)
{
	Context = InContext;
	StartAvgCorruption = SafeAvgCorruption(PCG);
	AnswersGiven = 0;
	bNoTargetFallback = false;

	// "Three answers invite company." An overread bell called something extra, and something extra
	// has to be answered too. This is the entire second clause, expressed as one integer.
	RequiredAnswers = InContext.HasOverreachReading() ? 2 : 1;

	const int32 TargetIndex = (PCG && InContext.bRequiresExactSemanticTarget)
		? ((InContext.TargetPointIndex >= 0 && PCG->IsPointRestored(InContext.TargetPointIndex))
			? InContext.TargetPointIndex
			: INDEX_NONE)
		: (PCG ? PCG->FindRestoredPointIndex(/*bMostLit*/ true) : INDEX_NONE);

	Objective = FNightObjective();
	Objective.Kind = ENightObjectiveKind::BreakBargain;
	Objective.TargetPointIndex = TargetIndex;

	if (TargetIndex < 0 || !PCG)
	{
		// The bell was never woken, so there is nothing to answer with. Failing closed to a quiet
		// night is right: punishing a player for not using a tool they do not have is arbitrary.
		bNoTargetFallback = true;
		Objective.Kind = ENightObjectiveKind::None;
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Bargain]: no restored bell shrine - quiet fallback."));
		return;
	}

	Objective.StartCorruption = PCG->GetCorruptionLevel(TargetIndex);
	Objective.ResolveAtOrBelow = FMath::Min(Objective.StartCorruption * 0.5f, 0.12f);
	Objective.bResolved = false;
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Bargain]: the bargain stands at point %d and needs %d answer(s)."),
		TargetIndex, RequiredAnswers);
}

void UNightBargainStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bNoTargetFallback || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return;
	}

	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, BargainPressureDelta);
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Bargain]: unanswered bargain deepens at point %d (%.2f), %d answer(s) still needed."),
		Objective.TargetPointIndex, NewLevel, GetAnswersRemaining());
}

bool UNightBargainStrategy::NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bNoTargetFallback || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return false;
	}

	++AnswersGiven;
	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, -ResonanceWardDelta);

	if (GetAnswersRemaining() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Bargain]: the bell answered, but company remains (%d left, corruption %.2f)."),
			GetAnswersRemaining(), NewLevel);
		return true;
	}

	Objective.bResolved = true;

	// The Insight payoff: a single clean answer on the right beat does not merely repel, it relights
	// what the night had already put out. That is what "One answer frees" earns over "answered it".
	if (Context.HasInsightReading())
	{
		const int32 Relit = PCG->ApplyCorruptionSpread(-ResonanceRelightDelta, /*MaxPoints*/ 3);
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Bargain]: one clean answer freed the night and relit %d point(s)."), Relit);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Bargain]: the bargain is broken at point %d (corruption %.2f)."),
			Objective.TargetPointIndex, NewLevel);
	}
	return true;
}

FNightRuntimeOutcome UNightBargainStrategy::ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.bObjectiveResolved = Objective.bResolved;

	if (bNoTargetFallback || Objective.Kind == ENightObjectiveKind::None)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("BargainNoTarget"));
		return Out;
	}

	float FinalCorruption = (PCG && Objective.TargetPointIndex >= 0)
		? PCG->GetCorruptionLevel(Objective.TargetPointIndex)
		: Objective.StartCorruption;

	if (Objective.bResolved)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("BargainBroken"));
	}
	else if (AnswersGiven > 0)
	{
		// Answered, but not enough times. That is precisely the overread's cost made legible.
		Out.Result = ENightOutcomeResult::Partial;
		Out.ResultTag = FName(TEXT("BargainCompanyRemains"));
	}
	else
	{
		if (PCG && Objective.TargetPointIndex >= 0)
		{
			FinalCorruption = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, UnansweredScarDelta);
		}
		Out.Result = ENightOutcomeResult::Failure;
		Out.ResultTag = FName(TEXT("BargainStruck"));
	}

	Out.TargetCorruptionDelta = FinalCorruption - Objective.StartCorruption;
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Bargain]: outcome %s (target delta %.2f, sanctuary delta %.2f)."),
		*GetNightOutcomeResultDisplayName(Out.Result), Out.TargetCorruptionDelta, Out.SanctuaryCorruptionDelta);
	return Out;
}

// ===== UNightSiegeStrategy (Cycle VI - fracture into true siege) =====

void UNightSiegeStrategy::EnterNight_Implementation(const FNightRuntimeContext& InContext, UGloamsteadPCGSubsystem* PCG)
{
	Context = InContext;
	StartAvgCorruption = SafeAvgCorruption(PCG);
	OpenSeams = 0;
	SeamsClosed = 0;
	bNoTargetFallback = false;

	// The two shapes the second clause names, and the only two things this night branches on.
	bRingHolds = InContext.HasInsightReading();
	bCrownCollapsed = InContext.HasOverreachReading();

	const int32 TargetIndex = (PCG && InContext.bRequiresExactSemanticTarget)
		? ((InContext.TargetPointIndex >= 0 && PCG->IsPointRestored(InContext.TargetPointIndex))
			? InContext.TargetPointIndex
			: INDEX_NONE)
		: (PCG ? PCG->FindRestoredPointIndex(/*bMostLit*/ true) : INDEX_NONE);

	Objective = FNightObjective();
	Objective.Kind = ENightObjectiveKind::HoldSiege;
	Objective.TargetPointIndex = TargetIndex;

	if (TargetIndex < 0 || !PCG)
	{
		bNoTargetFallback = true;
		Objective.Kind = ENightObjectiveKind::None;
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Siege]: no bound anchor - quiet fallback."));
		return;
	}

	Objective.StartCorruption = PCG->GetCorruptionLevel(TargetIndex);
	Objective.ResolveAtOrBelow = FMath::Min(Objective.StartCorruption * 0.5f, 0.10f);
	Objective.bResolved = false;
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Siege]: the seams open at point %d (ring=%s, crown=%s)."),
		TargetIndex, bRingHolds ? TEXT("held") : TEXT("open"), bCrownCollapsed ? TEXT("yes") : TEXT("no"));
}

void UNightSiegeStrategy::ApplyPressureStep_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bNoTargetFallback || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return;
	}

	// A closed ring links the outer lights, so the seams open at half the rate. A crown around the
	// Heart funnels everything to one place: the anchor takes double while the outer sanctuary,
	// which now has nothing holding it, is left to the spread.
	const float SeamScale = bRingHolds ? 0.5f : (bCrownCollapsed ? 2.0f : 1.0f);
	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, SeamPressureDelta * SeamScale);
	++OpenSeams;

	if (!bRingHolds)
	{
		const float SpreadScale = bCrownCollapsed ? 1.5f : 1.0f;
		const int32 Touched = PCG->ApplyCorruptionSpread(SanctuarySpreadDelta * SpreadScale, SanctuarySpreadPoints);
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Siege]: seam %d widens at point %d (%.2f); the fracture reached %d more point(s)."),
			OpenSeams, Objective.TargetPointIndex, NewLevel, Touched);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Siege]: seam %d strains at point %d (%.2f), but the ring is holding."),
			OpenSeams, Objective.TargetPointIndex, NewLevel);
	}
}

bool UNightSiegeStrategy::NotifyLightWard_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	if (!PCG || bNoTargetFallback || Objective.bResolved || Objective.TargetPointIndex < 0)
	{
		return false;
	}

	++SeamsClosed;
	// A held ring means the player's own linked light is doing half the work of every ward.
	const float WardScale = bRingHolds ? 1.5f : 1.0f;
	const float NewLevel = PCG->AddCorruptionAtIndex(Objective.TargetPointIndex, -SeamWardDelta * WardScale);

	if (NewLevel <= Objective.ResolveAtOrBelow)
	{
		Objective.bResolved = true;
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Siege]: the sanctuary held - seams closed at point %d (corruption %.2f)."),
			Objective.TargetPointIndex, NewLevel);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("NightStrategy[Siege]: seam %d pushed back (corruption %.2f)."), SeamsClosed, NewLevel);
	}
	return true;
}

FNightRuntimeOutcome UNightSiegeStrategy::ResolveNight_Implementation(UGloamsteadPCGSubsystem* PCG)
{
	FNightRuntimeOutcome Out = MakeBaseOutcome(PCG);
	Out.bObjectiveResolved = Objective.bResolved;

	if (bNoTargetFallback || Objective.Kind == ENightObjectiveKind::None)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = FName(TEXT("SiegeNoTarget"));
		return Out;
	}

	const float FinalCorruption = (PCG && Objective.TargetPointIndex >= 0)
		? PCG->GetCorruptionLevel(Objective.TargetPointIndex)
		: Objective.StartCorruption;

	if (Objective.bResolved)
	{
		Out.Result = ENightOutcomeResult::Success;
		Out.ResultTag = bRingHolds ? FName(TEXT("SiegeRingHeld")) : FName(TEXT("SiegeHeld"));
	}
	else if (SeamsClosed > 0 && FinalCorruption < Objective.StartCorruption)
	{
		Out.Result = ENightOutcomeResult::Partial;
		Out.ResultTag = FName(TEXT("SiegeSeamsStrained"));
	}
	else
	{
		// The failure is fail-forward, exactly like every other night: a scar the next dawn can
		// speak to, never a game over.
		Out.Result = ENightOutcomeResult::Failure;
		Out.ResultTag = bCrownCollapsed ? FName(TEXT("SiegeCrownBroke")) : FName(TEXT("SiegeFractured"));
	}

	Out.TargetCorruptionDelta = FinalCorruption - Objective.StartCorruption;
	UE_LOG(LogTemp, Log, TEXT("NightStrategy[Siege]: outcome %s (target delta %.2f, sanctuary delta %.2f, %d seam(s) closed)."),
		*GetNightOutcomeResultDisplayName(Out.Result), Out.TargetCorruptionDelta, Out.SanctuaryCorruptionDelta, SeamsClosed);
	return Out;
}
