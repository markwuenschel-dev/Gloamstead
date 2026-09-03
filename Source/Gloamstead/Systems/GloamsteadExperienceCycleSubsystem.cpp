#include "Systems/GloamsteadExperienceCycleSubsystem.h"

#include "UObject/UObjectGlobals.h"

namespace
{
	/**
	 * The canonical authored cycles, built once from the in-code definition.
	 *
	 * C++ stays the authority on what an authored cycle IS; the shipped DA_ExperienceCycleCatalog is
	 * transport. This used to be six hand-written `case` blocks, each listing whichever fields its
	 * author remembered - which meant a field nobody listed (a second reading, a support medium)
	 * could drift between the asset and the code forever without anything noticing. Comparing
	 * against the canonical plan checks EVERY field by construction, and adding a cycle now needs
	 * only the authored row plus its entry in PopulateDefaultExperienceCyclePlans.
	 */
	const UExperienceCycleCatalog& GetCanonicalCatalog()
	{
		static UExperienceCycleCatalog* Canonical = []()
		{
			UExperienceCycleCatalog* Built = NewObject<UExperienceCycleCatalog>(
				GetTransientPackage(), TEXT("GloamsteadCanonicalExperienceCycleCatalog"));
			PopulateDefaultExperienceCyclePlans(*Built);
			Built->AddToRoot();
			return Built;
		}();
		return *Canonical;
	}

	const FExperienceCyclePlan* FindCanonicalPlanForSlot(int32 Slot)
	{
		const UExperienceCycleCatalog& Canonical = GetCanonicalCatalog();
		const FExperienceCyclePlan* Match = nullptr;
		for (const FExperienceCyclePlan& Candidate : Canonical.AuthoredPlans)
		{
			if (Candidate.Slot != Slot || !Candidate.IsAuthoredPlan())
			{
				continue;
			}
			if (Match)
			{
				// The canonical source itself is malformed. Refuse rather than pick one.
				return nullptr;
			}
			Match = &Candidate;
		}
		return Match;
	}

	bool NameArraysMatchInOrder(const TArray<FName>& Left, const TArray<FName>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			// Order is part of the contract: RequiredSupportChannelTypes[i] declares the medium of
			// RequiredSupportIds[i], so a reordered pair silently reassigns every clue's medium.
			if (Left[Index] != Right[Index])
			{
				return false;
			}
		}
		return true;
	}

	bool SecondReadingsMatch(
		const TArray<FExperienceCycleSecondReading>& Left,
		const TArray<FExperienceCycleSecondReading>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			const FExperienceCycleSecondReading& A = Left[Index];
			const FExperienceCycleSecondReading& B = Right[Index];
			if (A.ReadingId != B.ReadingId
				|| A.Grade != B.Grade
				|| A.ConsequenceTag != B.ConsequenceTag
				|| !A.ChoicePrompt.ToString().Equals(B.ChoicePrompt.ToString(), ESearchCase::CaseSensitive)
				|| !A.OutcomeSummary.ToString().Equals(B.OutcomeSummary.ToString(), ESearchCase::CaseSensitive))
			{
				return false;
			}
		}
		return true;
	}

	/** Every authored field must agree with the canonical plan for that slot. */
	bool MatchesRequiredContract(const FExperienceCyclePlan& Plan, int32 Slot)
	{
		if (!Plan.IsAuthoredPlan() || Plan.Slot != Slot)
		{
			return false;
		}

		const FExperienceCyclePlan* Canonical = FindCanonicalPlanForSlot(Slot);
		if (!Canonical)
		{
			return false;
		}

		if (Plan.PlanId != Canonical->PlanId
			|| Plan.WarningId != Canonical->WarningId
			|| Plan.NightType != Canonical->NightType
			|| Plan.SemanticSubject != Canonical->SemanticSubject
			|| Plan.RequiredRitualType != Canonical->RequiredRitualType
			|| Plan.MinimumDistinctSupportCount != Canonical->MinimumDistinctSupportCount
			|| Plan.InterpretationReceiptId != Canonical->InterpretationReceiptId
			|| Plan.VisualStateKey != Canonical->VisualStateKey
			|| Plan.OutcomeSummaryKey != Canonical->OutcomeSummaryKey)
		{
			return false;
		}

		if (!NameArraysMatchInOrder(Plan.RequiredRestorationTags, Canonical->RequiredRestorationTags)
			|| !NameArraysMatchInOrder(Plan.RequiredSupportIds, Canonical->RequiredSupportIds)
			|| !NameArraysMatchInOrder(Plan.RequiredSupportChannelTypes, Canonical->RequiredSupportChannelTypes))
		{
			return false;
		}

		if (!SecondReadingsMatch(Plan.SecondReadings, Canonical->SecondReadings))
		{
			return false;
		}

		// A canonical plan that cannot pass its own authoring rule is a defect in this file, not in
		// the asset - but it must still refuse, or a half-authored reading set would ship.
		return Plan.HasCoherentSecondReadings();
	}
}

