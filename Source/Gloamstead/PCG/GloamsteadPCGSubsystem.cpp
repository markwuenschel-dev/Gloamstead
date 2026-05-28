#include "PCG/GloamsteadPCGSubsystem.h"
#include "PCGComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

void UGloamsteadPCGSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UGloamsteadPCGSubsystem::Deinitialize()
{
    MutablePointData = nullptr;
    CachedPoints.Empty();
    PointStates.Empty();
    SpatialGrid.Empty();
    RestoredPointIndices.Empty();
    Super::Deinitialize();
}

void UGloamsteadPCGSubsystem::InitializeFromPCGComponent(UPCGComponent* PCGComponent, int32 WorldSeed)
{
    if (!PCGComponent) return;

    UPCGData* GeneratedData = PCGComponent->GetGeneratedPCGData();
    UPCGPointData* SourcePointData = Cast<UPCGPointData>(GeneratedData);
    if (!SourcePointData) return;

    MutablePointData = DuplicateObject<UPCGPointData>(SourcePointData, this);
    CachedPoints = MutablePointData->GetPoints();

    // Build fast parallel state
    PointStates.SetNum(CachedPoints.Num());
    for (int32 i = 0; i < CachedPoints.Num(); ++i)
    {
        const FPCGPoint& Point = CachedPoints[i];
        PointStates[i].bIsRestored     = GetBoolAttribute(Point, "bIsRestored", false);
        PointStates[i].LightLevel      = GetFloatAttribute(Point, "LightLevel", 0.0f);
        PointStates[i].CorruptionLevel = GetFloatAttribute(Point, "CorruptionLevel", 0.0f);
    }

    CurrentWorldSeed = WorldSeed;
    RestoredPointIndices.Empty();

    BuildSpatialGrid();

    UE_LOG(LogTemp, Log, TEXT("UGloamsteadPCGSubsystem: Initialized with %d points (Hybrid State + Spatial Grid)"), CachedPoints.Num());
}

bool UGloamsteadPCGSubsystem::GetPointByIndex(int32 PointIndex, FPCGPoint& OutPoint) const
{
    if (!CachedPoints.IsValidIndex(PointIndex)) return false;
    OutPoint = CachedPoints[PointIndex];
    return true;
}

TArray<FPCGPoint> UGloamsteadPCGSubsystem::GetPointsByRitualType(ERitualType Type, bool bOnlyRestored) const
{
    TArray<FPCGPoint> Result;
    for (const FPCGPoint& Point : CachedPoints)
    {
        if (GetRitualTypeFromPoint(Point) != Type) continue;
        if (bOnlyRestored && !GetBoolAttribute(Point, "bIsRestored", false)) continue;
        Result.Add(Point);
    }
    return Result;
}

TArray<FPCGPoint> UGloamsteadPCGSubsystem::GetPointsAlongPath(int32 PathSegmentID) const
{
    TArray<FPCGPoint> Result;
    if (PathSegmentID < 0) return Result;

    for (const FPCGPoint& Point : CachedPoints)
    {
        if (GetIntAttribute(Point, "PathSegmentID", -1) == PathSegmentID)
            Result.Add(Point);
    }
    Result.Sort([](const FPCGPoint& A, const FPCGPoint& B)
    {
        return GetFloatAttributeStatic(A, "PathPosition", 0.0f) < GetFloatAttributeStatic(B, "PathPosition", 0.0f);
    });
    return Result;
}

int32 UGloamsteadPCGSubsystem::FindNearestUnrestoredPointIndex(const FVector& Location, ERitualType Type, float SearchRadius) const
{
    int32 BestIndex = -1;
    float BestDistSq = SearchRadius * SearchRadius;

    const FIntVector CenterCell = WorldToCell(Location);
    const int32 CellRadius = FMath::CeilToInt(SearchRadius / CellSize) + 1;

    for (int32 x = -CellRadius; x <= CellRadius; ++x)
    {
        for (int32 y = -CellRadius; y <= CellRadius; ++y)
        {
            for (int32 z = -CellRadius; z <= CellRadius; ++z)
            {
                const FIntVector Cell = CenterCell + FIntVector(x, y, z);
                if (const FRitualSpatialCell* CellData = SpatialGrid.Find(Cell))
                {
                    for (int32 PointIndex : CellData->PointIndices)
                    {
                        if (IsPointRestored(PointIndex)) continue;
                        if (GetRitualTypeFromPoint(CachedPoints[PointIndex]) != Type) continue;

                        const float DistSq = FVector::DistSquared(Location, CachedPoints[PointIndex].Transform.GetLocation());
                        if (DistSq < BestDistSq)
                        {
                            BestDistSq = DistSq;
                            BestIndex = PointIndex;
                        }
                    }
                }
            }
        }
    }
    return BestIndex;
}

