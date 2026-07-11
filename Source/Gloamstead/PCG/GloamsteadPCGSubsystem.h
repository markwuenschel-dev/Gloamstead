#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/PCGPointData.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "GloamsteadPCGSubsystem.generated.h"

class UGloamsteadSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStructureRestored, const FRestorationEventPayload&, Payload);

USTRUCT()
struct FRitualPointState
{
    GENERATED_BODY()

    UPROPERTY()
    bool bIsRestored = false;

    UPROPERTY()
    float LightLevel = 0.0f;

    UPROPERTY()
    float CorruptionLevel = 0.0f;
};

USTRUCT()
struct FRitualSpatialCell
{
	GENERATED_BODY()

	TArray<int32> PointIndices;
};

UCLASS()
class GLOAMSTEAD_API UGloamsteadPCGSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // === Lifecycle ===
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === Initialization ===
    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    void InitializeFromPCGComponent(UPCGComponent* PCGComponent, int32 WorldSeed);

    // === Core Queries (use fast parallel state where possible) ===
    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    bool GetPointByIndex(int32 PointIndex, FPCGPoint& OutPoint) const;

    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    TArray<FPCGPoint> GetPointsByRitualType(ERitualType Type, bool bOnlyRestored = false) const;

    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    TArray<FPCGPoint> GetPointsAlongPath(int32 PathSegmentID) const;

    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    int32 FindNearestUnrestoredPointIndex(const FVector& Location, ERitualType Type, float SearchRadius = 1600.f) const;

    UFUNCTION(BlueprintPure, Category="PCG|Ritual")
    bool IsPointRestored(int32 PointIndex) const;

    UFUNCTION(BlueprintPure, Category="PCG|Ritual")
    float GetLightLevel(int32 PointIndex) const;

    // === Attribute access (metadata-backed; safe for points from GetPointByIndex / GetPoints*) ===
    int32 GetIntAttribute(const FPCGPoint& Point, FName AttributeName, int32 DefaultValue = -1) const;
    float GetFloatAttribute(const FPCGPoint& Point, FName AttributeName, float DefaultValue = 0.0f) const;
    bool GetBoolAttribute(const FPCGPoint& Point, FName AttributeName, bool DefaultValue = false) const;
    FName GetNameAttribute(const FPCGPoint& Point, FName AttributeName, FName DefaultValue = NAME_None) const;
    FVector GetVectorAttribute(const FPCGPoint& Point, FName AttributeName, FVector DefaultValue = FVector::UpVector) const;

    UFUNCTION(BlueprintPure, Category="PCG|Ritual")
    float GetCorruptionLevel(int32 PointIndex) const;

    // === Sanctuary aggregates (read-only; safe defaults when uninitialized) ===
    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    float GetSanctuaryAverageLightLevel() const;

    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    float GetSanctuaryAverageCorruptionLevel() const;

    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    int32 GetRestoredPointCount() const;

    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    int32 GetRestoredCountByRitualType(ERitualType Type) const;

    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    FNightSanctuarySnapshot BuildSanctuarySnapshot() const;

    // === State Mutation (optimized hot path) ===
    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    bool ApplyRestoration(int32 PointIndex, const FRestorationEventPayload& Payload);

    /** Night-only corruption spread; does not alter restoration flags. MaxPoints clamped internally (default cap 32). */
    UFUNCTION(BlueprintCallable, Category="PCG|Night")
    int32 ApplyCorruptionSpread(float Delta, int32 MaxPoints = 8);

    // === Persistence (Vertical Slice Strategy) ===
    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    TSet<int32> GetRestoredPointIndices() const;

    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    void ReapplyRestoredState(const TSet<int32>& RestoredIndices);

    // === Full persistence (per-point state, not just restored flags) ===
    /** Copy full per-point state (light/corruption/restored) + restored set + seed into a save object. */
    UFUNCTION(BlueprintCallable, Category="PCG|Persistence")
    void CaptureToSaveGame(UGloamsteadSaveGame* SaveGame) const;

    /** Replace current per-point state with the save object's contents (full round-trip, unlike ReapplyRestoredState). */
    UFUNCTION(BlueprintCallable, Category="PCG|Persistence")
    void RestoreFromSaveGame(const UGloamsteadSaveGame* SaveGame);

    /** Convenience: capture into a fresh save object and write it to a named slot. */
    UFUNCTION(BlueprintCallable, Category="PCG|Persistence")
    bool SaveToSlot(const FString& SlotName, int32 UserIndex = 0) const;

    /** Convenience: load a named slot and apply it. Returns false if the slot is missing/invalid. */
    UFUNCTION(BlueprintCallable, Category="PCG|Persistence")
    bool LoadFromSlot(const FString& SlotName, int32 UserIndex = 0);

    /** Shared default slot for the sanctuary's persistent state (dawn autosave + load-on-start). */
    static const FString DefaultSaveSlot;

    // === Debugging ===
    UFUNCTION(BlueprintCallable, Category="PCG|Ritual|Debug")
    void DrawDebugRitualPoints(float Duration = 0.0f) const;

    UFUNCTION(BlueprintCallable, Category="PCG|Ritual|Debug")
    void DrawDebugSpatialGrid(float Duration = 0.0f) const;

    UFUNCTION(BlueprintCallable, Category="PCG|Ritual|Debug")
    void SetDrawSpatialGridDebug(bool bEnabled);

    // === Delegates ===
    UPROPERTY(BlueprintAssignable, Category="PCG|Ritual")
    FOnStructureRestored OnStructureRestored;

    // === Test seam (unconditional inline; unused in shipping → linker emits nothing) ===
    /** Test-only seam: install a known synthetic point-state set, bypassing PCG init. */
    void Test_SeedPointStates(const TArray<FRitualPointState>& InStates) { PointStates = InStates; }
    /** Test-only seam: read current point state for assertions. */
    const TArray<FRitualPointState>& Test_PeekPointStates() const { return PointStates; }

private:
    ERitualType GetRitualTypeFromPoint(const FPCGPoint& Point) const;

    // Spatial hash helpers
    void BuildSpatialGrid();
    FIntVector WorldToCell(const FVector& Location) const;

    // Explicit expensive sync to PCG metadata (use sparingly)
    void SyncPointToMetadata(int32 PointIndex);

    UPROPERTY()
    UPCGPointData* MutablePointData = nullptr;

    int32 CurrentWorldSeed = 0;
    TSet<int32> RestoredPointIndices;

    // Fast parallel state (primary source of truth for mutable data)
    TArray<FRitualPointState> PointStates;

    // Spatial acceleration
    TMap<FIntVector, FRitualSpatialCell> SpatialGrid;
    float CellSize = 400.0f;
    FBox WorldBounds;

    // Cached for performance
    TArray<FPCGPoint> CachedPoints;

    bool bDrawSpatialGridDebug = false;
};