void UGloamsteadExperienceCycleSubsystem::EnsureCatalog()
{
	if (ExperienceCatalog)
	{
		return;
	}

	ExperienceCatalog = Cast<UExperienceCycleCatalog>(StaticLoadObject(
		UExperienceCycleCatalog::StaticClass(), nullptr,
		TEXT("/Game/Data/DA_ExperienceCycleCatalog.DA_ExperienceCycleCatalog")));
	if (ExperienceCatalog)
	{
		// Say which source won. Without this the authored asset and the code fallback are
		// indistinguishable at runtime, which is precisely how code-as-content survives unnoticed.
		UE_LOG(LogTemp, Log,
			TEXT("ExperienceCycle: loaded the authored plan catalog from /Game/Data/DA_ExperienceCycleCatalog (%d plan(s))."),
			ExperienceCatalog->AuthoredPlans.Num());
		return;
	}

	ExperienceCatalog = NewObject<UExperienceCycleCatalog>(this, TEXT("DefaultExperienceCycleCatalog"));
	PopulateDefaultExperienceCyclePlans(*ExperienceCatalog);
	UE_LOG(LogTemp, Warning,
		TEXT("ExperienceCycle: /Game/Data/DA_ExperienceCycleCatalog is absent - falling back to the in-code "
			 "development plans (%d plan(s)). This fallback is NOT production content: author the plans in "
			 "specs/data/vs-polish-starter.json and re-import with "
			 "agent_collab/scripts/Invoke-GloamsteadDataAssetImport.ps1."),
		ExperienceCatalog->AuthoredPlans.Num());
}

int32 UGloamsteadExperienceCycleSubsystem::GetAuthoredSlotCount() const
{
	return ExperienceCatalog ? ExperienceCatalog->AuthoredPlans.Num() : 0;
}

const FExperienceCyclePlan* UGloamsteadExperienceCycleSubsystem::FindCanonicalRequiredPlan(int32 Slot) const
{
	// The ceiling is the authored catalog's size, not a literal. It was hardcoded to 4 in three places,
	// which meant authoring a fifth cycle would have been silently ignored - the catalog would grow and the
	// experience would still end after the fourth night.
	if (!ExperienceCatalog || Slot < 1 || Slot > GetAuthoredSlotCount())
	{
		return nullptr;
	}

	const FExperienceCyclePlan* Match = nullptr;
	for (const FExperienceCyclePlan& Candidate : ExperienceCatalog->AuthoredPlans)
	{
		if (Candidate.Slot != Slot)
		{
			continue;
		}

		if (Match || !MatchesRequiredContract(Candidate, Slot))
		{
			return nullptr;
		}
		Match = &Candidate;
	}

	return Match;
}

void UGloamsteadExperienceCycleSubsystem::SetInvalidPlan(int32 Slot)
{
	PersistentState.ArmedPlanId = NAME_None;
	ActivePlan = FExperienceCyclePlan::MakeInvalid(Slot);
}

void UGloamsteadExperienceCycleSubsystem::SetGenericHandoff(int32 Slot)
{
	PersistentState.ArmedPlanId = NAME_None;
	ActivePlan = FExperienceCyclePlan::MakeGenericHandoff(Slot);
}

bool UGloamsteadExperienceCycleSubsystem::EnsureUpcomingPlan()
{
	if (bPersistentRestoreFailed || PersistentState.bRequiresLegacyReconciliation)
	{
		SetInvalidPlan(PersistentState.CompletedCycleSlot + 1);
		return false;
	}

	if (ActivePlan.IsAuthoredPlan())
	{
		return true;
	}

	const int32 UpcomingSlot = PersistentState.CompletedCycleSlot + 1;
	if (UpcomingSlot < 1)
	{
		SetInvalidPlan(UpcomingSlot);
		return false;
	}

	// Load the catalog BEFORE asking how many slots it authors, or the ceiling would always read zero.
	EnsureCatalog();

	if (UpcomingSlot > GetAuthoredSlotCount())
	{
		// The authored experience is finished. This is a terminal, player-meaningful state - not an error -
		// and IsExperienceComplete() is what the rest of the game reads to present an ending rather than
		// leaving the player beside a Heart that has silently stopped answering.
		SetGenericHandoff(UpcomingSlot);
		UE_LOG(LogTemp, Log,
			TEXT("ExperienceCycle: the authored experience is complete - %d of %d cycles finished. No further "
				 "authored plan exists; the Heart should now present an ending rather than a rest."),
			PersistentState.CompletedCycleSlot, GetAuthoredSlotCount());
		return false;
	}

	const FExperienceCyclePlan* CanonicalPlan = FindCanonicalRequiredPlan(UpcomingSlot);
	if (!CanonicalPlan)
	{
		// The catalog authors a row for this slot, but it does not satisfy the canonical contract in
		// MatchesRequiredContract. That gate is deliberate: the asset is transport, and C++ stays the
		// authority on what an authored cycle IS, so an edited or forged catalog cannot invent a night.
		// Refusing here is correct - but say why, because "authored a new cycle and nothing happened" is
		// otherwise indistinguishable from the experience simply ending.
		UE_LOG(LogTemp, Error,
			TEXT("ExperienceCycle: slot %d has no authored row satisfying the canonical contract, so it is "
				 "refused - the row is missing, duplicated, or does not match. The catalog is transport; C++ "
				 "stays the authority on what a cycle IS, so adding one needs BOTH an authored row and a "
				 "matching entry in PopulateDefaultExperienceCyclePlans."),
			UpcomingSlot);
		SetInvalidPlan(UpcomingSlot);
		return false;
	}

	ActivePlan = *CanonicalPlan;
	PersistentState.ArmedPlanId = ActivePlan.PlanId;
	return true;
}