bool UGloamsteadPCGSubsystem::IsPointRestored(int32 PointIndex) const
{
    return PointStates.IsValidIndex(PointIndex) && PointStates[PointIndex].bIsRestored;
}

float UGloamsteadPCGSubsystem::GetLightLevel(int32 PointIndex) const
{
    return PointStates.IsValidIndex(PointIndex) ? PointStates[PointIndex].LightLevel : 0.0f;
}

float UGloamsteadPCGSubsystem::GetCorruptionLevel(int32 PointIndex) const
{
    return PointStates.IsValidIndex(PointIndex) ? PointStates[PointIndex].CorruptionLevel : 0.0f;
}

bool UGloamsteadPCGSubsystem::ApplyRestoration(int32 PointIndex, const FRestorationEventPayload& Payload)
{
    if (!PointStates.IsValidIndex(PointIndex)) return false;

    // Fast path using parallel state
    FRitualPointState& State = PointStates[PointIndex];
    State.bIsRestored = true;
    State.LightLevel += Payload.LightDelta;
    State.CorruptionLevel = FMath::Max(0.0f, State.CorruptionLevel - Payload.CorruptionCleared);

    RestoredPointIndices.Add(PointIndex);

    // NOTE: We intentionally do NOT write to PCG metadata here for performance.
    // Call SyncPointToMetadata() explicitly only when needed (debug, save, VFX binding).

    OnStructureRestored.Broadcast(Payload);
    return true;
}

TSet<int32> UGloamsteadPCGSubsystem::GetRestoredPointIndices() const
{
    return RestoredPointIndices;
}

void UGloamsteadPCGSubsystem::ReapplyRestoredState(const TSet<int32>& RestoredIndices)
{
    for (int32 Index : RestoredIndices)
    {
        if (PointStates.IsValidIndex(Index))
        {
            PointStates[Index].bIsRestored = true;
        }
    }
    RestoredPointIndices = RestoredIndices;
}

void UGloamsteadPCGSubsystem::DrawDebugRitualPoints(float Duration) const
{
    UWorld* World = GetWorld();
    if (!World) return;

    for (int32 i = 0; i < CachedPoints.Num(); ++i)
    {
        const FVector Location = CachedPoints[i].Transform.GetLocation();
        const ERitualType Type = GetRitualTypeFromPoint(CachedPoints[i]);
        const bool bRestored = IsPointRestored(i);

        FColor Color = FColor::White;
        float Size = 40.0f;

        switch (Type)
        {
            case ERitualType::LanternPost: Color = bRestored ? FColor::Green : FColor::Cyan; break;
            case ERitualType::GardenBed:   Color = bRestored ? FColor::Green : FColor::Emerald; break;
            case ERitualType::PathPoint:   Color = bRestored ? FColor(80,80,80) : FColor::Yellow; break;
            default: Color = FColor::Red; break;
        }

        if (bRestored) Size = 55.0f;

        DrawDebugSphere(World, Location, Size, 12, Color, false, Duration, 0, 2.0f);
    }
}

