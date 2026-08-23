#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PCGPointData.h"
#include "Data/RitualTypes.h"
#include "Data/RitualDefinition.h"
#include "RitualPlacementComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GLOAMSTEAD_API URitualPlacementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URitualPlacementComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    // === Public API (Blueprint Callable) ===
    UFUNCTION(BlueprintCallable, Category="Ritual|Placement")
    void EnterPlacementMode();

    UFUNCTION(BlueprintCallable, Category="Ritual|Placement")
    void ExitPlacementMode();

    UFUNCTION(BlueprintCallable, Category="Ritual|Placement")
    bool ConfirmPlacement();

    UFUNCTION(BlueprintCallable, Category="Ritual|Placement")
    void ForceUpdatePreview();

    // === Blueprint Pure Getters ===
    UFUNCTION(BlueprintPure, Category="Ritual|Placement")
    bool IsInPlacementMode() const { return bIsInPlacementMode; }

    UFUNCTION(BlueprintPure, Category="Ritual|Placement")
    bool IsCurrentPlacementValid() const;

    UFUNCTION(BlueprintPure, Category="Ritual|Placement")
    ERitualType GetCurrentTargetRitualType() const;

    UFUNCTION(BlueprintPure, Category="Ritual|Placement")
    FRitualPointInfo GetCurrentTargetPointInfo() const;

    UFUNCTION(BlueprintPure, Category="Ritual|Placement")
    bool GetCurrentTargetTransform(FVector& OutLocation, FRotator& OutRotation) const;

    // === Confirmation evidence: read-only diagnostics for a HUD ===
    //
    // These five describe ONE event: the most recent confirmation that actually restored a point. A
    // confirmation that was refused before restoring never touches them, so the five always agree with
    // each other and a HUD can render them as a single snapshot instead of a mix of two attempts.
    //
    // They live here, on the component, precisely so a HUD never has to reach into
    // UGloamsteadSurveySubjectRegistry. The registry deliberately exposes no UFUNCTIONs (it is plain
    // C++ by design), and adding some to make a HUD convenient would put a reporting concern inside a
    // system whose whole contract is that it takes no authority and answers only what it is asked.

    /** Request id the last restored confirmation was filed under. Empty until one has happened. */
    UFUNCTION(BlueprintPure, Category="Ritual|Evidence")
    FString GetLastEvidenceRequestId() const { return LastEvidenceRequestId; }

    /**
     * Path the artifact was filed under. On a refused publish this is still the path the request id maps
     * to (the publisher resolves it before it decides), so pair it with WasLastEvidencePublished() before
     * telling anyone a file is there. Empty only when publication never got as far as choosing a path.
     */
    UFUNCTION(BlueprintPure, Category="Ritual|Evidence")
    FString GetLastEvidenceReportPath() const { return LastEvidenceReportPath; }

    /** GSS codes from that emission. Empty on a clean publish. */
    UFUNCTION(BlueprintPure, Category="Ritual|Evidence")
    TArray<FString> GetLastEvidenceFailureCodes() const { return LastEvidenceFailureCodes; }

    /** True only when the artifact is on disk and validated. Distinguishes "published" from "never ran". */
    UFUNCTION(BlueprintPure, Category="Ritual|Evidence")
    bool WasLastEvidencePublished() const { return bLastEvidencePublished; }

    /** The ritual point that confirmation restored — the correlation key between gameplay and evidence. */
    UFUNCTION(BlueprintPure, Category="Ritual|Evidence")
    int32 GetLastEvidencePointIndex() const { return LastEvidencePointIndex; }

    /**
     * True when the last confirmation restored a point but no restored actor materialised — a DEGRADED
     * success. The point is spent and can never be retried, yet the player can see nothing there. A HUD
     * must not render this the same as a clean restoration; string-matching GSS016 out of
     * GetLastEvidenceFailureCodes() is not a HUD's job, so it is surfaced as its own answer.
     */
    UFUNCTION(BlueprintPure, Category="Ritual|Evidence")
    bool WasLastRestoredActorMissing() const { return bLastRestoredActorMissing; }

    /**
     * GSS016 — RESTORATION COMPLETED WITH NO RESTORED ACTOR.
     *
     * Stamped into the emitted request artifact's failure_codes whenever a restoration succeeded while
     * FRestorationEventPayload::RestoredActor was null or already dead. It says: the gameplay state
     * changed (the point is spent, light and corruption moved, and ApplyRestoration will refuse it
     * forever after) but nothing was placed in the world for the player to see.
     *
     * NOTE — this code is declared HERE, not in the canonical GSS list at
     * GloamsteadSurveySubjectTypes.h:195-211, because that file is outside this change's envelope. It
     * needs promoting into that list before anything else claims GSS016. Nothing in the survey system
     * validates against the list, so an unregistered code is emitted and read correctly today; the risk
     * is a future collision, not a malformed artifact.
     *
     * It is deliberately NOT expressed by forcing the request's Status to Unresolved. The survey subject
     * and the spawned actor are different things: the place-name may have resolved perfectly. Lying about
     * the resolution to signal a spawn failure would corrupt the one field the whole registry exists to
     * answer.
     */
    static const FString GSSRestoredActorMissing;

