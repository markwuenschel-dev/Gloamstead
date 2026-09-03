#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RitualTypes.h"
#include "GloamsteadRitualSiteComponent.generated.h"

/**
 * The shipping authority for a ritual point's semantic contract.
 *
 * An actor carrying this component DECLARES that the place it stands on is a specific authored ritual
 * site: which semantic subject it is, which warning recommends it, which ritual restores it, and which
 * restoration tag it carries. UGloamsteadPCGSubsystem reads that declaration during initialization and
 * stamps it onto the nearest generated point of the declared ritual type.
 *
 * Why this exists: UGloamsteadPCGSubsystem::PointMatchesExperiencePlan requires all four of
 * RecommendedForWarning / SemanticSubject / RitualType / RestorationTag to agree with the authored plan
 * before a night can act on a point. Until this component existed, the ONLY writer of those attributes
 * was Test_SetPointContractMetadata, which lives inside WITH_DEV_AUTOMATION_TESTS and is compiled out of
 * a player build - so every cycle that resolves its target semantically could pass its tests and still
 * find nothing in a real game.
 *
 * Content declares; only C++ writes. This component exposes no metadata setter of any kind, because
 * FairCrypticismTests asserts by reflection that no Blueprint route to PCG metadata may exist. The
 * author places an actor and fills in properties; the subsystem does the writing.
 *
 * Fail-closed: an incomplete declaration is refused and reported, never partially applied. A declared
 * site that matches no generated point within BindRadius is an authoring error and says so - it does
 * not silently leave the point at NAME_None, which is the failure mode this component was built to end.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GLOAMSTEAD_API UGloamsteadRitualSiteComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGloamsteadRitualSiteComponent();

	/** Stable semantic name for this place, e.g. "courtyard.lantern.first". Must match the authored plan. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual Site")
	FName SemanticSubject;

	/** The authored warning that recommends this place, e.g. "GardenRot". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual Site")
	FName RecommendedForWarning;

	/** Which ritual restores this place. Also selects which generated points are eligible to bind. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual Site")
	ERitualType RitualType = ERitualType::Invalid;

	/** The restoration tag this place carries when restored, e.g. "Gloamstead.RestoredLantern". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual Site")
	FName RestorationTag;

	/**
	 * Maximum distance from this actor to an eligible generated point, in centimetres. A declaration
	 * that binds nothing inside this radius is reported as an authoring error rather than silently
	 * binding something far away that the author never meant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual Site", meta = (ClampMin = "1.0"))
	double BindRadius = 2000.0;

	/**
	 * True only when every field of the contract is authored. Fills OutProblems with one actionable
	 * line per missing field, naming the owning actor so a level author can find it.
	 */
	bool IsCompleteDeclaration(TArray<FString>& OutProblems) const;
};
