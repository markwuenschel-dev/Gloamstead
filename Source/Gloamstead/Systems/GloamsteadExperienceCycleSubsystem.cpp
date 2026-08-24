#include "Systems/GloamsteadExperienceCycleSubsystem.h"

#include "UObject/UObjectGlobals.h"

namespace
{
	bool HasOnlyTag(const FExperienceCyclePlan& Plan, FName Tag)
	{
		return Plan.RequiredRestorationTags.Num() == 1 && Plan.RequiredRestorationTags[0] == Tag;
	}

	bool HasExactGardenSupports(const FExperienceCyclePlan& Plan)
	{
		static const FName CanonicalIds[] = {
			TEXT("GardenRot.WitheredVines"),
			TEXT("GardenRot.ColdSoil"),
			TEXT("GardenRot.BellMoths")
		};
		static const FName CanonicalMedia[] = {
			TEXT("Environmental"),
			TEXT("ObjectReaction"),
			TEXT("Audio")
		};

		if (Plan.RequiredSupportIds.Num() != UE_ARRAY_COUNT(CanonicalIds)
			|| Plan.RequiredSupportChannelTypes.Num() != UE_ARRAY_COUNT(CanonicalMedia))
		{
			return false;
		}

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(CanonicalIds); ++Index)
		{
			if (Plan.RequiredSupportIds[Index] != CanonicalIds[Index]
				|| Plan.RequiredSupportChannelTypes[Index] != CanonicalMedia[Index])
			{
				return false;
			}
		}
		return true;
	}

	bool MatchesRequiredContract(const FExperienceCyclePlan& Plan, int32 Slot)
	{
		if (!Plan.IsAuthoredPlan())
		{
			return false;
		}

		switch (Slot)
		{
		case 1:
			return Plan.Slot == 1
				&& Plan.PlanId == FName(TEXT("Cycle1_Tutorial"))
				&& Plan.WarningId == FName(TEXT("TutorialLostPath"))
				&& Plan.NightType == ENightConsequenceType::Tutorial
				&& Plan.SemanticSubject == FName(TEXT("courtyard.lantern.first"))
				&& HasOnlyTag(Plan, FName(TEXT("LanternPost")))
				&& Plan.RequiredRitualType == ERitualType::Invalid
				&& Plan.RequiredSupportIds.IsEmpty()
				&& Plan.RequiredSupportChannelTypes.IsEmpty()
				&& Plan.MinimumDistinctSupportCount == 0
				&& Plan.InterpretationReceiptId == NAME_None
				&& Plan.VisualStateKey == FName(TEXT("restoration_level"))
				&& Plan.OutcomeSummaryKey == FName(TEXT("Cycle1_Tutorial"));

		case 2:
			return Plan.Slot == 2
				&& Plan.PlanId == FName(TEXT("Cycle2_Garden"))
				&& Plan.WarningId == FName(TEXT("GardenRot"))
				&& Plan.NightType == ENightConsequenceType::Corruption
				&& Plan.SemanticSubject == FName(TEXT("Cycle2_Garden"))
				&& HasOnlyTag(Plan, FName(TEXT("GardenBed")))
				&& Plan.RequiredRitualType == ERitualType::GardenBed
				&& HasExactGardenSupports(Plan)
				&& Plan.MinimumDistinctSupportCount == 2
				&& Plan.InterpretationReceiptId == FName(TEXT("GardenRot.Interpreted"))
				&& Plan.VisualStateKey == FName(TEXT("restoration_level"))
				&& Plan.OutcomeSummaryKey == FName(TEXT("Cycle2_Garden"));

		case 3:
			return Plan.Slot == 3
				&& Plan.PlanId == FName(TEXT("Cycle3_Retrieval"))
				&& Plan.WarningId == FName(TEXT("GardenRot"))
				&& Plan.NightType == ENightConsequenceType::Retrieval
				&& Plan.SemanticSubject == FName(TEXT("Cycle2_Garden"))
				&& HasOnlyTag(Plan, FName(TEXT("GardenBed")))
				&& Plan.RequiredRitualType == ERitualType::GardenBed
				&& HasExactGardenSupports(Plan)
				&& Plan.MinimumDistinctSupportCount == 2
				&& Plan.InterpretationReceiptId == FName(TEXT("GardenRot.Retrieved"))
				&& Plan.VisualStateKey == FName(TEXT("restoration_level"))
				&& Plan.OutcomeSummaryKey == FName(TEXT("Cycle3_Retrieval"));

		case 4:
			return Plan.Slot == 4
				&& Plan.PlanId == FName(TEXT("Cycle4_Possession"))
				&& Plan.WarningId == FName(TEXT("GardenRot"))
				&& Plan.NightType == ENightConsequenceType::SilencePossession
				&& Plan.SemanticSubject == FName(TEXT("Cycle2_Garden"))
				&& HasOnlyTag(Plan, FName(TEXT("GardenBed")))
				&& Plan.RequiredRitualType == ERitualType::GardenBed
				&& HasExactGardenSupports(Plan)
				&& Plan.MinimumDistinctSupportCount == 2
				&& Plan.InterpretationReceiptId == FName(TEXT("GardenRot.Possessed"))
				&& Plan.VisualStateKey == FName(TEXT("restoration_level"))
				&& Plan.OutcomeSummaryKey == FName(TEXT("Cycle4_Possession"));

		default:
			return false;
		}
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

const FExperienceCyclePlan* UGloamsteadExperienceCycleSubsystem::FindCanonicalRequiredPlan(int32 Slot) const
{
	if (!ExperienceCatalog || Slot < 1 || Slot > 4)
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

	if (UpcomingSlot > 4)
	{
		SetGenericHandoff(UpcomingSlot);
		return false;
	}

	EnsureCatalog();
	const FExperienceCyclePlan* CanonicalPlan = FindCanonicalRequiredPlan(UpcomingSlot);
	if (!CanonicalPlan)
	{
		SetInvalidPlan(UpcomingSlot);
		return false;
	}

	ActivePlan = *CanonicalPlan;
	PersistentState.ArmedPlanId = ActivePlan.PlanId;
	return true;
}

bool UGloamsteadExperienceCycleSubsystem::RestoreArmedPlan(FName ArmedPlanId, int32 ExpectedSlot)
{
	if (ExpectedSlot < 1 || ExpectedSlot > 4)
	{
		SetInvalidPlan(ExpectedSlot);
		return false;
	}

	EnsureCatalog();
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
