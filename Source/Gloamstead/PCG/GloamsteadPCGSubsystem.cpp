#include "PCG/GloamsteadPCGSubsystem.h"
#include "Components/GloamsteadRitualSiteComponent.h"
#include "Data/GloamsteadRitualSiteCatalog.h"
#include "Systems/VeilHeart.h"
#include "EngineUtils.h"
#include "Data/ExperienceCycleTypes.h"
#include "PCGComponent.h"
#include "PCGData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGMetadata.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Save/GloamsteadSaveGame.h"
#include "Kismet/GameplayStatics.h"

const FString UGloamsteadPCGSubsystem::DefaultSaveSlot = TEXT("GloamsteadSanctuary");
const FName UGloamsteadPCGSubsystem::FirstLanternAnchorTag(TEXT("Gloamstead.FirstLantern.Anchor"));

void UGloamsteadPCGSubsystem::ApplyAuthoredAnchorOverride()
{
    UWorld* World = GetWorld();
    if (!World || CachedPoints.Num() == 0)
    {
        return;
    }

    TArray<AActor*> Anchors;
    UGameplayStatics::GetAllActorsWithTag(World, FirstLanternAnchorTag, Anchors);
    if (Anchors.Num() == 0)
    {
        return; // No authored site in this map: leave the procedural placement exactly as generated.
    }

    const AActor* Anchor = Anchors[0];
    const FVector AnchorLocation = Anchor->GetActorLocation();

    // Nearest LanternPost to the anchor, so a map with several lanterns re-seats the intended one
    // rather than whichever happens to be first in the array.
    int32 BestIndex = INDEX_NONE;
    double BestDistSq = TNumericLimits<double>::Max();
    for (int32 i = 0; i < CachedPoints.Num(); ++i)
    {
        if (GetRitualTypeFromPoint(CachedPoints[i]) != ERitualType::LanternPost)
        {
            continue;
        }
        const double DistSq = FVector::DistSquared(CachedPoints[i].Transform.GetLocation(), AnchorLocation);
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestIndex = i;
        }
    }

    if (BestIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UGloamsteadPCGSubsystem: authored first-lantern anchor '%s' found, but the graph produced no LanternPost point to seat on it."),
            *Anchor->GetName());
        return;
    }

    const FVector OldLocation = CachedPoints[BestIndex].Transform.GetLocation();
    if (OldLocation.Equals(AnchorLocation, 1.0))
    {
        AnchorSeatedPointIndex = BestIndex; // Already there, but still the lantern's point.
        return;
    }

    AnchorSeatedPointIndex = BestIndex;

    // In-place: keep MetadataEntry (and therefore RitualType / RestorationRadius) intact.
    CachedPoints[BestIndex].Transform.SetLocation(AnchorLocation);
    CachedPoints[BestIndex].Transform.SetRotation(Anchor->GetActorQuat());

    UE_LOG(LogTemp, Log,
        TEXT("UGloamsteadPCGSubsystem: re-seated first lantern (point %d) from %s onto authored anchor '%s' at %s."),
        BestIndex, *OldLocation.ToCompactString(), *Anchor->GetName(), *AnchorLocation.ToCompactString());
}

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

    // Resolve any point representation to legacy UPCGPointData.
    // UE 5.5+ emits UPCGPointArrayData (SoA) by default; it is a sibling of UPCGPointData
    // under UPCGBasePointData, so a direct Cast<UPCGPointData> fails. Convert via the spatial
    // interface so the rest of the subsystem (FPCGPoint / GetPoints / MetadataEntry) is unchanged.
    auto AsLegacyPointData = [](const UPCGData* Data) -> const UPCGPointData*
    {
        if (const UPCGPointData* PD = Cast<const UPCGPointData>(Data))
        {
            return PD; // already legacy
        }
        if (const UPCGSpatialData* Spatial = Cast<const UPCGSpatialData>(Data))
        {
            return Spatial->ToPointData(nullptr); // UPCGPointArrayData (or other spatial) -> legacy
        }
        return nullptr;
    };

    const UPCGPointData* SourcePointData = nullptr;

    // Preferred: post-generation output (contains the final points + metadata authored by the PCG graph)
    const FPCGDataCollection& GeneratedOutput = PCGComponent->GetGeneratedGraphOutput();
    for (const FPCGTaggedData& Tagged : GeneratedOutput.TaggedData)
    {
        if (const UPCGPointData* PD = AsLegacyPointData(Tagged.Data))
        {
            SourcePointData = PD;
            break;
        }
    }

    // Fallback for some generation modes
    if (!SourcePointData)
    {
        SourcePointData = AsLegacyPointData(PCGComponent->GetPCGData());
    }

    if (!SourcePointData)
    {
        UE_LOG(LogTemp, Warning, TEXT("UGloamsteadPCGSubsystem: PCG component produced no point data (output empty or not yet generated)."));
        return;
    }

    // The generated output's metadata is typically parented to upstream graph data. A plain
    // UObject duplicate can't preserve that parent link, leaving attributes with dangling parent
    // ids that crash on access (ensure in FPCGMetadataAttributeBase, then null-deref). Flatten the
    // (freshly converted) source so its metadata is self-contained before taking an owned copy.
    if (UPCGPointData* MutableSource = const_cast<UPCGPointData*>(SourcePointData))
    {
        if (MutableSource->Metadata)
        {
            MutableSource->Metadata->Flatten();
        }
    }

    MutablePointData = DuplicateObject<UPCGPointData>(SourcePointData, this);
    if (MutablePointData && MutablePointData->Metadata)
    {
        MutablePointData->Metadata->Flatten(); // ensure the owned copy is fully self-contained
    }

    // The graph supplies geometry and RitualType; the semantic contract is ours to carry. Create any
    // missing attribute here so an authored site can be stamped onto real generated points at all.
    EnsureContractMetadataAttributes();

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

    // Derive the restored set from the flags we just read, rather than merely clearing it.
    // IsPointRestored() answers from PointStates while GetRestoredPointCount() /
    // GetRestoredCountByRitualType() / CaptureToSaveGame() answer from this set; a bare Empty()
    // leaves a graph that ships pre-restored points reporting a restored count of zero.
    RebuildRestoredIndicesFromPointStates();

    // Re-seat the first lantern onto the level's authored anchor BEFORE the grid is built, so the grid
    // is correct on its first and only construction rather than being invalidated a moment later.
    ApplyAuthoredAnchorOverride();

    // Give every authored ritual site its semantic contract BEFORE the grid is built, so semantically
    // targeted nights can resolve a point in a shipping build rather than only under automation.
    ApplyAuthoredSiteContracts();

    BuildSpatialGrid();

    UE_LOG(LogTemp, Log, TEXT("UGloamsteadPCGSubsystem: Initialized with %d points (Hybrid State + Spatial Grid)"), CachedPoints.Num());
    NotifyAuthoritativeStateRebuilt();
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
    Result.Sort([this](const FPCGPoint& A, const FPCGPoint& B)
    {
        return GetFloatAttribute(A, "PathPosition", 0.0f) < GetFloatAttribute(B, "PathPosition", 0.0f);
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

int32 UGloamsteadPCGSubsystem::FindNearestUnrestoredPointMatchingExperiencePlan(
    const FVector& Location, const FExperienceCyclePlan& Plan, float SearchRadius) const
{
    int32 BestIndex = INDEX_NONE;
    float BestDistSq = SearchRadius * SearchRadius;

    const FIntVector CenterCell = WorldToCell(Location);
    const int32 CellRadius = FMath::CeilToInt(SearchRadius / CellSize) + 1;

    for (int32 X = -CellRadius; X <= CellRadius; ++X)
    {
        for (int32 Y = -CellRadius; Y <= CellRadius; ++Y)
        {
            for (int32 Z = -CellRadius; Z <= CellRadius; ++Z)
            {
                const FIntVector Cell = CenterCell + FIntVector(X, Y, Z);
                const FRitualSpatialCell* CellData = SpatialGrid.Find(Cell);
                if (!CellData)
                {
                    continue;
                }

                for (const int32 PointIndex : CellData->PointIndices)
                {
                    if (IsPointRestored(PointIndex)
                        || !PointMatchesExperiencePlan(PointIndex, Plan))
                    {
                        continue;
                    }

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

float UGloamsteadPCGSubsystem::GetSanctuaryAverageLightLevel() const
{
    if (PointStates.Num() == 0)
    {
        return 0.0f;
    }

    float Sum = 0.0f;
    for (const FRitualPointState& State : PointStates)
    {
        Sum += State.LightLevel;
    }
    return Sum / static_cast<float>(PointStates.Num());
}

float UGloamsteadPCGSubsystem::GetSanctuaryAverageCorruptionLevel() const
{
    if (PointStates.Num() == 0)
    {
        return 0.0f;
    }

    float Sum = 0.0f;
    for (const FRitualPointState& State : PointStates)
    {
        Sum += State.CorruptionLevel;
    }
    return Sum / static_cast<float>(PointStates.Num());
}

int32 UGloamsteadPCGSubsystem::GetRestoredPointCount() const
{
    return RestoredPointIndices.Num();
}

int32 UGloamsteadPCGSubsystem::GetRestoredCountByRitualType(ERitualType Type) const
{
    if (Type == ERitualType::Invalid)
    {
        return 0;
    }

    int32 Count = 0;
    for (int32 PointIndex : RestoredPointIndices)
    {
        if (!CachedPoints.IsValidIndex(PointIndex))
        {
            continue;
        }
        if (GetRitualTypeFromPoint(CachedPoints[PointIndex]) == Type)
        {
            ++Count;
        }
    }
    return Count;
}

FNightSanctuarySnapshot UGloamsteadPCGSubsystem::BuildSanctuarySnapshot() const
{
    FNightSanctuarySnapshot Snapshot;
    Snapshot.AverageLightLevel = GetSanctuaryAverageLightLevel();
    Snapshot.AverageCorruptionLevel = GetSanctuaryAverageCorruptionLevel();
    Snapshot.RestoredPointCount = GetRestoredPointCount();
    Snapshot.LanternPostRestored = GetRestoredCountByRitualType(ERitualType::LanternPost);
    Snapshot.GardenBedRestored = GetRestoredCountByRitualType(ERitualType::GardenBed);
    Snapshot.PathPointRestored = GetRestoredCountByRitualType(ERitualType::PathPoint);
    Snapshot.MirrorPillarRestored = GetRestoredCountByRitualType(ERitualType::MirrorPillar);
    Snapshot.BellShrineRestored = GetRestoredCountByRitualType(ERitualType::BellShrine);
    Snapshot.AnchorStoneRestored = GetRestoredCountByRitualType(ERitualType::AnchorStone);
    return Snapshot;
}

FDelegateHandle UGloamsteadPCGSubsystem::AddAuthoritativeStateRebuiltListener(const FSimpleDelegate& Listener)
{
    return AuthoritativeStateRebuilt.Add(Listener);
}

void UGloamsteadPCGSubsystem::RemoveAuthoritativeStateRebuiltListener(FDelegateHandle ListenerHandle)
{
    if (ListenerHandle.IsValid())
    {
        AuthoritativeStateRebuilt.Remove(ListenerHandle);
    }
}

bool UGloamsteadPCGSubsystem::ApplyRestoration(int32 PointIndex, const FRestorationEventPayload& Payload)
{
    if (!PointStates.IsValidIndex(PointIndex)) return false;

    // The payload — not the argument — is what travels to every listener below, and they index off
    // Payload.PointIndex (Veil Heart / night strategies / mesh forge adapter). A payload built for a
    // different point, or never assigned one at all (the -1 default), would mutate here and report
    // somewhere else. Reject rather than reconcile: a mismatch means the caller is confused.
    if (Payload.PointIndex != PointIndex)
    {
        UE_LOG(LogTemp, Error, TEXT("PCG: ApplyRestoration rejected - payload index %d does not match target index %d."),
            Payload.PointIndex, PointIndex);
        return false;
    }

    // Fast path using parallel state
    FRitualPointState& State = PointStates[PointIndex];

    // Restoration is once per point. This guard lives here and not only in ConfirmPlacement because
    // ApplyRestoration is BlueprintCallable: a Blueprint calling it directly would otherwise stack
    // LightDelta and clear corruption again on a point that is already mended. Reclaimed points
    // (see RevertRestoration) clear the flag and are restorable again, as intended.
    if (State.bIsRestored)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCG: ApplyRestoration rejected - point %d is already restored."), PointIndex);
        return false;
    }

    State.bIsRestored = true;
    State.LightLevel += Payload.LightDelta;
    State.CorruptionLevel = FMath::Max(0.0f, State.CorruptionLevel - Payload.CorruptionCleared);

    RestoredPointIndices.Add(PointIndex);

    // NOTE: We intentionally do NOT write to PCG metadata here for performance.
    // Call SyncPointToMetadata() explicitly only when needed (debug, save, VFX binding).

    OnStructureRestored.Broadcast(Payload);
    return true;
}

int32 UGloamsteadPCGSubsystem::ApplyCorruptionSpread(float Delta, int32 MaxPoints)
{
    if (PointStates.Num() == 0 || Delta <= 0.f)
    {
        return 0;
    }

    constexpr int32 HardCap = 32;
    const int32 NumToMutate = FMath::Clamp(MaxPoints, 1, FMath::Min(HardCap, PointStates.Num()));

    TArray<int32> Candidates;
    Candidates.Reserve(PointStates.Num());
    for (int32 Index = 0; Index < PointStates.Num(); ++Index)
    {
        Candidates.Add(Index);
    }

    for (int32 i = Candidates.Num() - 1; i > 0; --i)
    {
        const int32 j = FMath::RandRange(0, i);
        Candidates.Swap(i, j);
    }

    int32 Mutated = 0;
    for (int32 i = 0; i < NumToMutate; ++i)
    {
        const int32 PointIndex = Candidates[i];
        FRitualPointState& State = PointStates[PointIndex];
        State.CorruptionLevel = FMath::Clamp(State.CorruptionLevel + Delta, 0.f, 1.f);
        ++Mutated;
    }

    UE_LOG(LogTemp, Log, TEXT("PCG: ApplyCorruptionSpread delta=%.2f mutated=%d avg corruption now=%.2f"),
        Delta, Mutated, GetSanctuaryAverageCorruptionLevel());

    return Mutated;
}

float UGloamsteadPCGSubsystem::AddCorruptionAtIndex(int32 PointIndex, float Delta)
{
    if (!PointStates.IsValidIndex(PointIndex))
    {
        return -1.f;
    }
    FRitualPointState& State = PointStates[PointIndex];
    State.CorruptionLevel = FMath::Clamp(State.CorruptionLevel + Delta, 0.f, 1.f);
    return State.CorruptionLevel;
}

int32 UGloamsteadPCGSubsystem::FindMostCorruptedPointIndex(bool bOnlyUnrestored) const
{
    int32 BestIndex = -1;
    float BestCorruption = -1.f;
    for (int32 Index = 0; Index < PointStates.Num(); ++Index)
    {
        const FRitualPointState& State = PointStates[Index];
        if (bOnlyUnrestored && State.bIsRestored)
        {
            continue;
        }
        if (State.CorruptionLevel > BestCorruption)
        {
            BestCorruption = State.CorruptionLevel;
            BestIndex = Index;
        }
    }
    return BestIndex;
}

#if WITH_DEV_AUTOMATION_TESTS
void UGloamsteadPCGSubsystem::Test_SeedPoints(const TArray<FVector>& Locations)
{
	Test_SeedPoints(Locations, {}, {});
}

void UGloamsteadPCGSubsystem::Test_SeedPoints(
	const TArray<FVector>& Locations,
	const TArray<float>& Wetness,
	const TArray<FName>& RecommendedWarningTags)
{
    MutablePointData = NewObject<UPCGPointData>(this);
    check(MutablePointData && MutablePointData->Metadata);

    FPCGMetadataAttribute<int32>* RitualTypeAttribute =
        MutablePointData->Metadata->CreateAttribute<int32>(
            TEXT("RitualType"), static_cast<int32>(ERitualType::LanternPost),
            /*bAllowsInterpolation*/ false, /*bOverrideParent*/ false);
    FPCGMetadataAttribute<FName>* WarningAttribute = MutablePointData->Metadata->CreateAttribute<FName>(
        TEXT("RecommendedForWarning"), NAME_None,
        /*bAllowsInterpolation*/ false, /*bOverrideParent*/ false);
    MutablePointData->Metadata->CreateAttribute<FName>(
        TEXT("SemanticSubject"), NAME_None,
        /*bAllowsInterpolation*/ false, /*bOverrideParent*/ false);
    MutablePointData->Metadata->CreateAttribute<FName>(
        TEXT("RestorationTag"), NAME_None,
        /*bAllowsInterpolation*/ false, /*bOverrideParent*/ false);
    FPCGMetadataAttribute<float>* WetnessAttribute = MutablePointData->Metadata->CreateAttribute<float>(
        TEXT("Wetness"), 0.f, /*bAllowsInterpolation*/ true, /*bOverrideParent*/ false);

    TArray<FPCGPoint>& Points = MutablePointData->GetMutablePoints();
    Points.Reset(Locations.Num());
    for (int32 Index = 0; Index < Locations.Num(); ++Index)
    {
        FPCGPoint P;
        P.Transform = FTransform(Locations[Index]);
        P.MetadataEntry = MutablePointData->Metadata->AddEntry();
        RitualTypeAttribute->SetValue(P.MetadataEntry, static_cast<int32>(ERitualType::LanternPost));
        WarningAttribute->SetValue(P.MetadataEntry,
            RecommendedWarningTags.IsValidIndex(Index) ? RecommendedWarningTags[Index] : NAME_None);
        WetnessAttribute->SetValue(P.MetadataEntry, Wetness.IsValidIndex(Index) ? Wetness[Index] : 0.f);
        Points.Add(P);
    }
    CachedPoints = Points;
    BuildSpatialGrid();
}

bool UGloamsteadPCGSubsystem::Test_SetPointContractMetadata(
    int32 PointIndex,
    FName WarningId,
    FName SemanticSubject,
    ERitualType RitualType,
    FName RestorationTag)
{
    // Delegates to the production writer so the test seam and the shipping path cannot diverge: a fixture
    // that passes here is exercising exactly the code an authored ritual site runs.
    return WritePointContractMetadata(PointIndex, WarningId, SemanticSubject, RitualType, RestorationTag);
}
#endif // WITH_DEV_AUTOMATION_TESTS

void UGloamsteadPCGSubsystem::EnsureContractMetadataAttributes()
{
    if (!MutablePointData || !MutablePointData->Metadata)
    {
        return;
    }

    UPCGMetadata* Metadata = MutablePointData->Metadata;

    if (!Metadata->GetMutableTypedAttribute<int32>(TEXT("RitualType")))
    {
        Metadata->CreateAttribute<int32>(TEXT("RitualType"), static_cast<int32>(ERitualType::LanternPost),
            /*bAllowsInterpolation*/ false, /*bOverrideParent*/ false);
    }
    if (!Metadata->GetMutableTypedAttribute<FName>(TEXT("RecommendedForWarning")))
    {
        Metadata->CreateAttribute<FName>(TEXT("RecommendedForWarning"), NAME_None, false, false);
    }
    if (!Metadata->GetMutableTypedAttribute<FName>(TEXT("SemanticSubject")))
    {
        Metadata->CreateAttribute<FName>(TEXT("SemanticSubject"), NAME_None, false, false);
    }
    if (!Metadata->GetMutableTypedAttribute<FName>(TEXT("RestorationTag")))
    {
        Metadata->CreateAttribute<FName>(TEXT("RestorationTag"), NAME_None, false, false);
    }
}

bool UGloamsteadPCGSubsystem::WritePointContractMetadata(
    int32 PointIndex,
    FName WarningId,
    FName SemanticSubject,
    ERitualType RitualType,
    FName RestorationTag)
{
    if (!CachedPoints.IsValidIndex(PointIndex) || !MutablePointData || !MutablePointData->Metadata)
    {
        return false;
    }

    UPCGMetadata* Metadata = MutablePointData->Metadata;
    FPCGMetadataAttribute<int32>* RitualAttribute = Metadata->GetMutableTypedAttribute<int32>(TEXT("RitualType"));
    FPCGMetadataAttribute<FName>* WarningAttribute = Metadata->GetMutableTypedAttribute<FName>(TEXT("RecommendedForWarning"));
    FPCGMetadataAttribute<FName>* SubjectAttribute = Metadata->GetMutableTypedAttribute<FName>(TEXT("SemanticSubject"));
    FPCGMetadataAttribute<FName>* TagAttribute = Metadata->GetMutableTypedAttribute<FName>(TEXT("RestorationTag"));
    if (!RitualAttribute || !WarningAttribute || !SubjectAttribute || !TagAttribute)
    {
        return false;
    }

    // A duplicated graph point can share (or lack) a metadata entry. Give this point its own before
    // writing, or the stamp would land on every point sharing that entry - one site claiming the map.
    FPCGPoint& MutablePoint = CachedPoints[PointIndex];
    if (MutablePoint.MetadataEntry == PCGInvalidEntryKey)
    {
        Metadata->InitializeOnSet(MutablePoint.MetadataEntry);
    }

    const int64 Entry = MutablePoint.MetadataEntry;
    RitualAttribute->SetValue(Entry, static_cast<int32>(RitualType));
    WarningAttribute->SetValue(Entry, WarningId);
    SubjectAttribute->SetValue(Entry, SemanticSubject);
    TagAttribute->SetValue(Entry, RestorationTag);
    return true;
}

bool UGloamsteadPCGSubsystem::ResolveSiteAnchorLocation(uint8 Anchor, FVector& OutLocation) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    switch (static_cast<EGloamsteadSiteAnchor>(Anchor))
    {
    case EGloamsteadSiteAnchor::SanctuaryHeart:
    {
        for (TActorIterator<AVeilHeart> It(World); It; ++It)
        {
            OutLocation = It->GetActorLocation();
            return true;
        }
        return false;
    }
    case EGloamsteadSiteAnchor::FirstLantern:
    {
        TArray<AActor*> Anchors;
        UGameplayStatics::GetAllActorsWithTag(World, FirstLanternAnchorTag, Anchors);
        if (Anchors.Num() == 0 || !Anchors[0])
        {
            return false;
        }
        OutLocation = Anchors[0]->GetActorLocation();
        return true;
    }
    case EGloamsteadSiteAnchor::WorldOrigin:
    default:
        OutLocation = FVector::ZeroVector;
        return true;
    }
}

void UGloamsteadPCGSubsystem::ApplyAuthoredSiteContracts()
{
    UWorld* World = GetWorld();
    if (!World || CachedPoints.Num() == 0)
    {
        return;
    }

    TArray<UGloamsteadRitualSiteComponent*> Sites;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (UGloamsteadRitualSiteComponent* Site = It->FindComponentByClass<UGloamsteadRitualSiteComponent>())
        {
            Sites.Add(Site);
        }
    }


    TMap<FName, TWeakObjectPtr<AActor>> ClaimedSubjects;
    TSet<int32> ClaimedPoints;
    int32 BoundCount = 0;

    for (UGloamsteadRitualSiteComponent* Site : Sites)
    {
        AActor* Owner = Site->GetOwner();
        const FString OwnerName = Owner ? Owner->GetName() : TEXT("<no owner>");

        TArray<FString> Problems;
        if (!Site->IsCompleteDeclaration(Problems))
        {
            for (const FString& Problem : Problems)
            {
                UE_LOG(LogTemp, Error, TEXT("UGloamsteadPCGSubsystem: %s"), *Problem);
            }
            continue;
        }

        if (const TWeakObjectPtr<AActor>* Existing = ClaimedSubjects.Find(Site->SemanticSubject))
        {
            UE_LOG(LogTemp, Error,
                TEXT("UGloamsteadPCGSubsystem: semantic subject '%s' is declared by both '%s' and '%s'. Two actors ")
                TEXT("claiming one place is ambiguity, and ambiguity never resolves - refusing the second."),
                *Site->SemanticSubject.ToString(),
                Existing->IsValid() ? *Existing->Get()->GetName() : TEXT("<stale>"),
                *OwnerName);
            continue;
        }

        const FVector SiteLocation = Owner->GetActorLocation();
        const double RadiusSq = Site->BindRadius * Site->BindRadius;

        // Prefer a point the graph already typed for this ritual. Failing that, an unclaimed point of any
        // type is still a legitimate binding: the authored site is the AUTHORITY on what its place is, and
        // the graph currently types every point with the attribute default. Re-typing is announced, never
        // silent, and the first lantern's point is never eligible - Cycle 1 depends on it staying a
        // LanternPost.
        int32 TypedIndex = INDEX_NONE;
        double TypedDistSq = TNumericLimits<double>::Max();
        int32 AnyIndex = INDEX_NONE;
        double AnyDistSq = TNumericLimits<double>::Max();

        for (int32 Index = 0; Index < CachedPoints.Num(); ++Index)
        {
            if (ClaimedPoints.Contains(Index))
            {
                continue;
            }
            const double DistSq = FVector::DistSquared(CachedPoints[Index].Transform.GetLocation(), SiteLocation);
            if (DistSq > RadiusSq)
            {
                continue;
            }
            if (GetRitualTypeFromPoint(CachedPoints[Index]) == Site->RitualType)
            {
                if (DistSq < TypedDistSq)
                {
                    TypedDistSq = DistSq;
                    TypedIndex = Index;
                }
            }
            if (Index != AnchorSeatedPointIndex && DistSq < AnyDistSq)
            {
                AnyDistSq = DistSq;
                AnyIndex = Index;
            }
        }

        int32 BestIndex = TypedIndex;
        double BestDistSq = TypedDistSq;
        bool bRetyped = false;
        if (BestIndex == INDEX_NONE)
        {
            BestIndex = AnyIndex;
            BestDistSq = AnyDistSq;
            bRetyped = (BestIndex != INDEX_NONE);
        }

        if (BestIndex == INDEX_NONE)
        {
            UE_LOG(LogTemp, Error,
                TEXT("UGloamsteadPCGSubsystem: authored ritual site '%s' on '%s' binds nothing - no unclaimed ")
                TEXT("generated point within %.0f units. That night can never resolve a target. Move the actor ")
                TEXT("nearer a generated point, widen BindRadius, or check whether another site already claimed it."),
                *Site->SemanticSubject.ToString(),
                *OwnerName,
                Site->BindRadius);
            continue;
        }

        if (bRetyped)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("UGloamsteadPCGSubsystem: authored ritual site '%s' found no generated %s point in range, so ")
                TEXT("point %d is being re-typed to %s from the site's declaration. The site is the authority on ")
                TEXT("what this place is, but if the graph was meant to type it, that is the better fix."),
                *Site->SemanticSubject.ToString(),
                *GetRitualTypeDisplayName(Site->RitualType),
                BestIndex,
                *GetRitualTypeDisplayName(Site->RitualType));
        }

        if (!WritePointContractMetadata(
                BestIndex,
                Site->RecommendedForWarning,
                Site->SemanticSubject,
                Site->RitualType,
                Site->RestorationTag))
        {
            UE_LOG(LogTemp, Error,
                TEXT("UGloamsteadPCGSubsystem: authored ritual site '%s' matched point %d but its metadata could not ")
                TEXT("be written - the generated data is missing the contract attribute block."),
                *Site->SemanticSubject.ToString(),
                BestIndex);
            continue;
        }

        ClaimedSubjects.Add(Site->SemanticSubject, Owner);
        ClaimedPoints.Add(BestIndex);
        ++BoundCount;
        UE_LOG(LogTemp, Log,
            TEXT("UGloamsteadPCGSubsystem: bound authored site '%s' (%s/%s) to point %d at %.0f units."),
            *Site->SemanticSubject.ToString(),
            *Site->RecommendedForWarning.ToString(),
            *GetRitualTypeDisplayName(Site->RitualType),
            BestIndex,
            FMath::Sqrt(BestDistSq));
    }

    // Content-declared sites. A place in Gloamstead is described relative to a landmark, so these carry an
    // anchor rather than a transform - which also means they can be authored in the manifest without
    // opening the editor. Same C++ writer, same refusals; only the source of the declaration differs.
    int32 DeclaredCount = 0;
    if (UGloamsteadRitualSiteCatalog* SiteCatalog = Cast<UGloamsteadRitualSiteCatalog>(StaticLoadObject(
            UGloamsteadRitualSiteCatalog::StaticClass(), nullptr,
            TEXT("/Game/Data/DA_RitualSiteCatalog.DA_RitualSiteCatalog"))))
    {
        for (const FGloamsteadRitualSiteDeclaration& Declaration : SiteCatalog->Sites)
        {
            ++DeclaredCount;

            TArray<FString> Problems;
            if (!Declaration.IsCompleteDeclaration(Problems))
            {
                for (const FString& Problem : Problems)
                {
                    UE_LOG(LogTemp, Error, TEXT("UGloamsteadPCGSubsystem: %s"), *Problem);
                }
                continue;
            }

            if (ClaimedSubjects.Contains(Declaration.SemanticSubject))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("UGloamsteadPCGSubsystem: semantic subject '%s' is already claimed by a placed actor; ")
                    TEXT("refusing the authored declaration so one place has exactly one claimant."),
                    *Declaration.SemanticSubject.ToString());
                continue;
            }

            FVector AnchorLocation = FVector::ZeroVector;
            if (!ResolveSiteAnchorLocation(static_cast<uint8>(Declaration.Anchor), AnchorLocation))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("UGloamsteadPCGSubsystem: authored site '%s' names a landmark this map does not contain, ")
                    TEXT("so its place cannot be resolved. Place the landmark, or describe the site from one that exists."),
                    *Declaration.SemanticSubject.ToString());
                continue;
            }

            const double RadiusSq = Declaration.BindRadius * Declaration.BindRadius;
            const double MinSq = Declaration.MinimumAnchorDistance * Declaration.MinimumAnchorDistance;

            int32 BestIndex = INDEX_NONE;
            double BestDistSq = TNumericLimits<double>::Max();
            for (int32 Index = 0; Index < CachedPoints.Num(); ++Index)
            {
                if (ClaimedPoints.Contains(Index) || Index == AnchorSeatedPointIndex)
                {
                    continue;
                }
                const double DistSq = FVector::DistSquared(CachedPoints[Index].Transform.GetLocation(), AnchorLocation);
                if (DistSq > RadiusSq || DistSq < MinSq)
                {
                    continue;
                }
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    BestIndex = Index;
                }
            }

            if (BestIndex == INDEX_NONE)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("UGloamsteadPCGSubsystem: authored site '%s' binds nothing - no unclaimed generated point ")
                    TEXT("between %.0f and %.0f units of its anchor. Widen BindRadius, lower MinimumAnchorDistance, ")
                    TEXT("or check whether the graph produced points near that landmark at all."),
                    *Declaration.SemanticSubject.ToString(),
                    Declaration.MinimumAnchorDistance,
                    Declaration.BindRadius);
                continue;
            }

            if (!WritePointContractMetadata(
                    BestIndex,
                    Declaration.RecommendedForWarning,
                    Declaration.SemanticSubject,
                    Declaration.RitualType,
                    Declaration.RestorationTag))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("UGloamsteadPCGSubsystem: authored site '%s' matched point %d but its metadata could not be written."),
                    *Declaration.SemanticSubject.ToString(), BestIndex);
                continue;
            }

            ClaimedSubjects.Add(Declaration.SemanticSubject, nullptr);
            ClaimedPoints.Add(BestIndex);
            ++BoundCount;
            UE_LOG(LogTemp, Log,
                TEXT("UGloamsteadPCGSubsystem: bound authored site '%s' (%s/%s) to point %d at %.0f units from its anchor."),
                *Declaration.SemanticSubject.ToString(),
                *Declaration.RecommendedForWarning.ToString(),
                *GetRitualTypeDisplayName(Declaration.RitualType),
                BestIndex,
                FMath::Sqrt(BestDistSq));
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("UGloamsteadPCGSubsystem: bound %d of %d authored ritual site declaration(s) (%d placed, %d authored)."),
        BoundCount, Sites.Num() + DeclaredCount, Sites.Num(), DeclaredCount);
}

