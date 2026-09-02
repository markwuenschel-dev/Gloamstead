#include "Actors/GloamsteadRestoredStructure.h"

#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

/**
 * Named, not anonymous, deliberately. This module builds through UBT's adaptive unity, so a
 * file-local helper in an anonymous namespace collides with an identically-named one in whatever
 * other translation unit lands in the same unity blob - which has already shipped one broken commit
 * here (C2264 on R4/VectorJson/WriteJson). A named namespace costs nothing and cannot do that.
 */
namespace GloamsteadRestoredStructureRecipes
{
	// Every path below is a tracked, project-owned sanctuary kit asset that ships today. Nothing
	// here loads engine sample content or an asset the forge has yet to produce.
	constexpr const TCHAR* SlabB      = TEXT("/Game/Gloamstead/Kit/Meshes/SM_Slab_Floor_B.SM_Slab_Floor_B");
	constexpr const TCHAR* ColumnTall = TEXT("/Game/Gloamstead/Kit/Meshes/SM_Column_Intact.SM_Column_Intact");
	constexpr const TCHAR* Arch       = TEXT("/Game/Gloamstead/Kit/Meshes/SM_Gateway_Arch.SM_Gateway_Arch");
	constexpr const TCHAR* HeartCore  = TEXT("/Game/Gloamstead/Kit/Meshes/SM_Heart_Core.SM_Heart_Core");
	constexpr const TCHAR* Plinth     = TEXT("/Game/Gloamstead/Kit/Meshes/SM_Heart_Plinth.SM_Heart_Plinth");
	constexpr const TCHAR* Shard      = TEXT("/Game/Gloamstead/Kit/Meshes/SM_Heart_Shard_1.SM_Heart_Shard_1");

	constexpr const TCHAR* MatPaving    = TEXT("/Game/Gloamstead/Kit/Materials/MI_Sanctuary_Paving.MI_Sanctuary_Paving");
	constexpr const TCHAR* MatIron      = TEXT("/Game/Gloamstead/Kit/Materials/MI_Sanctuary_Iron.MI_Sanctuary_Iron");
	constexpr const TCHAR* MatStone     = TEXT("/Game/Gloamstead/Kit/Materials/MI_Sanctuary_Stone.MI_Sanctuary_Stone");
	constexpr const TCHAR* MatStoneDark = TEXT("/Game/Gloamstead/Kit/Materials/MI_Sanctuary_Stone_Dark.MI_Sanctuary_Stone_Dark");
	constexpr const TCHAR* MatHeartCore = TEXT("/Game/Gloamstead/Materials/MI_VeilHeart_Core.MI_VeilHeart_Core");

	FGloamRestoredStructureRecipe MakePathPoint()
	{
		// Cycle III mends the road. A mended path point is a laid paving stone that carries light
		// along the segment, so it reads low and wide and its lamp is deliberately modest - a road
		// is a chain of small lights, and one that outshone a lantern post would misteach that.
		FGloamRestoredStructureRecipe R;
		R.BodyMeshPath = SlabB;
		R.BodyMaterialPath = MatPaving;
		R.BodyScale = FVector(1.15f, 1.15f, 0.22f);
		R.LightColor = FLinearColor(1.f, 0.80f, 0.52f);
		R.LightIntensity = 2200.f;
		R.LightRadius = 750.f;
		R.LightHeightFraction = 2.4f;
		R.RestorationTag = TEXT("Gloamstead.RestoredPath");
		return R;
	}

	FGloamRestoredStructureRecipe MakeMirrorPillar()
	{
		// Cycle IV asks which way the mirror faces, so the mirror must be a thing with a face. The
		// crown is a thin plate stood on edge across the pillar's forward axis, and the beam runs
		// along that same axis: where the pillar looks is visible from across the plaza, which is
		// the only way "face it at the stolen light, never at the Heart" can be a fair question.
		FGloamRestoredStructureRecipe R;
		R.BodyMeshPath = ColumnTall;
		R.BodyMaterialPath = MatIron;
		R.BodyScale = FVector(0.85f, 0.85f, 1.f);
		R.CrownMeshPath = SlabB;
		R.CrownMaterialPath = MatIron;
		R.CrownScale = FVector(0.06f, 0.55f, 0.55f);
		R.CrownHeightFraction = 1.02f;
		R.LightColor = FLinearColor(0.78f, 0.86f, 1.f);
		R.LightIntensity = 5200.f;
		R.LightRadius = 2600.f;
		R.LightHeightFraction = 1.f;
		R.bAimsLightForward = true;
		R.RestorationTag = TEXT("Gloamstead.RestoredMirror");
		return R;
	}

