#include "Save/GloamsteadSaveGame.h"

bool UGloamsteadSaveGame::MigrateToCurrentVersion()
{
    if (SaveVersion == 1)
    {
        // Version 1 predates authored-cycle persistence. Its PCG fields remain
        // untouched, while every invented progression claim is erased.
        ExperienceCycleState.ResetForLegacyReconciliation();
        SaveVersion = CurrentSaveVersion;
        return true;
    }

    if (SaveVersion == CurrentSaveVersion)
    {
        return true;
    }

    if (SaveVersion <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("UGloamsteadSaveGame: rejected invalid save version %d."), SaveVersion);
        return false;
    }

    UE_LOG(LogTemp, Error, TEXT("UGloamsteadSaveGame: rejected newer save version %d (current is %d)."), SaveVersion, CurrentSaveVersion);
    return false;
}
