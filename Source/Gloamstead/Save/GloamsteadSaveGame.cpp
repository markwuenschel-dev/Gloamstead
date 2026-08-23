#include "Save/GloamsteadSaveGame.h"

void UGloamsteadSaveGame::MigrateToCurrentVersion()
{
    if (SaveVersion == 1)
    {
        // Version 1 predates authored-cycle persistence. Its PCG fields remain
        // untouched, while every invented progression claim is erased.
        ExperienceCycleState.ResetForLegacyReconciliation();
        SaveVersion = CurrentSaveVersion;
    }
}