	FGloamRestoredStructureRecipe MakeBellShrine()
	{
		// Cycle V is about restraint - one answer frees, three invite company. The bell hangs in
		// the arch's mouth so the shape says "this is struck", and its light is warm and small
		// because the bell's power is sound, not illumination.
		FGloamRestoredStructureRecipe R;
		R.BodyMeshPath = Arch;
		R.BodyMaterialPath = MatStone;
		R.BodyScale = FVector(0.75f, 0.75f, 0.75f);
		R.CrownMeshPath = HeartCore;
		R.CrownMaterialPath = MatIron;
		R.CrownScale = FVector(0.42f, 0.42f, 0.52f);
		R.CrownHeightFraction = 0.72f;
		R.LightColor = FLinearColor(1.f, 0.72f, 0.40f);
		R.LightIntensity = 3000.f;
		R.LightRadius = 1100.f;
		R.LightHeightFraction = 0.66f;
		R.RestorationTag = TEXT("Gloamstead.RestoredBell");
		return R;
	}

	FGloamRestoredStructureRecipe MakeAnchorStone()
	{
		// Cycle VI binds lights the player already raised. An anchor stone is therefore made of the
		// Heart's own parts - a plinth carrying a shard in the Heart's core material - so that a
		// closed ring of three reads on sight as three pieces of one thing rather than three more
		// lamps. It is the brightest and furthest-reaching recipe because reach is the mechanic.
		FGloamRestoredStructureRecipe R;
		R.BodyMeshPath = Plinth;
		R.BodyMaterialPath = MatStoneDark;
		R.BodyScale = FVector(0.8f, 0.8f, 0.8f);
		R.CrownMeshPath = Shard;
		R.CrownMaterialPath = MatHeartCore;
		R.CrownScale = FVector(0.55f, 0.55f, 0.55f);
		R.CrownHeightFraction = 1.f;
		R.LightColor = FLinearColor(0.88f, 0.90f, 1.f);
		R.LightIntensity = 6000.f;
		R.LightRadius = 1900.f;
		R.LightHeightFraction = 1.15f;
		R.RestorationTag = TEXT("Gloamstead.RestoredAnchor");
		return R;
	}
}

bool GetRestoredStructureRecipe(ERitualType RitualType, FGloamRestoredStructureRecipe& OutRecipe)
{
	using namespace GloamsteadRestoredStructureRecipes;

	switch (RitualType)
	{
	case ERitualType::PathPoint:    OutRecipe = MakePathPoint();    return true;
	case ERitualType::MirrorPillar: OutRecipe = MakeMirrorPillar(); return true;
	case ERitualType::BellShrine:   OutRecipe = MakeBellShrine();   return true;
	case ERitualType::AnchorStone:  OutRecipe = MakeAnchorStone();  return true;

	// LanternPost and GardenBed own dedicated actors that predate this class and carry art this
	// generic body cannot: the lantern's Niagara motes and the garden's bed surface. Returning
	// false here is the contract, not an omission - see the placement component's switch.
	case ERitualType::LanternPost:
	case ERitualType::GardenBed:
	case ERitualType::Invalid:
	default:
		return false;
	}
}

AGloamsteadRestoredStructure::AGloamsteadRestoredStructure()
{
	PrimaryActorTick.bCanEverTick = false;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	SetRootComponent(BodyMesh);
	BodyMesh->SetMobility(EComponentMobility::Movable);
	// Query-only, matching the garden bed: a restoration must never be able to push the player out
	// of the world or block the pawn it just appeared under.
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	CrownMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrownMesh"));
	CrownMesh->SetupAttachment(BodyMesh);
	CrownMesh->SetMobility(EComponentMobility::Movable);
	CrownMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CrownMesh->SetVisibility(false);

	RestoredLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RestoredLight"));
	RestoredLight->SetupAttachment(BodyMesh);
	RestoredLight->SetMobility(EComponentMobility::Movable);
	RestoredLight->SetVisibility(false);
	RestoredLight->SetCastShadows(false);

	AimedLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("AimedLight"));
	AimedLight->SetupAttachment(BodyMesh);
	AimedLight->SetMobility(EComponentMobility::Movable);
	AimedLight->SetVisibility(false);
	AimedLight->SetCastShadows(false);
	AimedLight->SetInnerConeAngle(16.f);
	AimedLight->SetOuterConeAngle(34.f);
	// Identity rotation on purpose: a spot light emits along its own forward (+X), so leaving this
	// unrotated makes the beam and the pillar's facing the same fact rather than two that can drift
	// apart. Cycle IV asks which way the mirror is turned; the answer has to be visible from across
	// the plaza, and it is only visible because the light points where the pillar does.
	AimedLight->SetRelativeRotation(FRotator::ZeroRotator);
}

