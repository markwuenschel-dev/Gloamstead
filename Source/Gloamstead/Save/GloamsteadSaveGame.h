#pragma once

#include "CoreMinimal.h"
#include "Data/ExperienceCycleTypes.h"
#include "GameFramework/SaveGame.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "GloamsteadSaveGame.generated.h"

class UGloamsteadDayNightSubsystem;

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

#if WITH_DEV_AUTOMATION_TESTS
    /** Fixture-only writer for migration/load tests. Production writes are DayNight-only. */
    void Test_SetExperienceCycleState(const FExperienceCyclePersistentState& InState)
    {
        SetExperienceCycleState(InState);
    }
#endif

private:
    // A raw cycle payload can include an interpretation receipt. It is never a
    // generic SaveGame authoring surface: DayNight captures it atomically with
    // validated PCG/cycle state, while automation gets the macro-gated writer.
    friend class UGloamsteadDayNightSubsystem;

    /** Authored progression facts persisted alongside the existing PCG payload. */
    UPROPERTY()
    FExperienceCyclePersistentState ExperienceCycleState;

    void SetExperienceCycleState(const FExperienceCyclePersistentState& InState) { ExperienceCycleState = InState; }
};