int32 UGloamsteadPCGSubsystem::FindRestoredPointIndex(bool bMostLit) const
{
    int32 BestIndex = -1;
    float BestLight = -1.f;
    for (int32 Index = 0; Index < PointStates.Num(); ++Index)
    {
        const FRitualPointState& State = PointStates[Index];
        if (!State.bIsRestored)
        {
            continue;
        }
        if (!bMostLit)
        {
            return Index; // first restored point is enough when brightness doesn't matter
        }
        if (State.LightLevel > BestLight)
        {
            BestLight = State.LightLevel;
            BestIndex = Index;
        }
    }
    return BestIndex;
}

bool UGloamsteadPCGSubsystem::RevertRestoration(int32 PointIndex)
{
    if (!PointStates.IsValidIndex(PointIndex))
    {
        return false;
    }
    FRitualPointState& State = PointStates[PointIndex];
    if (!State.bIsRestored)
    {
        return false; // nothing to reclaim
    }
    State.bIsRestored = false;
    State.LightLevel = FMath::Max(0.f, State.LightLevel * 0.5f); // the night takes back its light
    RestoredPointIndices.Remove(PointIndex);
    // Reclaiming is an authoritative restoration-flag transition. Observers
    // such as the one-way WorldForge projection must rebuild only after this
    // mutation has completed; the early returns above deliberately publish
    // nothing when there was no point or no restored state to reclaim.
    NotifyAuthoritativeStateRebuilt();
    return true;
}