bool AGloamsteadRestoredStructure::ConfigureForRitualType(ERitualType RitualType)
{
	FGloamRestoredStructureRecipe Recipe;
	if (!GetRestoredStructureRecipe(RitualType, Recipe))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RestoredStructure: no recipe for ritual type %d; leaving this restoration empty "
				 "rather than guessing what it looks like."),
			static_cast<int32>(RitualType));
		return false;
	}

	ConfiguredRitualType = RitualType;

	UStaticMesh* Body = Recipe.BodyMeshPath ? LoadObject<UStaticMesh>(nullptr, Recipe.BodyMeshPath) : nullptr;
	if (!Body)
	{
		// Loud, once. A silent failure here is precisely how four cycles stayed invisible.
		UE_LOG(LogTemp, Error,
			TEXT("RestoredStructure: could not load body mesh %s for ritual type %d - this "
				 "restoration will not be visible."),
			Recipe.BodyMeshPath ? Recipe.BodyMeshPath : TEXT("(none)"), static_cast<int32>(RitualType));
		return false;
	}

	BodyMesh->SetStaticMesh(Body);
	BodyMesh->SetRelativeScale3D(Recipe.BodyScale);
	if (UMaterialInterface* BodyMaterial =
			Recipe.BodyMaterialPath ? LoadObject<UMaterialInterface>(nullptr, Recipe.BodyMaterialPath) : nullptr)
	{
		BodyMesh->SetMaterial(0, BodyMaterial);
	}

	// Measure the body we actually loaded instead of trusting an authored offset. Max.Z is the real
	// top of the mesh in its own space whichever corner its pivot sits in, so this lands the crown
	// and the lamp correctly for a centred pivot and a base-at-zero pivot alike.
	const float BodyTopZ = Body->GetBoundingBox().Max.Z * Recipe.BodyScale.Z;

	if (Recipe.CrownMeshPath)
	{
		if (UStaticMesh* Crown = LoadObject<UStaticMesh>(nullptr, Recipe.CrownMeshPath))
		{
			CrownMesh->SetStaticMesh(Crown);
			// The crown is parented to a scaled body, so undo that scale before applying its own -
			// otherwise a 0.22-tall paving slab would squash whatever it carries by the same factor.
			const FVector InverseBody(
				1.f / FMath::Max(Recipe.BodyScale.X, KINDA_SMALL_NUMBER),
				1.f / FMath::Max(Recipe.BodyScale.Y, KINDA_SMALL_NUMBER),
				1.f / FMath::Max(Recipe.BodyScale.Z, KINDA_SMALL_NUMBER));
			CrownMesh->SetRelativeScale3D(Recipe.CrownScale * InverseBody);
			CrownMesh->SetRelativeLocation(
				FVector(0.f, 0.f, (BodyTopZ * Recipe.CrownHeightFraction) * InverseBody.Z));
			if (UMaterialInterface* CrownMaterial = Recipe.CrownMaterialPath
					? LoadObject<UMaterialInterface>(nullptr, Recipe.CrownMaterialPath)
					: nullptr)
			{
				CrownMesh->SetMaterial(0, CrownMaterial);
			}
			CrownMesh->SetVisibility(true);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("RestoredStructure: crown mesh %s missing for ritual type %d; the body still "
					 "stands, so the restoration remains visible."),
				Recipe.CrownMeshPath, static_cast<int32>(RitualType));
		}
	}

	// Lights are parented to the scaled body too, so their offset needs the same correction.
	const float LightZ = Recipe.LightHeightFraction * BodyTopZ
		/ FMath::Max(Recipe.BodyScale.Z, KINDA_SMALL_NUMBER);

	USceneComponent* Chosen = Recipe.bAimsLightForward
		? static_cast<USceneComponent*>(AimedLight.Get())
		: static_cast<USceneComponent*>(RestoredLight.Get());
	ULocalLightComponent* ChosenLight = Recipe.bAimsLightForward
		? static_cast<ULocalLightComponent*>(AimedLight.Get())
		: static_cast<ULocalLightComponent*>(RestoredLight.Get());
	if (Chosen && ChosenLight)
	{
		Chosen->SetRelativeLocation(FVector(0.f, 0.f, LightZ));
		ChosenLight->SetLightColor(Recipe.LightColor);
		ChosenLight->SetIntensity(Recipe.LightIntensity);
		ChosenLight->SetAttenuationRadius(Recipe.LightRadius);
		ChosenLight->SetVisibility(true);
	}

	if (Recipe.RestorationTag)
	{
		Tags.AddUnique(FName(Recipe.RestorationTag));
	}

	UE_LOG(LogTemp, Log, TEXT("RestoredStructure: raised ritual form %d at %s."),
		static_cast<int32>(RitualType), *GetActorLocation().ToCompactString());
	return true;
}

bool AGloamsteadRestoredStructure::HasVisibleBody() const
{
	return BodyMesh && BodyMesh->GetStaticMesh() != nullptr;
}

bool AGloamsteadRestoredStructure::HasRestoredLight() const
{
	return (RestoredLight && RestoredLight->IsVisible())
		|| (AimedLight && AimedLight->IsVisible());
}
