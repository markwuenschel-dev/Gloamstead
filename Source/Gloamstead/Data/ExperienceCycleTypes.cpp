#include "Data/ExperienceCycleTypes.h"

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
}
