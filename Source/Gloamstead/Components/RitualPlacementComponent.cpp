#include "Components/RitualPlacementComponent.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

URitualPlacementComponent::URitualPlacementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;

    QueryUpdateInterval = 0.15f;
    QueryMovementThreshold = 75.0f;
    VerticalOffset = 12.0f;
    SteepSlopeCameraBias = 28.0f;
}

void URitualPlacementComponent::BeginPlay()
{
    Super::BeginPlay();

    CachedSubsystem = GetWorld()->GetSubsystem<UGloamsteadPCGSubsystem>();
    if (!CachedSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("RitualPlacementComponent: Could not get UGloamsteadPCGSubsystem. Placement will be disabled."));
    }
}

void URitualPlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsInPlacementMode || !CachedSubsystem) return;

    TimeSinceLastQuery += DeltaTime;
    const FVector CurrentLocation = GetOwner()->GetActorLocation();
    const float DistanceMoved = FVector::Dist(CurrentLocation, LastQueryLocation);

    if (TimeSinceLastQuery >= QueryUpdateInterval || DistanceMoved > QueryMovementThreshold)
    {
        UpdateTargetPoint();
        TimeSinceLastQuery = 0.0f;
        LastQueryLocation = CurrentLocation;
    }
}

void URitualPlacementComponent::EnterPlacementMode()
{
    if (!CachedSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("RitualPlacementComponent: Cannot enter placement mode - Subsystem is missing."));
        return;
    }

    bIsInPlacementMode = true;
    CurrentMode = ERitualType::LanternPost;
    CurrentTargetPointIndex = -1;
    bHasShownPathPointMessageThisSession = false;

    LastQueryLocation = GetOwner()->GetActorLocation();
    TimeSinceLastQuery = QueryUpdateInterval;

    UpdateTargetPoint();
}

void URitualPlacementComponent::ExitPlacementMode()
{
    if (!bIsInPlacementMode) return;

    bIsInPlacementMode = false;
    CurrentTargetPointIndex = -1;

    OnPlacementModeExited();
}

bool URitualPlacementComponent::ConfirmPlacement()
{
    if (!bIsInPlacementMode || !CachedSubsystem) return false;
    if (!IsCurrentPlacementValid()) return false;

    const int32 FinalPointIndex = ResolveTargetForPlacement(CurrentTargetPointIndex);
    if (FinalPointIndex == -1 || CachedSubsystem->IsPointRestored(FinalPointIndex)) return false;

    AActor* SpawnedActor = nullptr;
    SpawnRestoredActor(FinalPointIndex, SpawnedActor);

    FRestorationEventPayload Payload;
    BuildRestorationPayload(FinalPointIndex, SpawnedActor, Payload);

    const bool bSuccess = CachedSubsystem->ApplyRestoration(FinalPointIndex, Payload);

    if (bSuccess)
    {
        OnPlacementConfirmed(FinalPointIndex);
        ExitPlacementMode();
    }

    return bSuccess;
}

void URitualPlacementComponent::ForceUpdatePreview()
{
    if (bIsInPlacementMode) UpdateTargetPoint();
}

void URitualPlacementComponent::UpdateTargetPoint()
{
    if (!CachedSubsystem) return;

    const FVector PlayerLocation = GetOwner()->GetActorLocation();
    const float SearchRadius = 1600.0f;

    int32 RawIndex = CachedSubsystem->FindNearestUnrestoredPointIndex(PlayerLocation, CurrentMode, SearchRadius);
    const int32 ResolvedIndex = ResolveTargetForPlacement(RawIndex);

    if (ResolvedIndex != CurrentTargetPointIndex)
    {
        CurrentTargetPointIndex = ResolvedIndex;

        FPCGPoint Point;
        ERitualType ResolvedType = ERitualType::Invalid;

        if (CurrentTargetPointIndex != -1 && CachedSubsystem->GetPointByIndex(CurrentTargetPointIndex, Point))
        {
            ResolvedType = static_cast<ERitualType>(Point.GetMetadataEntry<int32>("RitualType", 0));
        }

        const bool bValid = IsCurrentPlacementValid();
        OnPreviewTargetChanged(CurrentTargetPointIndex, ResolvedType, bValid);
    }
}

int32 URitualPlacementComponent::ResolveTargetForPlacement(int32 RawPointIndex)
{
    if (RawPointIndex == -1 || !CachedSubsystem) return -1;

    FPCGPoint Point;
    if (!CachedSubsystem->GetPointByIndex(RawPointIndex, Point)) return -1;

    const int32 TypeInt = Point.GetMetadataEntry<int32>("RitualType", -1);
    const ERitualType Type = static_cast<ERitualType>(TypeInt);

    if (Type != ERitualType::PathPoint) return RawPointIndex;

    const FVector PointLocation = Point.Transform.GetLocation();
    const int32 RedirectedIndex = CachedSubsystem->FindNearestUnrestoredPointIndex(PointLocation, ERitualType::LanternPost, 1400.0f);

    if (!bHasShownPathPointMessageThisSession && RedirectedIndex != -1)
    {
        OnPathPointRedirected(TEXT("This point is part of a path — restore a lantern nearby to activate it."));
        bHasShownPathPointMessageThisSession = true;
    }

    return RedirectedIndex;
}

