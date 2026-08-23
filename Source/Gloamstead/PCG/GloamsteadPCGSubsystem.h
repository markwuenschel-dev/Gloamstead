#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/PCGPointData.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "GloamsteadPCGSubsystem.generated.h"

class UGloamsteadSaveGame;
struct FExperienceCyclePlan;

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

    /**
     * Checks the immutable authored metadata carried by one PCG point. This is
     * the runtime authority for Cycle II target/receipt evaluation; callers
     * must not trust matching literals supplied in a restoration payload.
     */
    bool PointMatchesExperiencePlan(int32 PointIndex, const FExperienceCyclePlan& Plan, bool bRequireRestored = false) const;

    /** Fills only contract metadata from the PCG point, never caller-provided literals. */
    bool PopulateAuthoritativeRestorationMetadata(int32 PointIndex, FRestorationEventPayload& InOutPayload) const;

    UFUNCTION(BlueprintPure, Category="PCG|Ritual")
    float GetCorruptionLevel(int32 PointIndex) const;

    // === Sanctuary aggregates (read-only; safe defaults when uninitialized) ===
    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    float GetSanctuaryAverageLightLevel() const;

    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    float GetSanctuaryAverageCorruptionLevel() const;

    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    int32 GetRestoredPointCount() const;

    /** Read-only: total number of initialized ritual points (for consumers that render/iterate points). */
    UFUNCTION(BlueprintPure, Category="PCG|Ritual")
    int32 GetRitualPointCount() const { return CachedPoints.Num(); }

    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    int32 GetRestoredCountByRitualType(ERitualType Type) const;

    UFUNCTION(BlueprintPure, Category="PCG|Sanctuary")
    FNightSanctuarySnapshot BuildSanctuarySnapshot() const;

    // === State Mutation (optimized hot path) ===
    /** Mend a ritual point and broadcast OnStructureRestored. Returns false — mutating and broadcasting
     *  nothing — when PointIndex is out of range, when Payload.PointIndex disagrees with PointIndex
     *  (listeners index off the payload, so the two must be the same point), or when the point is
     *  already restored. The last two checks are enforced here rather than by the caller because this
     *  is BlueprintCallable and Blueprint can reach it without passing through placement. */
    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    bool ApplyRestoration(int32 PointIndex, const FRestorationEventPayload& Payload);

    /** Night-only corruption spread; does not alter restoration flags. MaxPoints clamped internally (default cap 32). */
    UFUNCTION(BlueprintCallable, Category="PCG|Night")
    int32 ApplyCorruptionSpread(float Delta, int32 MaxPoints = 8);

    /** Night-only: raise (or, with negative Delta, lower) corruption on a single point (the "bloom"); clamped [0,1].
     *  Does not alter restoration flags. Returns the new corruption level, or -1 if the index is invalid. */
    UFUNCTION(BlueprintCallable, Category="PCG|Night")
    float AddCorruptionAtIndex(int32 PointIndex, float Delta);

    /** Read-only: index of the most-corrupted point (optionally restricted to unrestored points). -1 if none. */
    UFUNCTION(BlueprintPure, Category="PCG|Night")
    int32 FindMostCorruptedPointIndex(bool bOnlyUnrestored = false) const;

    /** Read-only: pick a restored point for a night to target (Retrieval). When bMostLit, returns the
     *  brightest restored point (the one most worth reclaiming); otherwise the lowest restored index.
     *  Returns -1 if nothing is restored — the caller's honest no-target fallback. */
    UFUNCTION(BlueprintPure, Category="PCG|Night")
    int32 FindRestoredPointIndex(bool bMostLit = true) const;

    /** Night-only: reclaim a restored point (Retrieval failure). Clears the restored flag, drops its light
     *  (the night takes back what it gave), and removes it from the restored set. Corruption is left to the
     *  caller (pressure already scarred it). Returns true if a restored point was actually reclaimed. */
    UFUNCTION(BlueprintCallable, Category="PCG|Night")
    bool RevertRestoration(int32 PointIndex);

    // === Persistence (Vertical Slice Strategy) ===
    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    TSet<int32> GetRestoredPointIndices() const;

    UFUNCTION(BlueprintCallable, Category="PCG|Ritual")
    void ReapplyRestoredState(const TSet<int32>& RestoredIndices);

    // === Full persistence (per-point state, not just restored flags) ===
    /** Copy full per-point state (light/corruption/restored) + restored set + seed into a save object. */
    UFUNCTION(BlueprintCallable, Category="PCG|Persistence")
    void CaptureToSaveGame(UGloamsteadSaveGame* SaveGame) const;

    /** Migrate then replace current per-point state with the save object's contents. Returns false for an unsupported save version. */
    UFUNCTION(BlueprintCallable, Category="PCG|Persistence")
    bool RestoreFromSaveGame(UGloamsteadSaveGame* SaveGame);

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
    /** Test-only seam: install synthetic LanternPost points with metadata and rebuild the spatial grid. */
    void Test_SeedPoints(const TArray<FVector>& Locations);
    /** Test-only metadata injection for an existing synthetic point. */
    bool Test_SetPointContractMetadata(
        int32 PointIndex,
        FName WarningId,
        FName SemanticSubject,
        ERitualType RitualType,
        FName RestorationTag);
    /** Test-only seam: read current point state for assertions. */
    const TArray<FRitualPointState>& Test_PeekPointStates() const { return PointStates; }

public:
    /**
     * Actor tag marking the authored first-lantern site. A level that places an actor with this tag
     * declares "the first lantern belongs HERE", and the procedurally-placed LanternPost point is
     * re-seated onto it at init. Same tag the first-night director uses to find its marker actor.
     */
    static const FName FirstLanternAnchorTag;

private:
    ERitualType GetRitualTypeFromPoint(const FPCGPoint& Point) const;

    /**
     * Moves the LanternPost point nearest the authored anchor onto that anchor's transform.
     *
     * The PCG graph scatters ritual points around the sanctuary bootstrap, which sits at the origin —
     * so the restorable lantern point landed on top of the Veil Heart while the authored broken-lantern
     * dressing stood ~1300uu away. The player could see a ruin they could not restore and restore a
     * point they could not see. This reconciles the two without duplicating PCG's authority: PCG still
     * owns the point set, its state, and its metadata; the level only gets to say where the FIRST
     * lantern is. Maps with no tagged anchor are untouched.
     *
     * Mutates Transform in place so FPCGPoint::MetadataEntry survives — rebuilding the point would
     * silently reset RitualType and RestorationRadius to their defaults.
     */
    void ApplyAuthoredAnchorOverride();

    // Spatial hash helpers
    void BuildSpatialGrid();
    FIntVector WorldToCell(const FVector& Location) const;

    // Explicit expensive sync to PCG metadata (use sparingly)
    void SyncPointToMetadata(int32 PointIndex);

    /** Rebuild RestoredPointIndices so it agrees exactly with the bIsRestored flags in PointStates.
     *  Sole owner of that invariant: PointStates is the source of truth and the index set is derived
     *  from it. Any path that replaces PointStates wholesale, or that is handed a restored set from
     *  outside, must end here rather than assigning RestoredPointIndices directly — a direct assign
     *  is how an index with no point behind it gets into the set, and a second copy of this loop is
     *  how the two views drift apart again. */
    void RebuildRestoredIndicesFromPointStates();

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