#if WITH_DEV_AUTOMATION_TESTS
    // === Automation-only test seam ===
    /**
     * Test seam: run the confirm path's restore-then-publish tail against explicit collaborators.
     * This is the SAME function ConfirmPlacement calls, not a parallel copy. Reaching it through
     * ConfirmPlacement needs a preview target, and that needs UGloamsteadPCGSubsystem's spatial grid,
     * which automation-only synthetic metadata supplies. This declaration does
     * not exist in shipping, so it cannot become a generic placement authority.
     */
    bool Test_CommitRestorationWithEvidence(class UGloamsteadPCGSubsystem* Subsystem, int32 PointIndex,
        const FRestorationEventPayload& Payload, const FString& RequestId)
    {
        return CommitRestorationWithEvidence(Subsystem, PointIndex, Payload, RequestId);
    }
#endif

    // === Events for Blueprint Child ===
    UFUNCTION(BlueprintImplementableEvent, Category="Ritual|Placement")
    void OnPreviewTargetChanged(int32 PointIndex, ERitualType Type, bool bIsValid);

    UFUNCTION(BlueprintImplementableEvent, Category="Ritual|Placement")
    void OnPathPointRedirected(const FString& Message);

    UFUNCTION(BlueprintImplementableEvent, Category="Ritual|Placement")
    void OnPlacementConfirmed(int32 PointIndex);

    UFUNCTION(BlueprintImplementableEvent, Category="Ritual|Placement")
    void OnRestoredActorSpawned(AActor* SpawnedActor, int32 PointIndex, ERitualType RitualType);

    UFUNCTION(BlueprintImplementableEvent, Category="Ritual|Placement")
    void OnPlacementModeExited();

    /** Optional per-character override. When unset, the project-owned first-lantern Blueprint is loaded. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ritual|Placement")
    TSubclassOf<AActor> LanternPostRestoredClass;

    /** Keep enabled for the playable slice. Tests and specialized characters may disable the project fallback. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ritual|Placement", meta=(AdvancedDisplay))
    bool bUseProjectDefaultLanternPostClass = true;

    UFUNCTION(BlueprintNativeEvent, Category="Ritual|Placement")
    void SpawnRestoredActor(int32 PointIndex, AActor*& OutSpawnedActor);
    virtual void SpawnRestoredActor_Implementation(int32 PointIndex, AActor*& OutSpawnedActor);

    // === Ghost preview ===
    //
    // The preview is owned HERE rather than in a Blueprint child, because "exactly one preview" is a
    // lifecycle invariant, not a presentation detail: cancel must remove it and re-entry must create
    // one, and only the component knows every path that ends placement mode. OnPreviewTargetChanged
    // still fires for Blueprints that want to add their own flourish on top.

    /** Optional per-character override. When unset, the project-owned preview Blueprint is loaded. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ritual|Placement|Preview")
    TSubclassOf<AActor> PreviewActorClass;

    /** Keep enabled for the playable slice. Tests and specialized characters may disable the fallback. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ritual|Placement|Preview", meta=(AdvancedDisplay))
    bool bUseProjectDefaultPreviewClass = true;

    /** The live ghost, or null when placement mode is closed or no valid target is in range. */
    UFUNCTION(BlueprintPure, Category="Ritual|Placement|Preview")
    AActor* GetActivePreviewActor() const { return ActivePreviewActor.Get(); }