TSet<int32> UGloamsteadPCGSubsystem::GetRestoredPointIndices() const
{
    return RestoredPointIndices;
}

void UGloamsteadPCGSubsystem::RebuildRestoredIndicesFromPointStates()
{
    RestoredPointIndices.Empty();
    for (int32 Index = 0; Index < PointStates.Num(); ++Index)
    {
        if (PointStates[Index].bIsRestored)
        {
            RestoredPointIndices.Add(Index);
        }
    }
}

void UGloamsteadPCGSubsystem::ReapplyRestoredState(const TSet<int32>& RestoredIndices)
{
    int32 RejectedCount = 0;
    for (int32 Index : RestoredIndices)
    {
        if (PointStates.IsValidIndex(Index))
        {
            PointStates[Index].bIsRestored = true;
        }
        else
        {
            ++RejectedCount;
        }
    }

    if (RejectedCount > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCG: ReapplyRestoredState dropped %d of %d incoming index/indices outside the current %d-point set."),
            RejectedCount, RestoredIndices.Num(), PointStates.Num());
    }

    // Derive the set from the flags we just wrote instead of assigning the caller's set wholesale.
    // A wholesale assign lets an out-of-range index sit in RestoredPointIndices with no PointStates
    // entry behind it: GetRestoredPointCount() then over-reports against IsPointRestored(), and
    // CaptureToSaveGame() persists the phantom so it survives the save/load round trip.
    RebuildRestoredIndicesFromPointStates();
    NotifyAuthoritativeStateRebuilt();
}

