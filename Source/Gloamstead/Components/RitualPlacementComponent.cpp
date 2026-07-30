#include "Components/RitualPlacementComponent.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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

    EnsureRitualDefinitionsLoaded();
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

    // We keep spawning the actor before restoring, and undo it on failure, rather than spawning after.
    // The payload carries RestoredActor and ApplyRestoration broadcasts that payload from inside itself,
    // so restoring first would hand every OnStructureRestored listener a null actor. The price of
    // keeping that contract is an orphan on failure, so every failure path below destroys the actor.
    AActor* SpawnedActor = nullptr;
    SpawnRestoredActor(FinalPointIndex, SpawnedActor);

    FRestorationEventPayload Payload;
    if (!BuildRestorationPayload(FinalPointIndex, SpawnedActor, Payload))
    {
        if (SpawnedActor) SpawnedActor->Destroy();
        UE_LOG(LogTemp, Warning, TEXT("RitualPlacement: Could not build a payload for point %d; placement aborted."), FinalPointIndex);
        return false;
    }

    if (!CachedSubsystem->ApplyRestoration(FinalPointIndex, Payload))
    {
        if (SpawnedActor) SpawnedActor->Destroy();
        UE_LOG(LogTemp, Warning, TEXT("RitualPlacement: Restoration of point %d was rejected; placement aborted."), FinalPointIndex);
        return false;
    }

    OnPlacementConfirmed(FinalPointIndex);
    OnRestoredActorSpawned(SpawnedActor, FinalPointIndex, Payload.RitualType);
    UE_LOG(LogTemp, Log, TEXT("RitualPlacement: Restored point %d type %d"), FinalPointIndex, static_cast<int32>(Payload.RitualType));
    ExitPlacementMode();

    return true;
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
            ResolvedType = static_cast<ERitualType>(CachedSubsystem->GetIntAttribute(Point, "RitualType", 0));
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

    const int32 TypeInt = CachedSubsystem->GetIntAttribute(Point, "RitualType", -1);
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

    const float Radius = CachedSubsystem->GetFloatAttribute(Point, "RestorationRadius", 800.0f);
    const float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Point.Transform.GetLocation());
    return Distance <= Radius * 1.25f;
}

bool URitualPlacementComponent::IsCurrentPlacementValid() const
{
    return IsPointValidForPlacement(CurrentTargetPointIndex);
}