bool URitualPlacementComponent::IsPointValidForPlacement(int32 PointIndex) const
{
    if (!CachedSubsystem || PointIndex == -1) return false;
    if (CachedSubsystem->IsPointRestored(PointIndex)) return false;

    FPCGPoint Point;
    if (!CachedSubsystem->GetPointByIndex(PointIndex, Point)) return false;

    const float Radius = Point.GetMetadataEntry<float>("RestorationRadius", 800.0f);
    const float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Point.Transform.GetLocation());
    return Distance <= Radius * 1.25f;
}

bool URitualPlacementComponent::IsCurrentPlacementValid() const
{
    return IsPointValidForPlacement(CurrentTargetPointIndex);
}

void URitualPlacementComponent::BuildRestorationPayload(int32 PointIndex, AActor* SpawnedRestoredActor, FRestorationEventPayload& OutPayload) const
{
    OutPayload = FRestorationEventPayload();

    if (!CachedSubsystem || !CachedSubsystem->GetPointByIndex(PointIndex, OutPayload)) // reuse GetPointByIndex to validate
    {
        return;
    }

    FPCGPoint Point;
    if (!CachedSubsystem->GetPointByIndex(PointIndex, Point)) return;

    OutPayload.PointIndex = PointIndex;
    OutPayload.RitualType = static_cast<ERitualType>(Point.GetMetadataEntry<int32>("RitualType", 0));
    OutPayload.WorldLocation = Point.Transform.GetLocation();
    OutPayload.PathSegmentID = Point.GetMetadataEntry<int32>("PathSegmentID", -1);
    OutPayload.PathPosition = Point.GetMetadataEntry<float>("PathPosition", 0.0f);
    OutPayload.RestoredActor = SpawnedRestoredActor;
    OutPayload.WarningTagSatisfied = Point.GetMetadataEntry<FName>("RecommendedForWarning", NAME_None);

    OutPayload.LightDelta = (OutPayload.RitualType == ERitualType::LanternPost) ? 0.35f : 0.15f;
    OutPayload.CorruptionCleared = (OutPayload.RitualType == ERitualType::LanternPost) ? 0.2f : 0.35f;

    OutPayload.TimeOfDayAtRestoration = 0.5f;
    OutPayload.NightCountAtRestoration = 0;
    if (const UWorld* World = GetWorld())
    {
        if (const UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>())
        {
            OutPayload.TimeOfDayAtRestoration = DayNight->GetNormalizedTimeOfDay();
            OutPayload.NightCountAtRestoration = DayNight->GetNightCount();
        }
    }
}

FRotator URitualPlacementComponent::CalculateAlignedRotation(const FVector& Location, const FVector& TerrainNormal) const
{
    FRotator Rotation = TerrainNormal.Rotation();

    const float SlopeDot = FVector::DotProduct(TerrainNormal, FVector::UpVector);
    if (SlopeDot < 0.6f)
    {
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            FVector CameraForward = PC->GetControlRotation().Vector();
            CameraForward.Z = 0.0f;
            CameraForward.Normalize();

            FVector BlendedForward = FMath::Lerp(TerrainNormal, CameraForward, 0.35f);
            Rotation = BlendedForward.Rotation();
        }
    }
    return Rotation;
}

FRitualPointInfo URitualPlacementComponent::GetCurrentTargetPointInfo() const
{
    FRitualPointInfo Info;
    Info.PointIndex = CurrentTargetPointIndex;
    Info.bIsValid = IsCurrentPlacementValid();

    if (!CachedSubsystem || CurrentTargetPointIndex == -1) return Info;

    FPCGPoint Point;
    if (!CachedSubsystem->GetPointByIndex(CurrentTargetPointIndex, Point)) return Info;

    Info.Location = Point.Transform.GetLocation();
    Info.RitualType = static_cast<ERitualType>(Point.GetMetadataEntry<int32>("RitualType", 0));

    const FVector Normal = Point.GetMetadataEntry<FVector>("TerrainNormal", FVector::UpVector);
    Info.Rotation = CalculateAlignedRotation(Info.Location, Normal);
    Info.Location += Normal * VerticalOffset;

    return Info;
}

bool URitualPlacementComponent::GetCurrentTargetTransform(FVector& OutLocation, FRotator& OutRotation) const
{
    const FRitualPointInfo Info = GetCurrentTargetPointInfo();
    if (Info.PointIndex == -1) return false;

    OutLocation = Info.Location;
    OutRotation = Info.Rotation;
    return true;
}

class UGloamsteadPCGSubsystem* URitualPlacementComponent::GetSubsystem() const
{
    return CachedSubsystem;
}