void UGloamsteadPCGSubsystem::CaptureToSaveGame(UGloamsteadSaveGame* SaveGame) const
{
    if (!SaveGame)
    {
        return;
    }
    SaveGame->PointStates           = PointStates;
    SaveGame->RestoredPointIndices  = RestoredPointIndices.Array();
    SaveGame->WorldSeed             = CurrentWorldSeed;
    SaveGame->SaveVersion           = UGloamsteadSaveGame::CurrentSaveVersion;
}

bool UGloamsteadPCGSubsystem::RestoreFromSaveGame(UGloamsteadSaveGame* SaveGame)
{
    if (!SaveGame)
    {
        return false;
    }

    // This is the first PCG consumer of a loaded payload. Do not read either
    // point state or authored state until the schema is known to be current.
    if (!SaveGame->MigrateToCurrentVersion())
    {
        UE_LOG(LogTemp, Error, TEXT("UGloamsteadPCGSubsystem: refusing to restore unsupported save version %d."), SaveGame->SaveVersion);
        return false;
    }

    // Full per-point restore (light + corruption + flags), unlike ReapplyRestoredState which only flips flags.
    PointStates      = SaveGame->PointStates;
    CurrentWorldSeed = SaveGame->WorldSeed;

    // Same wholesale-assign hazard as ReapplyRestoredState: the persisted index list is a second copy
    // of the restored view, and a save taken against a larger point set carries indices this
    // PointStates has no entry for. Derive the set from the flags we just loaded — the same record
    // that carries light and corruption — and report any disagreement instead of trusting the copy.
    RebuildRestoredIndicesFromPointStates();

    int32 UnbackedCount = 0;
    for (int32 Index : SaveGame->RestoredPointIndices)
    {
        if (!RestoredPointIndices.Contains(Index))
        {
            ++UnbackedCount;
        }
    }
    if (UnbackedCount > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCG: RestoreFromSaveGame ignored %d of %d persisted restored index/indices with no restored point behind them (save holds %d points)."),
            UnbackedCount, SaveGame->RestoredPointIndices.Num(), SaveGame->PointStates.Num());
    }

    NotifyAuthoritativeStateRebuilt();
    return true;
}

