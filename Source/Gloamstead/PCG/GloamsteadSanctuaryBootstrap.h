#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GloamsteadSanctuaryBootstrap.generated.h"

class UBoxComponent;
class UPCGComponent;
class USceneComponent;

/**
 * Owns the sanctuary PCG graph and hands its generated points to the runtime PCG subsystem.
 */
UCLASS(BlueprintType, Blueprintable)
class GLOAMSTEAD_API AGloamsteadSanctuaryBootstrap : public AActor
{
	GENERATED_BODY()

public:
	AGloamsteadSanctuaryBootstrap();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Gloamstead|PCG")
	bool TryInitializeSanctuary();

	UFUNCTION(BlueprintPure, Category = "Gloamstead|PCG")
	bool HasInitializedSanctuary() const { return bInitializedSanctuary; }

	UFUNCTION(BlueprintPure, Category = "Gloamstead|PCG")
	UPCGComponent* GetPCGComponent() const { return PCGComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gloamstead|PCG")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Supplies spatial bounds so the PCGComponent registers with the PCG scheduler; without a
	 *  bounds source PCG logs "Component has invalid bounds, not registered" and emits zero points. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gloamstead|PCG")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gloamstead|PCG")
	TObjectPtr<UPCGComponent> PCGComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gloamstead|PCG")
	int32 WorldSeed = 42;

private:
	void BindToPCGComponent();
	void UnbindFromPCGComponent();
	void HandlePCGGraphGenerated(UPCGComponent* GeneratedComponent);
	bool HasGeneratedOutput() const;

	UPROPERTY(Transient)
	bool bInitializedSanctuary = false;
};