void UGloamsteadPCGSubsystem::DrawDebugSpatialGrid(float Duration) const
{
    UWorld* World = GetWorld();
    if (!World || SpatialGrid.Num() == 0) return;

    const float DrawDuration = (Duration > 0.0f) ? Duration : 0.0f;

    for (const auto& GridPair : SpatialGrid)
    {
        const FIntVector& CellCoord = GridPair.Key;
        const FRitualSpatialCell& Cell = GridPair.Value;
        const int32 PointCount = Cell.PointIndices.Num();

        const FVector CellMin(CellCoord.X * CellSize, CellCoord.Y * CellSize, CellCoord.Z * CellSize);
        const FVector CellMax = CellMin + FVector(CellSize);
        const FVector CellCenter = (CellMin + CellMax) * 0.5f;
        const FVector CellExtent = FVector(CellSize * 0.5f);

        FColor CellColor;
        if (PointCount == 0)      CellColor = FColor::Black;
        else if (PointCount == 1) CellColor = FColor::Green;
        else if (PointCount <= 3) CellColor = FColor::Yellow;
        else if (PointCount <= 6) CellColor = FColor::Orange;
        else                      CellColor = FColor::Red;

        DrawDebugBox(World, CellCenter, CellExtent, FQuat::Identity, CellColor, false, DrawDuration, 0, 2.0f);

        const FString Label = FString::Printf(TEXT("%d"), PointCount);
        DrawDebugString(World, CellCenter + FVector(0, 0, CellExtent.Z + 20.0f), Label, nullptr, CellColor, DrawDuration, true, 1.2f);

        if (PointCount >= 5)
        {
            DrawDebugSphere(World, CellCenter, 25.0f, 8, CellColor, false, DrawDuration, 0, 2.0f);
        }
    }
}

void UGloamsteadPCGSubsystem::SetDrawSpatialGridDebug(bool bEnabled)
{
    bDrawSpatialGridDebug = bEnabled;
}

void UGloamsteadPCGSubsystem::BuildSpatialGrid()
{
    SpatialGrid.Empty();
    if (CachedPoints.Num() == 0) return;

    FVector Min(FLT_MAX), Max(-FLT_MAX);
    for (const FPCGPoint& Point : CachedPoints)
    {
        const FVector Loc = Point.Transform.GetLocation();
        Min = Min.ComponentMin(Loc);
        Max = Max.ComponentMax(Loc);
    }
    WorldBounds = FBox(Min, Max);

    for (int32 i = 0; i < CachedPoints.Num(); ++i)
    {
        const FIntVector Cell = WorldToCell(CachedPoints[i].Transform.GetLocation());
        SpatialGrid.FindOrAdd(Cell).PointIndices.Add(i);
    }
}

FIntVector UGloamsteadPCGSubsystem::WorldToCell(const FVector& Location) const
{
    return FIntVector(
        FMath::FloorToInt(Location.X / CellSize),
        FMath::FloorToInt(Location.Y / CellSize),
        FMath::FloorToInt(Location.Z / CellSize)
    );
}

void UGloamsteadPCGSubsystem::SyncPointToMetadata(int32 PointIndex)
{
    if (!CachedPoints.IsValidIndex(PointIndex) || !MutablePointData) return;

    const FRitualPointState& State = PointStates[PointIndex];
    FPCGPoint& Point = CachedPoints[PointIndex];

    Point.SetMetadataEntry("bIsRestored", State.bIsRestored);
    Point.SetMetadataEntry("LightLevel", State.LightLevel);
    Point.SetMetadataEntry("CorruptionLevel", State.CorruptionLevel);

    MutablePointData->SetPoints(CachedPoints);
}

// ==================== Private Attribute Helpers ====================

bool UGloamsteadPCGSubsystem::GetBoolAttribute(const FPCGPoint& Point, FName AttributeName, bool DefaultValue) const
{
    return Point.GetMetadataEntry<bool>(AttributeName, DefaultValue);
}

float UGloamsteadPCGSubsystem::GetFloatAttribute(const FPCGPoint& Point, FName AttributeName, float DefaultValue) const
{
    return Point.GetMetadataEntry<float>(AttributeName, DefaultValue);
}

int32 UGloamsteadPCGSubsystem::GetIntAttribute(const FPCGPoint& Point, FName AttributeName, int32 DefaultValue) const
{
    return Point.GetMetadataEntry<int32>(AttributeName, DefaultValue);
}

ERitualType UGloamsteadPCGSubsystem::GetRitualTypeFromPoint(const FPCGPoint& Point) const
{
    return static_cast<ERitualType>(GetIntAttribute(Point, "RitualType", 0));
}

float UGloamsteadPCGSubsystem::GetFloatAttributeStatic(const FPCGPoint& Point, FName AttributeName, float DefaultValue)
{
    return Point.GetMetadataEntry<float>(AttributeName, DefaultValue);
}