bool URitualPlacementComponent::BuildRestorationPayload(int32 PointIndex, AActor* SpawnedRestoredActor, FRestorationEventPayload& OutPayload) const
{
    OutPayload = FRestorationEventPayload();

    if (!CachedSubsystem) return false;

    FPCGPoint Point;
    if (!CachedSubsystem->GetPointByIndex(PointIndex, Point)) return false;

    // Assigned first, so no path can reach the return-true below still carrying the -1 default:
    // ApplyRestoration rejects a payload whose index disagrees with its target.
    OutPayload.PointIndex = PointIndex;
    OutPayload.RitualType = static_cast<ERitualType>(CachedSubsystem->GetIntAttribute(Point, "RitualType", 0));
    OutPayload.WorldLocation = Point.Transform.GetLocation();
    OutPayload.PathSegmentID = CachedSubsystem->GetIntAttribute(Point, "PathSegmentID", -1);
    OutPayload.PathPosition = CachedSubsystem->GetFloatAttribute(Point, "PathPosition", 0.0f);
    OutPayload.RestoredActor = SpawnedRestoredActor;
    OutPayload.WarningTagSatisfied = CachedSubsystem->GetNameAttribute(Point, "RecommendedForWarning", NAME_None);

    OutPayload.LightDelta = GetDefaultLightContribution(OutPayload.RitualType);
    OutPayload.CorruptionCleared = GetDefaultCorruptionClearance(OutPayload.RitualType);

    if (const URitualDefinition* Definition = GetRitualDefinitionForType(OutPayload.RitualType))
    {
        OutPayload.LightDelta = Definition->DefaultLightContribution;
        OutPayload.CorruptionCleared = Definition->DefaultCorruptionClearance;

        if (OutPayload.WarningTagSatisfied == NAME_None && Definition->SatisfiableWarningTags.Num() > 0)
        {
            OutPayload.WarningTagSatisfied = Definition->SatisfiableWarningTags[0];
        }
    }

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

    return true;
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
    Info.RitualType = static_cast<ERitualType>(CachedSubsystem->GetIntAttribute(Point, "RitualType", 0));

    const FVector Normal = CachedSubsystem->GetVectorAttribute(Point, "TerrainNormal", FVector::UpVector);
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

ERitualType URitualPlacementComponent::GetCurrentTargetRitualType() const
{
    return GetCurrentTargetPointInfo().RitualType;
}

void URitualPlacementComponent::EnsureRitualDefinitionsLoaded()
{
    // The DA_Ritual_* assets are the designer tuning surface for light/corruption per ritual type.
    // Nothing populated this map, so GetRitualDefinitionForType() always missed and every payload
    // silently used the RitualTypes.cpp switch defaults. Load them here, following the same
    // StaticLoadObject-by-path pattern as NightConsequenceManager's catalog and the Veil Heart's.
    struct FRitualDefinitionAsset
    {
        ERitualType Type;
        const TCHAR* ObjectPath;
    };

    static const FRitualDefinitionAsset DefinitionAssets[] =
    {
        { ERitualType::LanternPost,  TEXT("/Game/Data/DA_Ritual_LanternPost.DA_Ritual_LanternPost")   },
        { ERitualType::GardenBed,    TEXT("/Game/Data/DA_Ritual_GardenBed.DA_Ritual_GardenBed")       },
        { ERitualType::PathPoint,    TEXT("/Game/Data/DA_Ritual_PathPoint.DA_Ritual_PathPoint")       },
        { ERitualType::MirrorPillar, TEXT("/Game/Data/DA_Ritual_MirrorPillar.DA_Ritual_MirrorPillar") },
        { ERitualType::BellShrine,   TEXT("/Game/Data/DA_Ritual_BellShrine.DA_Ritual_BellShrine")     },
    };

    int32 LoadedCount = 0;
    for (const FRitualDefinitionAsset& Entry : DefinitionAssets)
    {
        // An entry assigned on the component in the editor is a deliberate override; leave it alone.
        if (const TObjectPtr<URitualDefinition>* Existing = RitualDefinitions.Find(Entry.Type))
        {
            if (*Existing) continue;
        }

        URitualDefinition* Loaded = Cast<URitualDefinition>(
            StaticLoadObject(URitualDefinition::StaticClass(), nullptr, Entry.ObjectPath));
        if (!Loaded)
        {
            // Not fatal: BuildRestorationPayload falls back to GetDefaultLightContribution /
            // GetDefaultCorruptionClearance for any type with no definition.
            continue;
        }

        if (Loaded->RitualType != Entry.Type)
        {
            UE_LOG(LogTemp, Warning, TEXT("RitualPlacementComponent: %s declares RitualType %d but is keyed as %d; keying by path."),
                Entry.ObjectPath, static_cast<int32>(Loaded->RitualType), static_cast<int32>(Entry.Type));
        }

        RitualDefinitions.Add(Entry.Type, Loaded);
        ++LoadedCount;
    }

    UE_LOG(LogTemp, Log, TEXT("RitualPlacementComponent: Loaded %d ritual definition asset(s); %d type(s) now mapped."),
        LoadedCount, RitualDefinitions.Num());
}

const URitualDefinition* URitualPlacementComponent::GetRitualDefinitionForType(ERitualType Type) const
{
    if (const TObjectPtr<URitualDefinition>* Found = RitualDefinitions.Find(Type))
    {
        return Found->Get();
    }
    return nullptr;
}

class UGloamsteadPCGSubsystem* URitualPlacementComponent::GetSubsystem() const
{
    return CachedSubsystem;
}