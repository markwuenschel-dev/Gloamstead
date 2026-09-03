#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GloamsteadInterpretationSiteBuilder.generated.h"

class AGloamsteadEvidenceSource;
class AGloamsteadReadingChoice;
class UGloamsteadPCGSubsystem;
struct FExperienceCyclePlan;

/**
 * Materializes the authored interpretation furniture: the evidence a warning can be read from, and
 * the choices its second clause can be acted on through.
 *
 * This exists because authoring the data was not enough to make it reachable. Support channels have
 * been fully authored since Cycle II, but AGloamsteadEvidenceSource carries EditInstanceOnly identity
 * fields, so an instance only existed if somebody hand-placed one in the map - and none were. The
 * result was a fair-crypticism contract that the runtime enforced, the tests exercised, and no player
 * could ever satisfy. Spawning from the authored plan closes that gap and keeps it closed: adding a
 * clue to the manifest now puts a clue in the world.
 *
 * Placement is intentionally simple and deterministic - a ring around the point the plan already
 * names. It is a legible default, not a set-dressing system: a level author who hand-places a source
 * with the same identity is respected and nothing is spawned over the top of it.
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadInterpretationSiteBuilder : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Spawns whatever the authored plans need and the map does not already provide.
	 *
	 * Safe to call repeatedly: it is keyed on authored identity, so a second call is a no-op. Returns
	 * how many actors it created.
	 */
	int32 MaterializeAuthoredInterpretationSites();

	/** How many evidence sources this subsystem has spawned in this world. */
	UFUNCTION(BlueprintPure, Category = "Gloamstead|Interpretation")
	int32 GetSpawnedEvidenceCount() const { return SpawnedEvidence.Num(); }

	/** How many reading choices this subsystem has spawned in this world. */
	UFUNCTION(BlueprintPure, Category = "Gloamstead|Interpretation")
	int32 GetSpawnedChoiceCount() const { return SpawnedChoices.Num(); }

	/** Distance from the plan's point at which evidence is placed, in centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Gloamstead|Interpretation", meta = (ClampMin = "0.0"))
	float EvidenceRingRadius = 420.f;

	/** Distance from the plan's point at which reading choices are placed, in centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Gloamstead|Interpretation", meta = (ClampMin = "0.0"))
	float ChoiceRingRadius = 260.f;

private:
	/** True when the map already provides an evidence source with this exact authored identity. */
	bool HasExistingEvidence(FName WarningId, FName SupportId) const;
	bool HasExistingChoice(FName WarningId, FName ReadingId) const;

	/** The location the plan's furniture arranges itself around, or false when the plan has no point. */
	bool ResolvePlanAnchor(const FExperienceCyclePlan& Plan, const UGloamsteadPCGSubsystem* PCG, FVector& OutLocation) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGloamsteadEvidenceSource>> SpawnedEvidence;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGloamsteadReadingChoice>> SpawnedChoices;
};
