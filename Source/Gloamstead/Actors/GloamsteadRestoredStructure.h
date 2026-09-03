#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RitualTypes.h"
#include "GloamsteadRestoredStructure.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USpotLightComponent;

/**
 * How one ritual form looks once it has been restored.
 *
 * Deliberately a plain struct of asset paths rather than a data asset: these are the code-owned
 * fallbacks that guarantee a restoration is never invisible, and a fallback that can be emptied by
 * editing content is not a fallback. Designers override per-type by setting the placement
 * component's explicit class properties, which still win.
 */
struct FGloamRestoredStructureRecipe
{
	/** The body of the structure - always present, or the recipe is not a recipe. */
	const TCHAR* BodyMeshPath = nullptr;
	const TCHAR* BodyMaterialPath = nullptr;
	FVector BodyScale = FVector(1.f);

	/** Optional second piece sitting above the body: a bell, a mirror face, a bound shard. */
	const TCHAR* CrownMeshPath = nullptr;
	const TCHAR* CrownMaterialPath = nullptr;
	FVector CrownScale = FVector(1.f);

	/**
	 * Where the crown and the light sit, as a fraction of the body's own scaled height.
	 *
	 * Measured from the body mesh's real bounds at configure time rather than authored as a literal
	 * Z. The sanctuary kit's pivots are not uniform - the three prototyping meshes alone use three
	 * different conventions - so any hand-written offset is a guess that reads back exactly as
	 * authored while hanging the bell inside the arch or a metre above it.
	 */
	float CrownHeightFraction = 1.f;
	float LightHeightFraction = 0.75f;

	/**
	 * Every restoration in this game is finally about light, so every restored structure carries
	 * some. This is presentation - the sanctuary's *gameplay* light lives in the PCG point state -
	 * but a "restored light" the player cannot see is the exact failure this class exists to end.
	 */
	FLinearColor LightColor = FLinearColor(1.f, 0.78f, 0.48f);
	float LightIntensity = 8.f;
	float LightRadius = 900.f;

	/**
	 * True for the mirror pillar alone: its beam points along the actor's forward axis, because
	 * *which way it faces* is the whole Cycle IV question. A point light would answer it for free.
	 */
	bool bAimsLightForward = false;

	/** Tag the placement contract stamps on the spawned actor so later systems can find it. */
	const TCHAR* RestorationTag = nullptr;
};

/**
 * Looks up the code-owned restored-structure recipe for a ritual form.
 *
 * Returns false for LanternPost and GardenBed, which have their own dedicated actors, and for
 * Invalid. Every other authored ERitualType must return true - see the automation test
 * Gloamstead.Restoration.EveryAuthoredRitualFormHasAVisibleRestoration, which is the reason a new
 * ritual form cannot be added without also deciding what restoring it looks like.
 */
GLOAMSTEAD_API bool GetRestoredStructureRecipe(ERitualType RitualType, FGloamRestoredStructureRecipe& OutRecipe);

/**
 * Code-owned fallback body for the ritual forms of Cycles III-VI.
 *
 * Cycles III, IV, V and VI shipped with authored plans, authored warnings, authored evidence and
 * authored night rules - and no restored actor of any kind. Restoring a road, a mirror, a bell or an
 * anchor consumed its PCG point, advanced the cycle, and changed nothing the player could see;
 * URitualPlacementComponent logged "ritual type N has no restored-actor contract" and spawned
 * nothing. Four cycles of a six-cycle game were therefore invisible.
 *
 * This actor closes that with the sanctuary kit that already ships. It is one class rather than four
 * because the difference between the forms is a handful of values, and holding them in one readable
 * table is how a fifth form gets noticed at review time instead of at playtest.
 *
 * Configure it immediately after spawn with ConfigureForRitualType; it does no work before that, so
 * an unconfigured one is visibly empty rather than quietly wrong.
 */
UCLASS(Blueprintable)
class GLOAMSTEAD_API AGloamsteadRestoredStructure : public AActor
{
	GENERATED_BODY()

public:
	AGloamsteadRestoredStructure();

	/**
	 * Builds this structure into the given ritual form. Safe to call once; calling it with a form
	 * that has no recipe leaves the actor empty and returns false rather than guessing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gloamstead|Restoration")
	bool ConfigureForRitualType(ERitualType RitualType);

	/** The form this structure was built as, or Invalid while unconfigured. */
	UFUNCTION(BlueprintPure, Category = "Gloamstead|Restoration")
	ERitualType GetRitualType() const { return ConfiguredRitualType; }

	/** True once this structure has a renderable project-owned body. */
	UFUNCTION(BlueprintPure, Category = "Gloamstead|Restoration")
	bool HasVisibleBody() const;

	/** True once this structure is casting the light its recipe asked for. */
	UFUNCTION(BlueprintPure, Category = "Gloamstead|Restoration")
	bool HasRestoredLight() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gloamstead|Restoration")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gloamstead|Restoration")
	TObjectPtr<UStaticMeshComponent> CrownMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gloamstead|Restoration")
	TObjectPtr<UPointLightComponent> RestoredLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gloamstead|Restoration")
	TObjectPtr<USpotLightComponent> AimedLight;

private:
	UPROPERTY(VisibleAnywhere, Category = "Gloamstead|Restoration")
	ERitualType ConfiguredRitualType = ERitualType::Invalid;
};