bool UGloamsteadPCGSubsystem::SaveToSlot(const FString& SlotName, int32 UserIndex) const
{
    UGloamsteadSaveGame* SaveGame = Cast<UGloamsteadSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UGloamsteadSaveGame::StaticClass()));
    if (!SaveGame)
    {
        return false;
    }
    CaptureToSaveGame(SaveGame);
    return UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex);
}

bool UGloamsteadPCGSubsystem::LoadFromSlot(const FString& SlotName, int32 UserIndex)
{
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
    {
        return false;
    }
    UGloamsteadSaveGame* SaveGame = Cast<UGloamsteadSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
    if (!SaveGame)
    {
        return false;
    }
    return RestoreFromSaveGame(SaveGame);
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

void UGloamsteadPCGSubsystem::NotifyAuthoritativeStateRebuilt()
{
    AuthoritativeStateRebuilt.Broadcast();
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
    if (!CachedPoints.IsValidIndex(PointIndex) || !MutablePointData || !MutablePointData->Metadata) return;

    const FRitualPointState& State = PointStates[PointIndex];
    const FPCGPoint& Point = CachedPoints[PointIndex];
    UPCGMetadata* Meta = MutablePointData->Metadata;

    if (FPCGMetadataAttribute<bool>* AttrBool = Meta->GetMutableTypedAttribute<bool>(TEXT("bIsRestored")))
    {
        AttrBool->SetValue(Point.MetadataEntry, State.bIsRestored);
    }
    if (FPCGMetadataAttribute<float>* AttrFloat = Meta->GetMutableTypedAttribute<float>(TEXT("LightLevel")))
    {
        AttrFloat->SetValue(Point.MetadataEntry, State.LightLevel);
    }
    if (FPCGMetadataAttribute<float>* AttrCorrupt = Meta->GetMutableTypedAttribute<float>(TEXT("CorruptionLevel")))
    {
        AttrCorrupt->SetValue(Point.MetadataEntry, State.CorruptionLevel);
    }

    MutablePointData->SetPoints(CachedPoints);
}

// ==================== Private Attribute Helpers (metadata-backed) ====================

bool UGloamsteadPCGSubsystem::GetBoolAttribute(const FPCGPoint& Point, FName AttributeName, bool DefaultValue) const
{
    if (const UPCGMetadata* Meta = (MutablePointData ? MutablePointData->Metadata : nullptr))
    {
        if (const FPCGMetadataAttribute<bool>* Attr = Meta->GetConstTypedAttribute<bool>(AttributeName))
        {
            return Attr->GetValueFromItemKey(Point.MetadataEntry);
        }
    }
    return DefaultValue;
}

float UGloamsteadPCGSubsystem::GetFloatAttribute(const FPCGPoint& Point, FName AttributeName, float DefaultValue) const
{
    if (const UPCGMetadata* Meta = (MutablePointData ? MutablePointData->Metadata : nullptr))
    {
        if (const FPCGMetadataAttribute<float>* Attr = Meta->GetConstTypedAttribute<float>(AttributeName))
        {
            return Attr->GetValueFromItemKey(Point.MetadataEntry);
        }
    }
    return DefaultValue;
}

int32 UGloamsteadPCGSubsystem::GetIntAttribute(const FPCGPoint& Point, FName AttributeName, int32 DefaultValue) const
{
    if (const UPCGMetadata* Meta = (MutablePointData ? MutablePointData->Metadata : nullptr))
    {
        if (const FPCGMetadataAttribute<int32>* Attr = Meta->GetConstTypedAttribute<int32>(AttributeName))
        {
            return Attr->GetValueFromItemKey(Point.MetadataEntry);
        }
    }
    return DefaultValue;
}

FName UGloamsteadPCGSubsystem::GetNameAttribute(const FPCGPoint& Point, FName AttributeName, FName DefaultValue) const
{
    if (const UPCGMetadata* Meta = (MutablePointData ? MutablePointData->Metadata : nullptr))
    {
        if (const FPCGMetadataAttribute<FName>* Attr = Meta->GetConstTypedAttribute<FName>(AttributeName))
        {
            return Attr->GetValueFromItemKey(Point.MetadataEntry);
        }
    }
    return DefaultValue;
}

bool UGloamsteadPCGSubsystem::PointMatchesExperiencePlan(
    int32 PointIndex,
    const FExperienceCyclePlan& Plan,
    bool bRequireRestored) const
{
    if (!Plan.IsAuthoredPlan()
        || Plan.WarningId == NAME_None
        || Plan.SemanticSubject == NAME_None
        || Plan.RequiredRitualType == ERitualType::Invalid
        || Plan.RequiredRestorationTags.Num() != 1
        || Plan.RequiredRestorationTags[0] == NAME_None
        || !CachedPoints.IsValidIndex(PointIndex)
        || (bRequireRestored && !IsPointRestored(PointIndex)))
    {
        return false;
    }

    const FPCGPoint& Point = CachedPoints[PointIndex];
    return GetNameAttribute(Point, TEXT("RecommendedForWarning"), NAME_None) == Plan.WarningId
        && GetNameAttribute(Point, TEXT("SemanticSubject"), NAME_None) == Plan.SemanticSubject
        && static_cast<ERitualType>(GetIntAttribute(Point, TEXT("RitualType"), static_cast<int32>(ERitualType::Invalid))) == Plan.RequiredRitualType
        && GetNameAttribute(Point, TEXT("RestorationTag"), NAME_None) == Plan.RequiredRestorationTags[0];
}

bool UGloamsteadPCGSubsystem::PopulateAuthoritativeRestorationMetadata(
    int32 PointIndex,
    FRestorationEventPayload& InOutPayload) const
{
    if (!CachedPoints.IsValidIndex(PointIndex))
    {
        return false;
    }

    const FPCGPoint& Point = CachedPoints[PointIndex];
    InOutPayload.PointIndex = PointIndex;
    InOutPayload.RitualType = static_cast<ERitualType>(
        GetIntAttribute(Point, TEXT("RitualType"), static_cast<int32>(ERitualType::Invalid)));
    InOutPayload.WarningId = GetNameAttribute(Point, TEXT("RecommendedForWarning"), NAME_None);
    InOutPayload.SemanticSubject = GetNameAttribute(Point, TEXT("SemanticSubject"), NAME_None);
    InOutPayload.WarningTagSatisfied = GetNameAttribute(Point, TEXT("RestorationTag"), NAME_None);
    return true;
}

FVector UGloamsteadPCGSubsystem::GetVectorAttribute(const FPCGPoint& Point, FName AttributeName, FVector DefaultValue) const
{
    if (const UPCGMetadata* Meta = (MutablePointData ? MutablePointData->Metadata : nullptr))
    {
        if (const FPCGMetadataAttribute<FVector>* Attr = Meta->GetConstTypedAttribute<FVector>(AttributeName))
        {
            return Attr->GetValueFromItemKey(Point.MetadataEntry);
        }
    }
    return DefaultValue;
}

ERitualType UGloamsteadPCGSubsystem::GetRitualTypeFromPoint(const FPCGPoint& Point) const
{
    return static_cast<ERitualType>(GetIntAttribute(Point, "RitualType", 0));
}