bool UGloamsteadExperienceCycleSubsystem::RestoreArmedPlan(FName ArmedPlanId, int32 ExpectedSlot)
{
	EnsureCatalog();
	if (ExpectedSlot < 1 || ExpectedSlot > GetAuthoredSlotCount())
	{
		SetInvalidPlan(ExpectedSlot);
		return false;
	}

	const FExperienceCyclePlan* CanonicalPlan = FindCanonicalRequiredPlan(ExpectedSlot);
	if (!CanonicalPlan || CanonicalPlan->PlanId != ArmedPlanId)
	{
		SetInvalidPlan(ExpectedSlot);
		return false;
	}

	ActivePlan = *CanonicalPlan;
	PersistentState.ArmedPlanId = ActivePlan.PlanId;
	return true;
}

bool UGloamsteadExperienceCycleSubsystem::RestorePersistentState(const FExperienceCyclePersistentState& InPersistentState)
{
	PersistentState = InPersistentState;
	bPersistentRestoreFailed = false;
	ActivePlan = FExperienceCyclePlan::MakeInvalid(PersistentState.CompletedCycleSlot + 1);

	const int32 UpcomingSlot = PersistentState.CompletedCycleSlot + 1;
	if (PersistentState.CompletedCycleSlot < 0 || PersistentState.bRequiresLegacyReconciliation)
	{
		SetInvalidPlan(UpcomingSlot);
		bPersistentRestoreFailed = true;
		return false;
	}

	if (PersistentState.ArmedPlanId == NAME_None)
	{
		return true;
	}

	const bool bRestored = RestoreArmedPlan(PersistentState.ArmedPlanId, UpcomingSlot);
	bPersistentRestoreFailed = !bRestored;
	return bRestored;
}

FExperienceCyclePersistentState UGloamsteadExperienceCycleSubsystem::CapturePersistentState() const
{
	FExperienceCyclePersistentState CapturedState = PersistentState;
	CapturedState.ArmedPlanId = ActivePlan.IsAuthoredPlan() ? ActivePlan.PlanId : NAME_None;
	return CapturedState;
}

bool UGloamsteadExperienceCycleSubsystem::RecordActivePlanOutcome(const FNightRuntimeOutcome& Outcome)
{
	if (!ActivePlan.IsAuthoredPlan() || Outcome.NightType != ActivePlan.NightType)
	{
		return false;
	}

	PersistentState.CompletedCycleSlot = ActivePlan.Slot;
	PersistentState.LastPlanId = ActivePlan.PlanId;
	PersistentState.LastOutcomeResultTag = Outcome.ResultTag;
	if (ActivePlan.Slot == 1)
	{
		PersistentState.bFirstRestCompleted = true;
	}

	// ScarTags are durable aftermath, not a second copy of every successful
	// outcome. Keep an explicit failure marker only when the runtime supplied
	// one, and avoid duplicating it across a restore/retry.
	if (Outcome.Result == ENightOutcomeResult::Failure
		&& Outcome.ResultTag != NAME_None
		&& !PersistentState.ScarTags.Contains(Outcome.ResultTag))
	{
		PersistentState.ScarTags.Add(Outcome.ResultTag);
	}

	// A second reading leaves a durable mark regardless of how the night scored, because the mark IS
	// the payoff: a player who opened the sluice should still carry Boon.GardenAura into Cycle III
	// even on a night that only went partly right, and a player who ashed the bed carries Scar.AshFed
	// even on one they survived. Tying it to the result would make the sharper reading matter only
	// when the night was already going well.
	if (Outcome.SecondReadingTag != NAME_None
		&& !PersistentState.ScarTags.Contains(Outcome.SecondReadingTag))
	{
		PersistentState.ScarTags.Add(Outcome.SecondReadingTag);
	}

	PersistentState.ArmedPlanId = NAME_None;
	ActivePlan = FExperienceCyclePlan::MakeInvalid(PersistentState.CompletedCycleSlot + 1);
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void UGloamsteadExperienceCycleSubsystem::Test_SetCatalog(UExperienceCycleCatalog* InCatalog)
{
	ExperienceCatalog = InCatalog;
	ActivePlan = FExperienceCyclePlan::MakeInvalid(PersistentState.CompletedCycleSlot + 1);
}
#endif
