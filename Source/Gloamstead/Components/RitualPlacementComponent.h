#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PCGPointData.h"
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
    void OnPlacementModeExited();

    UFUNCTION(BlueprintImplementableEvent, Category="Ritual|Placement")
    void SpawnRestoredActor(int32 PointIndex, AActor*& OutSpawnedActor);

protected:
    void UpdateTargetPoint();
    bool IsPointValidForPlacement(int32 PointIndex) const;
    int32 ResolveTargetForPlacement(int32 RawPointIndex);
    void BuildRestorationPayload(int32 PointIndex, AActor* SpawnedRestoredActor, FRestorationEventPayload& OutPayload) const;
    FRotator CalculateAlignedRotation(const FVector& Location, const FVector& TerrainNormal) const;

    const class URitualDefinition* GetRitualDefinitionForType(ERitualType Type) const;

    class UGloamsteadPCGSubsystem* GetSubsystem() const;

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

    UPROPERTY(EditDefaultsOnly, Category="Ritual Definitions")
    TMap<ERitualType, TObjectPtr<URitualDefinition>> RitualDefinitions;
};