#pragma once

#include "CoreMinimal.h"
#include "Data/ExperienceCycleTypes.h"
#include "GameFramework/SaveGame.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "GloamsteadSaveGame.generated.h"

/**
 * Vertical-slice save payload for the ritual sanctuary.
 *
 * Unlike the lightweight restored-index persistence (GetRestoredPointIndices /
 * ReapplyRestoredState), this captures the FULL per-point state — light and
 * corruption levels as well as restored flags — so a load is a true round-trip.
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    /** Current save layout version. New save objects always begin at this version. */
    static constexpr int32 CurrentSaveVersion = 3;

    /** Full per-point state, ordered by point index (mirrors the subsystem's PointStates). */
    UPROPERTY()
    TArray<FRitualPointState> PointStates;

    /** Indices that have been restored (mirrors the subsystem's RestoredPointIndices set). */
    UPROPERTY()
    TArray<int32> RestoredPointIndices;

    /** Seed the world was generated with, for deterministic regeneration on load. */
    UPROPERTY()
    int32 WorldSeed = 0;

    /** Bumped when the payload layout changes, so loads can migrate or reject. */
    UPROPERTY()
    int32 SaveVersion = CurrentSaveVersion;

    /** Authored progression facts persisted alongside the existing PCG payload. */
    UPROPERTY()
    FExperienceCyclePersistentState ExperienceCycleState;

    /**
     * Migrate this payload without consulting world state or selecting authored progression.
     * V1 retains PCG data but enters an explicit reconciliation state. V2 retains
     * PCG/cycle facts but clears the newly-versioned Heart interpretation state,
     * because old payloads cannot prove which presented warning or evidence led
     * to a receipt. V3 is unchanged.
     * Returns false for invalid or newer schemas so callers do not restore an unsupported payload.
     */
    bool MigrateToCurrentVersion();

    const FExperienceCyclePersistentState& GetExperienceCycleState() const { return ExperienceCycleState; }
    void SetExperienceCycleState(const FExperienceCyclePersistentState& InState) { ExperienceCycleState = InState; }
};
