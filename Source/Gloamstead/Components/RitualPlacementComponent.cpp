#include "Components/RitualPlacementComponent.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/GloamsteadSurveySubjectRegistry.h"
#include "Gloamstead.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"

// How many times one already-built evidence record may be handed to the publisher. See the RETRY RULE
// comment in PublishRestorationEvidence for why the record is never rebuilt between attempts.
//
// Distinctively named at file scope rather than sitting in an anonymous namespace: an anonymous
// namespace does NOT keep helper names apart once files land in the same unity translation unit (the
// same C2264 note as GloamsteadSurveySubjectTypes.cpp:170-174).
static constexpr int32 GRitualEvidenceEmitAttemptLimit = 2;

// Declared in the header, where the code's meaning and its unregistered status are documented.
const FString URitualPlacementComponent::GSSRestoredActorMissing = TEXT("GSS016");

URitualPlacementComponent::URitualPlacementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;

    QueryUpdateInterval = 0.15f;
    QueryMovementThreshold = 75.0f;
    VerticalOffset = 12.0f;
    SteepSlopeCameraBias = 28.0f;

    EvidenceSubjectId = TEXT("courtyard.lantern.first");
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
        CachedSubsystem = GetSubsystem();
    }
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

    // Cancel must leave nothing behind, or re-entry stacks a second ghost on the first.
    DestroyPreviewActor();

    OnPlacementModeExited();
}

void URitualPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // A ghost outliving PIE would be a visible leak in the next session.
    DestroyPreviewActor();
    Super::EndPlay(EndPlayReason);
}

UClass* URitualPlacementComponent::ResolvePreviewClass() const
{
    if (PreviewActorClass)
    {
        return PreviewActorClass;
    }
    if (!bUseProjectDefaultPreviewClass)
    {
        return nullptr;
    }
    // Mirrors SpawnRestoredActor_Implementation's fallback: the slice ships one project-owned preview.
    static const TCHAR* PreviewPath = TEXT("/Game/Gloamstead/Placement/BP_RitualPreview.BP_RitualPreview_C");
    return LoadClass<AActor>(nullptr, PreviewPath);
}

void URitualPlacementComponent::DestroyPreviewActor()
{
    if (AActor* Preview = ActivePreviewActor.Get())
    {
        Preview->Destroy();
    }
    ActivePreviewActor.Reset();
}

void URitualPlacementComponent::RefreshPreviewActor()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // A ghost is shown only for a target the player could actually confirm. Anything else — out of
    // placement mode, no target, or a target out of range — shows nothing, so the preview's presence
    // is itself the readable signal that confirming will work.
    // Initialised because GetCurrentTargetTransform only writes them on success, and the && below
    // short-circuits before it on the no-target paths.
    FVector TargetLocation = FVector::ZeroVector;
    FRotator TargetRotation = FRotator::ZeroRotator;
    const bool bShouldShow = bIsInPlacementMode
        && IsCurrentPlacementValid()
        && GetCurrentTargetTransform(TargetLocation, TargetRotation);

    if (!bShouldShow)
    {
        DestroyPreviewActor();
        return;
    }

    if (AActor* Existing = ActivePreviewActor.Get())
    {
        // Move the one we have rather than respawning: a ghost that blinks every query tick reads as
        // a bug, and this is what makes the preview "stable" as the player walks around.
        Existing->SetActorLocationAndRotation(TargetLocation, TargetRotation);
        return;
    }

    UClass* PreviewClass = ResolvePreviewClass();
    if (!PreviewClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("RitualPlacementComponent: no preview class resolved; placement will have no ghost."));
        return;
    }

    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient; // never let a ghost be saved into the level

    AActor* Spawned = World->SpawnActor<AActor>(PreviewClass, TargetLocation, TargetRotation, Params);
    if (!Spawned)
    {
        UE_LOG(LogTemp, Warning, TEXT("RitualPlacementComponent: preview actor failed to spawn at %s."), *TargetLocation.ToCompactString());
        return;
    }

    Spawned->Tags.AddUnique(TEXT("Gloamstead.RitualPreview"));
    ActivePreviewActor = Spawned;

    UE_LOG(LogTemp, Log, TEXT("RitualPlacement: preview ghost spawned at %s (target point %d)."),
        *TargetLocation.ToCompactString(), CurrentTargetPointIndex);
}

