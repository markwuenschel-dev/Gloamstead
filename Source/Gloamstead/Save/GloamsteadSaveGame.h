#pragma once

#include "CoreMinimal.h"
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
    int32 SaveVersion = 1;
};