protected:
    void UpdateTargetPoint();
    bool IsPointValidForPlacement(int32 PointIndex) const;
    int32 ResolveTargetForPlacement(int32 RawPointIndex);

    /** Spawns, moves, or removes the ghost so it always matches the current valid target. */
    void RefreshPreviewActor();
    /** Idempotent teardown. Every path that leaves placement mode ends here. */
    void DestroyPreviewActor();
    /** Resolves PreviewActorClass, falling back to the project preview Blueprint. */
    UClass* ResolvePreviewClass() const;
    /** Fill OutPayload for PointIndex. Returns false (leaving OutPayload at defaults) when the point
     *  cannot be resolved; a true return guarantees OutPayload.PointIndex == PointIndex, which is what
     *  ApplyRestoration checks. Callers must not proceed on a false return. */
    bool BuildRestorationPayload(int32 PointIndex, AActor* SpawnedRestoredActor, FRestorationEventPayload& OutPayload) const;
    FRotator CalculateAlignedRotation(const FVector& Location, const FVector& TerrainNormal) const;

    /**
     * Publish one request-bound survey artifact for a restoration that has ALREADY been applied.
     * Reads world state only; it never touches UGloamsteadPCGSubsystem, so it cannot start a second
     * restoration. Records the outcome in the LastEvidence* fields either way.
     */
    void PublishRestorationEvidence(const FRestorationEventPayload& AppliedPayload, const FString& RequestId);

    const class URitualDefinition* GetRitualDefinitionForType(ERitualType Type) const;

    /** Fill any unassigned RitualDefinitions slot from the DA_Ritual_* assets in /Game/Data.
     *  Editor-assigned entries win; types whose asset fails to load fall back to the RitualTypes.cpp
     *  defaults at payload-build time. */
    void EnsureRitualDefinitionsLoaded();

    class UGloamsteadPCGSubsystem* GetSubsystem() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ritual Definitions")
    TMap<ERitualType, TObjectPtr<URitualDefinition>> RitualDefinitions;

private:
    /**
     * The only runtime path that may emit PCG's native placement-authorized
     * completion event. ConfirmPlacement reaches it only after target and
     * ritual validation; the automation wrapper above disappears from shipping.
     */
    bool CommitRestorationWithEvidence(class UGloamsteadPCGSubsystem* Subsystem, int32 PointIndex,
        const FRestorationEventPayload& Payload, const FString& RequestId);

    UPROPERTY()
    TObjectPtr<class UGloamsteadPCGSubsystem> CachedSubsystem;

    int32 CurrentTargetPointIndex = -1;
    ERitualType CurrentMode = ERitualType::LanternPost;
    bool bIsInPlacementMode = false;

    /** Weak: the ghost is a world actor and may be destroyed by level teardown out from under us. */
    UPROPERTY()
    TWeakObjectPtr<AActor> ActivePreviewActor;

    // Optimization
    FVector LastQueryLocation = FVector::ZeroVector;
    float TimeSinceLastQuery = 0.0f;

    // PathPoint first-time messaging
    bool bHasShownPathPointMessageThisSession = false;

    // Confirmation evidence — see the getters above for what these describe.
    FString LastEvidenceRequestId;
    FString LastEvidenceReportPath;
    TArray<FString> LastEvidenceFailureCodes;
    bool bLastEvidencePublished = false;
    bool bLastRestoredActorMissing = false;
    int32 LastEvidencePointIndex = -1;

    // Config
    UPROPERTY(EditDefaultsOnly, Category="Placement Settings")
    float QueryUpdateInterval = 0.15f;

    UPROPERTY(EditDefaultsOnly, Category="Placement Settings")
    float QueryMovementThreshold = 75.0f;

    UPROPERTY(EditDefaultsOnly, Category="Placement Settings")
    float VerticalOffset = 12.0f;

    UPROPERTY(EditDefaultsOnly, Category="Placement Settings")
    float SteepSlopeCameraBias = 28.0f;

    /**
     * The survey place-name a confirmation from this component is evidence about. Assigned in the
     * constructor to "courtyard.lantern.first"; it is a declared subject
     * (GloamsteadSurveySubjectRegistry.cpp:52-61) whose only resolver is a map actor carrying a
     * UGloamsteadSurveySubjectComponent. Until that actor exists the request is published honestly
     * unresolved — which is the point: the artifact records what the world actually said.
     */
    UPROPERTY(EditDefaultsOnly, Category="Ritual|Evidence")
    FName EvidenceSubjectId;
};