bool URitualPlacementComponent::ConfirmPlacement()
{
    if (!bIsInPlacementMode || !CachedSubsystem) return false;
    if (!IsCurrentPlacementValid()) return false;

    // The evidence request id is minted HERE — before the target is resolved, before anything is
    // spawned, and long before the survey subject is resolved inside BuildRequest. One id per
    // confirmation, carried unchanged through resolution, publication and the GetLastEvidence* getters.
    //
    // Minting it later (or letting BuildRequest generate its own) would break two things at once: the
    // abort logs below could not name the attempt they belong to, and a retry would file a SECOND
    // artifact for ONE restoration instead of re-publishing the same one.
    const FString EvidenceRequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

    const int32 FinalPointIndex = ResolveTargetForPlacement(CurrentTargetPointIndex);
    if (FinalPointIndex == -1 || CachedSubsystem->IsPointRestored(FinalPointIndex)) return false;

    // We keep spawning the actor before restoring, and undo it on failure, rather than spawning after.
    // The payload carries RestoredActor and ApplyRestoration broadcasts that payload from inside itself,
    // so restoring first would hand every OnStructureRestored listener a null actor. The price of
    // keeping that contract is an orphan on failure, so every failure path below destroys the actor.
    AActor* SpawnedActor = nullptr;
    SpawnRestoredActor(FinalPointIndex, SpawnedActor);

    // === RULE: a confirmation that spawns NO actor still restores the point, and is reported as a
    // === degraded success. It is never refused, and never reported as clean.
    //
    // SpawnRestoredActor is a BlueprintNativeEvent. Its native implementation materialises the configured
    // LanternPost class; a missing class still returns null and follows the degraded-success rule below.
    // The three candidate rules:
    //
    //   REFUSE — return false here. Correct in the abstract, and wrong today: it would make the ritual
    //     loop unplayable for everyone until the presentation lane lands a Blueprint child.
    //   PROCEED SILENTLY — what this code did before. This is the actual defect: the point is consumed,
    //     ApplyRestoration will refuse it forever after (GloamsteadPCGSubsystem.cpp:296-300), the player
    //     permanently loses that lantern with nothing visible to show for it, and the evidence artifact
    //     records a clean success. A false success written down durably is worse than a loud failure.
    //   PROCEED AND MARK — chosen. The restoration happens and ConfirmPlacement still returns true (a
    //     false return would tell Blueprint the placement failed while the point was in fact spent,
    //     inviting a retry that can never succeed), but the confirmation is stamped GSS016 in the emitted
    //     artifact, flagged through WasLastRestoredActorMissing(), and logged at Error.
    //
    // The mark is NOT applied from here. It is derived in PublishRestorationEvidence from
    // Payload.RestoredActor — the same field ApplyRestoration broadcast to every listener — so no caller
    // can reach the publisher and forget to declare it, and a spawned actor that dies before publication
    // is caught too.

    FRestorationEventPayload Payload;
    if (!BuildRestorationPayload(FinalPointIndex, SpawnedActor, Payload))
    {
        if (SpawnedActor) SpawnedActor->Destroy();
        UE_LOG(LogTemp, Warning, TEXT("RitualPlacement: Could not build a payload for point %d (request %s); placement aborted."),
            FinalPointIndex, *EvidenceRequestId);
        return false;
    }

    // Restore, then report on what was restored. The false return means the RESTORATION was rejected;
    // a failed report never lands here (see CommitRestorationWithEvidence).
    if (!CommitRestorationWithEvidence(CachedSubsystem, FinalPointIndex, Payload, EvidenceRequestId))
    {
        if (SpawnedActor) SpawnedActor->Destroy();
        UE_LOG(LogTemp, Warning, TEXT("RitualPlacement: Restoration of point %d (request %s) was rejected; placement aborted."),
            FinalPointIndex, *EvidenceRequestId);
        return false;
    }

    // The ghost has been replaced by the real thing; remove it before any listener can see both.
    DestroyPreviewActor();

    // The Blueprint notifications fire AFTER the evidence is published, deliberately. They run arbitrary
    // Blueprint that is free to move, re-parent or destroy the actor that was just spawned, and the
    // artifact has to describe the world as the restoration left it — not as a listener rearranged it.
    OnPlacementConfirmed(FinalPointIndex);
    OnRestoredActorSpawned(SpawnedActor, FinalPointIndex, Payload.RitualType);
    UE_LOG(LogTemp, Log, TEXT("RitualPlacement: Restored point %d type %d (request %s)"),
        FinalPointIndex, static_cast<int32>(Payload.RitualType), *EvidenceRequestId);
    ExitPlacementMode();

    return true;
}

