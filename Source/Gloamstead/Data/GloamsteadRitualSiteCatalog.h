#pragma once

#include "CoreMinimal.h"
#include "Data/RitualTypes.h"
#include "Engine/DataAsset.h"
#include "GloamsteadRitualSiteCatalog.generated.h"

/**
 * Where an authored ritual site anchors itself in the level.
 *
 * A site declares a PLACE, and a place in Gloamstead is described relative to the sanctuary's own
 * landmarks, not as a raw coordinate. Anchoring this way means the declaration keeps meaning if the
 * courtyard is re-authored: "around the Veil Heart" stays true when the Heart moves, where a literal
 * transform would quietly point at empty ground.
 */
UENUM(BlueprintType)
enum class EGloamsteadSiteAnchor : uint8
{
	/** The Veil Heart - the sanctuary's centre and the player's orientation anchor. */
	SanctuaryHeart UMETA(DisplayName = "Sanctuary Heart"),

	/** The authored first-lantern anchor actor, if the map places one. */
	FirstLantern UMETA(DisplayName = "First Lantern"),

	/** The world origin. Use only when a site genuinely has no landmark to describe it. */
	WorldOrigin UMETA(DisplayName = "World Origin")
};

/**
 * One authored ritual site, declared as CONTENT rather than as a placed actor.
 *
 * UGloamsteadRitualSiteComponent covers the case where a level author drops a marker in the map. This
 * covers the case where the place is described relative to a landmark - which is how the sanctuary is
 * actually authored, and which can be written into the manifest without opening the editor.
 *
 * Both routes converge on the same C++ writer: content declares, only C++ stamps PCG metadata.
 */
USTRUCT(BlueprintType)
struct GLOAMSTEAD_API FGloamsteadRitualSiteDeclaration
{
	GENERATED_BODY()

	/** Stable semantic name for this place, e.g. "Cycle2_Garden". Must match the authored plan. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ritual Site")
	FName SemanticSubject;

	/** The authored warning that recommends this place, e.g. "GardenRot". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ritual Site")
	FName RecommendedForWarning;

	/** Which ritual restores this place. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ritual Site")
	ERitualType RitualType = ERitualType::Invalid;

	/** The restoration tag this place carries once restored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ritual Site")
	FName RestorationTag;

	/** The landmark this place is described relative to. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ritual Site")
	EGloamsteadSiteAnchor Anchor = EGloamsteadSiteAnchor::SanctuaryHeart;

	/**
	 * How far from the anchor an eligible generated point may be, in centimetres. A declaration that
	 * binds nothing inside this radius is reported as an authoring error rather than binding something
	 * far away nobody meant.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ritual Site", meta = (ClampMin = "1.0"))
	double BindRadius = 4000.0;

	/**
	 * Smallest distance from the anchor an eligible point may be. Keeps a site that surrounds a landmark
	 * from binding the landmark's own point - the garden lies AROUND the Heart, not on top of it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ritual Site", meta = (ClampMin = "0.0"))
	double MinimumAnchorDistance = 0.0;

	bool IsCompleteDeclaration(TArray<FString>& OutProblems) const;
};

/** Authored ritual sites for a map, shipped as content. */
UCLASS(BlueprintType)
class GLOAMSTEAD_API UGloamsteadRitualSiteCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ritual Site")
	TArray<FGloamsteadRitualSiteDeclaration> Sites;
};
