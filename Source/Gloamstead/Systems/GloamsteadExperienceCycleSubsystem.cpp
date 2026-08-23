#include "Systems/GloamsteadExperienceCycleSubsystem.h"

#include "UObject/UObjectGlobals.h"

namespace
{
	bool HasOnlyTag(const FExperienceCyclePlan& Plan, FName Tag)
	{
		return Plan.RequiredRestorationTags.Num() == 1 && Plan.RequiredRestorationTags[0] == Tag;
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
				&& Plan.WarningId == FName(TEXT("Tutorial"))
				&& Plan.NightType == ENightConsequenceType::Tutorial
				&& Plan.SemanticSubject == FName(TEXT("courtyard.lantern.first"))
				&& HasOnlyTag(Plan, FName(TEXT("LanternPost")))
				&& Plan.VisualStateKey == FName(TEXT("restoration_level"))
				&& Plan.OutcomeSummaryKey == FName(TEXT("Cycle1_Tutorial"));

		case 2:
			return Plan.Slot == 2
				&& Plan.PlanId == FName(TEXT("Cycle2_Garden"))
				&& Plan.WarningId == FName(TEXT("GardenRot"))
				&& Plan.NightType == ENightConsequenceType::Corruption
				&& Plan.SemanticSubject == FName(TEXT("Cycle2_Garden"))
				&& HasOnlyTag(Plan, FName(TEXT("GardenBed")))
				&& Plan.VisualStateKey == FName(TEXT("restoration_level"))
				&& Plan.OutcomeSummaryKey == FName(TEXT("Cycle2_Garden"));

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
		return;
	}

	ExperienceCatalog = NewObject<UExperienceCycleCatalog>(this, TEXT("DefaultExperienceCycleCatalog"));
	PopulateDefaultExperienceCyclePlans(*ExperienceCatalog);
}

const FExperienceCyclePlan* UGloamsteadExperienceCycleSubsystem::FindCanonicalRequiredPlan(int32 Slot) const
{
	if (!ExperienceCatalog || Slot < 1 || Slot > 2)
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

	if (UpcomingSlot > 2)
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
	if (ExpectedSlot < 1 || ExpectedSlot > 2)
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

#if WITH_DEV_AUTOMATION_TESTS
void UGloamsteadExperienceCycleSubsystem::Test_SetCatalog(UExperienceCycleCatalog* InCatalog)
{
	ExperienceCatalog = InCatalog;
	ActivePlan = FExperienceCyclePlan::MakeInvalid(PersistentState.CompletedCycleSlot + 1);
}
#endif
