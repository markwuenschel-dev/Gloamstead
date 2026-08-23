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

    if (SaveVersion == 2)
    {
        // V2 has legitimate PCG and authored-cycle facts, but predates an
        // auditable interpretation payload. Retaining an invented warning,
        // support encounter, or receipt would let knowledge from a future
        // timeline survive a rollback, so clear only those facts.
        ExperienceCycleState.HeartInterpretationState.Reset();
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