bool URitualPlacementComponent::CommitRestorationWithEvidence(
    UGloamsteadPCGSubsystem* Subsystem, int32 PointIndex,
    const FRestorationEventPayload& Payload, const FString& RequestId)
{
    if (!Subsystem) return false;

    // The one authoritative act, performed exactly once. Everything after this line is reporting.
    if (!Subsystem->ApplyRestoration(PointIndex, Payload))
    {
        // Nothing was restored, so there is nothing to report and no artifact is written. This is also
        // where a REPEATED confirmation of the same point lands: ApplyRestoration refuses an
        // already-restored point (GloamsteadPCGSubsystem.cpp:296-300), so a second confirmation cannot
        // reach the publisher and cannot produce a second successful report.
        return false;
    }

    // Publish the evidence for the payload that was ACCEPTED — not a freshly rebuilt one. ApplyRestoration
    // has already proved Payload.PointIndex == PointIndex (GloamsteadPCGSubsystem.cpp:282-287), so the
    // point index the artifact is correlated with is the point the restoration actually mutated.
    PublishRestorationEvidence(Payload, RequestId);

    // Deliberately NOT `return bPublished`. The restoration above happened and stands; folding the
    // report's outcome into this return value is exactly the rollback this contract forbids.
    return true;
}

void URitualPlacementComponent::SpawnRestoredActor_Implementation(int32 PointIndex, AActor*& OutSpawnedActor)
{
    OutSpawnedActor = nullptr;

    UGloamsteadPCGSubsystem* Subsystem = GetSubsystem();
    UWorld* World = GetWorld();
    if (!Subsystem || !World || PointIndex < 0)
    {
        return;
    }

    FPCGPoint Point;
    if (!Subsystem->GetPointByIndex(PointIndex, Point))
    {
        return;
    }

    const ERitualType RitualType = static_cast<ERitualType>(
        Subsystem->GetIntAttribute(Point, TEXT("RitualType"), static_cast<int32>(ERitualType::LanternPost)));
    if (RitualType != ERitualType::LanternPost)
    {
        return;
    }

    UClass* ClassToSpawn = LanternPostRestoredClass.Get();
    if (!ClassToSpawn && bUseProjectDefaultLanternPostClass)
    {
        ClassToSpawn = StaticLoadClass(AActor::StaticClass(), nullptr,
            TEXT("/Game/Gloamstead/Restoration/FirstLantern/BP_Restored_LanternPost.BP_Restored_LanternPost_C"));
    }
    if (!ClassToSpawn)
    {
        return;
    }

    const FVector TerrainNormal = Subsystem->GetVectorAttribute(Point, TEXT("TerrainNormal"), FVector::UpVector);
    const FVector SpawnLocation = Point.Transform.GetLocation() + TerrainNormal * VerticalOffset;
    const FRotator SpawnRotation = CalculateAlignedRotation(SpawnLocation, TerrainNormal);

    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    OutSpawnedActor = World->SpawnActor<AActor>(ClassToSpawn, SpawnLocation, SpawnRotation, Params);
    if (OutSpawnedActor)
    {
        OutSpawnedActor->Tags.AddUnique(TEXT("Gloamstead.RestoredLantern"));
        OutSpawnedActor->Tags.AddUnique(*FString::Printf(TEXT("Gloamstead.RitualPoint.%d"), PointIndex));
    }
}

