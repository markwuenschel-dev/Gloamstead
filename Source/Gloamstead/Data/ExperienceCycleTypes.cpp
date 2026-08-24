#include "Data/ExperienceCycleTypes.h"

FExperienceCyclePlan FExperienceCyclePlan::MakeInvalid(int32 InSlot)
{
	FExperienceCyclePlan Plan;
	Plan.Slot = InSlot;
	Plan.Resolution = EExperiencePlanResolution::Invalid;
	return Plan;
}

FExperienceCyclePlan FExperienceCyclePlan::MakeGenericHandoff(int32 InSlot)
{
	FExperienceCyclePlan Plan;
	Plan.Slot = InSlot;
	Plan.Resolution = EExperiencePlanResolution::GenericHandoff;
	return Plan;
}

void PopulateDefaultExperienceCyclePlans(UExperienceCycleCatalog& Catalog)
{
	Catalog.AuthoredPlans.Reset();

	FExperienceCyclePlan Tutorial;
	Tutorial.Slot = 1;
	Tutorial.PlanId = TEXT("Cycle1_Tutorial");
	Tutorial.WarningId = TEXT("TutorialLostPath");
	Tutorial.NightType = ENightConsequenceType::Tutorial;
	Tutorial.SemanticSubject = TEXT("courtyard.lantern.first");
	Tutorial.RequiredRestorationTags = { TEXT("LanternPost") };
	Tutorial.VisualStateKey = TEXT("restoration_level");
	Tutorial.OutcomeSummaryKey = TEXT("Cycle1_Tutorial");
	Tutorial.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Tutorial);

	FExperienceCyclePlan Garden;
	Garden.Slot = 2;
	Garden.PlanId = TEXT("Cycle2_Garden");
	Garden.WarningId = TEXT("GardenRot");
	Garden.NightType = ENightConsequenceType::Corruption;
	Garden.SemanticSubject = TEXT("Cycle2_Garden");
	Garden.RequiredRestorationTags = { TEXT("GardenBed") };
	Garden.RequiredRitualType = ERitualType::GardenBed;
	Garden.RequiredSupportIds = {
		TEXT("GardenRot.WitheredVines"),
		TEXT("GardenRot.ColdSoil"),
		TEXT("GardenRot.BellMoths")
	};
	Garden.RequiredSupportChannelTypes = {
		TEXT("Environmental"),
		TEXT("ObjectReaction"),
		TEXT("Audio")
	};
	Garden.MinimumDistinctSupportCount = 2;
	Garden.InterpretationReceiptId = TEXT("GardenRot.Interpreted");
	Garden.VisualStateKey = TEXT("restoration_level");
	Garden.OutcomeSummaryKey = TEXT("Cycle2_Garden");
	Garden.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Garden);

	FExperienceCyclePlan Retrieval;
	Retrieval.Slot = 3;
	Retrieval.PlanId = TEXT("Cycle3_Retrieval");
	Retrieval.WarningId = TEXT("GardenRot");
	Retrieval.NightType = ENightConsequenceType::Retrieval;
	// Retrieval is the second reading of the same place: the night tests what
	// the player mended rather than inventing a new, unrelated target.
	Retrieval.SemanticSubject = TEXT("Cycle2_Garden");
	Retrieval.RequiredRestorationTags = { TEXT("GardenBed") };
	Retrieval.RequiredRitualType = ERitualType::GardenBed;
	Retrieval.RequiredSupportIds = Garden.RequiredSupportIds;
	Retrieval.RequiredSupportChannelTypes = Garden.RequiredSupportChannelTypes;
	Retrieval.MinimumDistinctSupportCount = 2;
	Retrieval.InterpretationReceiptId = TEXT("GardenRot.Retrieved");
	Retrieval.VisualStateKey = TEXT("restoration_level");
	Retrieval.OutcomeSummaryKey = TEXT("Cycle3_Retrieval");
	Retrieval.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Retrieval);

	FExperienceCyclePlan Possession;
	Possession.Slot = 4;
	Possession.PlanId = TEXT("Cycle4_Possession");
	// The fourth night deliberately re-reads the garden's existing warning. The
	// player already restored this place; the new evidence is its unnatural silence,
	// so the consequence tests attention rather than introducing an unrelated POI.
	Possession.WarningId = TEXT("GardenRot");
	Possession.NightType = ENightConsequenceType::SilencePossession;
	Possession.SemanticSubject = TEXT("Cycle2_Garden");
	Possession.RequiredRestorationTags = { TEXT("GardenBed") };
	Possession.RequiredRitualType = ERitualType::GardenBed;
	Possession.RequiredSupportIds = Garden.RequiredSupportIds;
	Possession.RequiredSupportChannelTypes = Garden.RequiredSupportChannelTypes;
	Possession.MinimumDistinctSupportCount = 2;
	Possession.InterpretationReceiptId = TEXT("GardenRot.Possessed");
	Possession.VisualStateKey = TEXT("restoration_level");
	Possession.OutcomeSummaryKey = TEXT("Cycle4_Possession");
	Possession.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Possession);

	FExperienceCyclePlan Mirror;
	Mirror.Slot = 5;
	Mirror.PlanId = TEXT("Cycle5_Mirror");
	Mirror.WarningId = TEXT("MirrorDebt");
	Mirror.NightType = ENightConsequenceType::Mirror;
	// The current authored world has no materialized BellShrine point. Cycle V
	// therefore reads the restored garden's still water as the first mirror:
	// the semantic subject remains exact and the player is never sent to an
	// unresolved procedural target.
	Mirror.SemanticSubject = TEXT("Cycle2_Garden");
	Mirror.RequiredRestorationTags = { TEXT("GardenBed") };
	Mirror.RequiredRitualType = ERitualType::GardenBed;
	Mirror.RequiredSupportIds = {
		TEXT("MirrorDebt.StillWater"),
		TEXT("MirrorDebt.DoubleShadow"),
		TEXT("MirrorDebt.HeartWhisper")
	};
	Mirror.RequiredSupportChannelTypes = {
		TEXT("Environmental"),
		TEXT("ObjectReaction"),
		TEXT("Audio")
	};
	Mirror.MinimumDistinctSupportCount = 2;
	Mirror.InterpretationReceiptId = TEXT("MirrorDebt.Interpreted");
	Mirror.VisualStateKey = TEXT("restoration_level");
	Mirror.OutcomeSummaryKey = TEXT("Cycle5_Mirror");
	Mirror.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Mirror);
}

void FExperienceCyclePersistentState::ResetForLegacyReconciliation()
{
    CompletedCycleSlot = 0;
    ArmedPlanId = NAME_None;
    LastPlanId = NAME_None;
    LastOutcomeResultTag = NAME_None;
    ScarTags.Reset();
    bFirstRestCompleted = false;
    SavedPhaseOrdinal = INDEX_NONE;
    bRequiresLegacyReconciliation = true;
	HeartInterpretationState.Reset();
}
