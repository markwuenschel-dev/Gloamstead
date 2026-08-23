#pragma once

#include "CoreMinimal.h"
#include "Data/RitualTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "GloamsteadWorldStateProjectionSubsystem.generated.h"

class UGloamsteadPCGSubsystem;

/**
 * One-way adapter from Gloamstead's authoritative Cycle II restoration state
 * to WorldForge's generic Region state mirror. It owns neither authored plans,
 * PCG state, save data, nor night outcomes; the only state it writes is the
 * WorldForge restoration_level value for Cycle2_Garden.
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadWorldStateProjectionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Recompute the generic WorldForge mirror from the exact immutable Cycle II
	 * PCG target contract. Empty, missing, or ambiguous authoritative state
	 * fails safe to 0.0 and never writes back to Gloamstead.
	 */
	void RebuildFromAuthoritativeState();

	/**
	 * Semantic-only validation for the authored WorldForge input. This parser
	 * never asks WorldForge to generate or materialize and never mutates
	 * gameplay; it makes the JSON schema's cross-field invariants executable in
	 * focused automation.
	 */
	static bool ValidateWorldSpecificationJson(const FString& SpecificationJson, FString* OutError = nullptr);

private:
	UFUNCTION()
	void HandleStructureRestored(const FRestorationEventPayload& Payload);

	void HandleAuthoritativePCGStateRebuilt();
	void BindToAuthoritativePCG();
	void UnbindFromAuthoritativePCG();
	float DetermineCycle2GardenRestorationLevel() const;
	void WriteWorldForgeRestorationLevel(float RestorationLevel) const;

	/** The PCG source currently bound by this subsystem, never an authority sink. */
	TWeakObjectPtr<UGloamsteadPCGSubsystem> BoundPCG;
	FDelegateHandle AuthoritativeStateRebuiltHandle;
};