void URitualPlacementComponent::PublishRestorationEvidence(
    const FRestorationEventPayload& AppliedPayload, const FString& RequestId)
{
    // From here on the diagnostics describe THIS confirmation, whichever way the publish goes. Set as a
    // block so a HUD can never read a request id from one confirmation next to a path from another.
    LastEvidenceRequestId = RequestId;
    LastEvidencePointIndex = AppliedPayload.PointIndex;
    LastEvidenceReportPath.Reset();
    LastEvidenceFailureCodes.Reset();
    bLastEvidencePublished = false;

    // Read from the payload the restoration was actually performed with, not from a caller's assertion.
    // TWeakObjectPtr::IsValid() also catches the harder case: an actor that DID spawn and then died
    // before publication, which is just as invisible to the player as one that never spawned.
    const bool bRestoredActorMissing = !AppliedPayload.RestoredActor.IsValid();
    bLastRestoredActorMissing = bRestoredActorMissing;
    if (bRestoredActorMissing)
    {
        // Error, not Warning. The restoration is unrecoverable — ApplyRestoration will refuse this point
        // for the rest of the session — so the player has permanently lost it with nothing visible in
        // its place. Error is also the level the automation framework escalates on, which means no
        // harness can certify a green run of the ritual loop while the loop is producing invisible
        // lanterns. That is the intended consequence: a missing class or failed spawn stays red even
        // though the point mutation remains authoritative.
        //
        // Logged here rather than in ConfirmPlacement so it fires on every path that publishes evidence,
        // including the ones that bail out below before an artifact is ever written.
        LastEvidenceFailureCodes.AddUnique(GSSRestoredActorMissing);
        UE_LOG(LogGloamstead, Error,
            TEXT("[GSS016] Point %d was restored but NO restored actor exists (request %s). The point is ")
            TEXT("spent and cannot be restored again, and nothing is visible there. SpawnRestoredActor ")
            TEXT("had no usable class or the configured actor failed to spawn."),
            AppliedPayload.PointIndex, RequestId.IsEmpty() ? TEXT("<unminted>") : *RequestId);
    }

    if (RequestId.IsEmpty())
    {
        // The id is supposed to have been minted before resolution. Handing an empty one to BuildRequest
        // would have it mint its own, and the artifact would then be filed under an id that nothing else
        // in this confirmation — no log line, no getter — ever saw.
        LastEvidenceFailureCodes.AddUnique(TEXT("GSS015"));
        UE_LOG(LogGloamstead, Error,
            TEXT("[GSS015] Point %d was restored and STAYS restored, but no request id was minted for it, ")
            TEXT("so no evidence was published."),
            AppliedPayload.PointIndex);
        return;
    }

    const UWorld* World = GetWorld();
    const UGloamsteadSurveySubjectRegistry* Registry =
        World ? World->GetSubsystem<UGloamsteadSurveySubjectRegistry>() : nullptr;
    if (!Registry)
    {
        LastEvidenceFailureCodes.AddUnique(TEXT("GSS015"));
        UE_LOG(LogGloamstead, Error,
            TEXT("[GSS015] Point %d was restored and STAYS restored, but request %s could not be published: ")
            TEXT("this world has no survey subject registry."),
            AppliedPayload.PointIndex, *RequestId);
        return;
    }

    // Resolve ONCE, under the id minted before this call, and keep the record. Every field the artifact
    // carries — subject id, actor object path, transform, resolved class, map name, map package, world
    // instance id, request id, schema version, producer version, created-at timestamp and failure codes
    // — is stamped into this single struct (GloamsteadSurveySubjectRegistry.cpp:338-380). Nothing below
    // rebuilds or re-resolves it, so no attempt can describe a different actor from the one that was
    // live when the point was restored, or a different world from the one the restoration ran in.
    //
    // Note what is NOT touched here: the PCG subsystem. The registry only reads placed-actor state, so
    // publishing cannot start a second restoration however it fails.
    FGloamsteadSurveyRequest Request = Registry->BuildRequest(EvidenceSubjectId, RequestId);

    // The degraded-success mark, stamped into the record BEFORE the retry loop so the record stays
    // immutable across attempts and the retry rule below still holds.
    //
    // This is the requirement the defect exposed: the artifact must never assert a clean restoration for
    // a lantern that does not exist. GSSValidateRequest derives its verdict from the record's typed
    // fields and never reads FailureCodes (GloamsteadSurveySubjectTypes.cpp:128-166), so an extra code
    // marks the artifact without blocking the write — the evidence of the degradation is exactly what
    // must survive to disk.
    if (bRestoredActorMissing)
    {
        Request.FailureCodes.AddUnique(GSSRestoredActorMissing);
    }

    // RETRY RULE — bounded, idempotent, and stated once, here:
    //
    //   Every attempt publishes THE SAME immutable record. WriteRequest compares bytes against whatever
    //   is already filed under the id, so a repeat is either a no-op that honestly reports success (the
    //   first attempt did land) or a clean retry of a write that left nothing behind
    //   (GloamsteadSurveySubjectTypes.cpp:492-507).
    //
    //   Rebuilding the record between attempts would re-stamp CreatedAt; the bytes would differ and the
    //   retry would be refused as a request-id collision (GSS012). Minting a fresh id would instead file
    //   a SECOND artifact for ONE restoration, which is precisely the duplicate evidence GSS012 exists
    //   to prevent. So: same id, same record, at most GRitualEvidenceEmitAttemptLimit attempts, then
    //   stop and report. Never a loop that runs until it succeeds.
    FString Path;
    TArray<FString> Codes;
    bool bPublished = false;
    for (int32 Attempt = 1; Attempt <= GRitualEvidenceEmitAttemptLimit && !bPublished; ++Attempt)
    {
        bPublished = Registry->EmitRequest(Request, Path, Codes);
    }

    LastEvidenceReportPath = Path;
    LastEvidenceFailureCodes = Codes;
    if (bRestoredActorMissing)
    {
        // Re-applied after the assignment: the publisher's codes describe the WRITE, and would otherwise
        // overwrite the mark that describes the RESTORATION. A HUD needs both.
        LastEvidenceFailureCodes.AddUnique(GSSRestoredActorMissing);
    }
    bLastEvidencePublished = bPublished;

    if (!bPublished)
    {
        // Loud, and only loud. Reporting is downstream of gameplay: the point is mended and stays mended.
        UE_LOG(LogGloamstead, Error,
            TEXT("[GSS] Point %d was restored and STAYS restored, but its evidence (request %s, subject '%s') ")
            TEXT("could not be published after %d attempt(s): %s"),
            AppliedPayload.PointIndex, *RequestId, *EvidenceSubjectId.ToString(),
            GRitualEvidenceEmitAttemptLimit, *FString::Join(Codes, TEXT(",")));
        return;
    }

    // The codes are printed even on the success line, so a reader of the log never sees a publication
    // that looks clean while the artifact it just wrote carries GSS016.
    UE_LOG(LogGloamstead, Log,
        TEXT("[GSS] Point %d restored; evidence for subject '%s' filed under request %s at '%s' ")
        TEXT("(status %s, actor '%s', codes [%s])."),
        AppliedPayload.PointIndex, *EvidenceSubjectId.ToString(), *RequestId, *Path,
        *GSSRequestStatusToken(Request.Status), *Request.ActorObjectPath,
        *FString::Join(Request.FailureCodes, TEXT(",")));
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

    // Outside the index-changed branch on purpose: walking in and out of RestorationRadius flips
    // validity without changing the target index, and the ghost has to appear and vanish with it.
    RefreshPreviewActor();
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
    if (CachedSubsystem)
    {
        return CachedSubsystem;
    }
    if (const UWorld* World = GetWorld())
    {
        return World->GetSubsystem<UGloamsteadPCGSubsystem>();
    }
    return nullptr;
}
