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

    UFUNCTION(BlueprintImplementableEvent, Category="Ritual|Placement")
    void SpawnRestoredActor(int32 PointIndex, AActor*& OutSpawnedActor);

protected:
    void UpdateTargetPoint();
    bool IsPointValidForPlacement(int32 PointIndex) const;
    int32 ResolveTargetForPlacement(int32 RawPointIndex);
    /** Fill OutPayload for PointIndex. Returns false (leaving OutPayload at defaults) when the point
     *  cannot be resolved; a true return guarantees OutPayload.PointIndex == PointIndex, which is what
     *  ApplyRestoration checks. Callers must not proceed on a false return. */
    bool BuildRestorationPayload(int32 PointIndex, AActor* SpawnedRestoredActor, FRestorationEventPayload& OutPayload) const;
    FRotator CalculateAlignedRotation(const FVector& Location, const FVector& TerrainNormal) const;

    const class URitualDefinition* GetRitualDefinitionForType(ERitualType Type) const;

    /** Fill any unassigned RitualDefinitions slot from the DA_Ritual_* assets in /Game/Data.
     *  Editor-assigned entries win; types whose asset fails to load fall back to the RitualTypes.cpp
     *  defaults at payload-build time. */
    void EnsureRitualDefinitionsLoaded();

    class UGloamsteadPCGSubsystem* GetSubsystem() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ritual Definitions")
    TMap<ERitualType, TObjectPtr<URitualDefinition>> RitualDefinitions;

private:
    UPROPERTY()
    TObjectPtr<class UGloamsteadPCGSubsystem> CachedSubsystem;

    int32 CurrentTargetPointIndex = -1;
    ERitualType CurrentMode = ERitualType::LanternPost;
    bool bIsInPlacementMode = false;

    // Optimization
    FVector LastQueryLocation = FVector::ZeroVector;
    float TimeSinceLastQuery = 0.0f;

    // PathPoint first-time messaging
    bool bHasShownPathPointMessageThisSession = false;

    // Config
    UPROPERTY(EditDefaultsOnly, Category="Placement Settings")
    float QueryUpdateInterval = 0.15f;

    UPROPERTY(EditDefaultsOnly, Category="Placement Settings")
    float QueryMovementThreshold = 75.0f;

    UPROPERTY(EditDefaultsOnly, Category="Placement Settings")
    float VerticalOffset = 12.0f;

    UPROPERTY(EditDefaultsOnly, Category="Placement Settings")
    float SteepSlopeCameraBias = 28.0f;
};