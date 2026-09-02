// The three ways this game was invisible while every other test was green.
//
// A restoration that consumes its point and spawns nothing, a night threat that walks and drains
// with no body to see, and a sanctuary whose light, corruption, cycle and phase existed only in the
// log: none of those is a logic defect, so none of them could fail a logic test. They shipped past
// 170 green tests because the suite asked whether the rules were right, never whether the player
// could see them.
//
// These tests ask the second question. Two of them load real shipped assets on purpose - a typo in a
// /Game path is the exact failure mode a pure-logic suite cannot catch, and it degrades silently
// into the invisibility this whole file exists to end.
#include "Misc/AutomationTest.h"

#include "Actors/GloamsteadNightThreat.h"
#include "Actors/GloamsteadRestoredStructure.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/NightThreatTypes.h"
#include "Data/RitualTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GloamsteadGameMode.h"
#include "Materials/MaterialInterface.h"
#include "UI/GloamsteadHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GloamsteadVisiblePlayabilityFixtures
{
	/** Every ritual form the authored six-cycle arc actually asks the player to perform. */
	const ERitualType AuthoredForms[] = {
		ERitualType::LanternPost,
		ERitualType::GardenBed,
		ERitualType::PathPoint,
		ERitualType::MirrorPillar,
		ERitualType::BellShrine,
		ERitualType::AnchorStone,
	};

	const ENightThreatArchetype Archetypes[] = {
		ENightThreatArchetype::Gatherer,
		ENightThreatArchetype::Borrowed,
		ENightThreatArchetype::Bargainer,
		ENightThreatArchetype::Echo,
	};
}

// ---------------------------------------------------------------------------------------------
// 1. Every authored ritual form produces something the player can see.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamEveryRitualFormIsVisibleTest,
	"Gloamstead.Restoration.EveryAuthoredRitualFormHasAVisibleRestoration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEveryRitualFormIsVisibleTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamsteadVisiblePlayabilityFixtures;

	// The contract: a form is visible either because it owns a dedicated restored actor (the lantern
	// and the garden, which carry art this generic body cannot) or because it has a kit recipe.
	// Nothing may be neither. That was true of four of the six forms for the whole of Phase 3.
	for (const ERitualType Form : AuthoredForms)
	{
		const bool bHasDedicatedActor =
			Form == ERitualType::LanternPost || Form == ERitualType::GardenBed;

		FGloamRestoredStructureRecipe Recipe;
		const bool bHasRecipe = GetRestoredStructureRecipe(Form, Recipe);

		TestTrue(
			*FString::Printf(TEXT("ritual form %d restores into something visible"), static_cast<int32>(Form)),
			bHasDedicatedActor || bHasRecipe);

		// And exactly one of the two, so a form never has two competing answers to "what appears".
		TestFalse(
			*FString::Printf(TEXT("ritual form %d has one visible answer, not two"), static_cast<int32>(Form)),
			bHasDedicatedActor && bHasRecipe);

		if (bHasRecipe)
		{
			TestNotNull(
				*FString::Printf(TEXT("form %d recipe names a body mesh"), static_cast<int32>(Form)),
				Recipe.BodyMeshPath);
			TestNotNull(
				*FString::Printf(TEXT("form %d recipe names its restoration tag"), static_cast<int32>(Form)),
				Recipe.RestorationTag);
			TestTrue(
				*FString::Printf(TEXT("form %d recipe carries light"), static_cast<int32>(Form)),
				Recipe.LightIntensity > 0.f && Recipe.LightRadius > 0.f);
		}
	}

	// Invalid must never resolve to a body. A restoration of nothing that draws a column is worse
	// than one that draws nothing.
	FGloamRestoredStructureRecipe InvalidRecipe;
	TestFalse(TEXT("the invalid ritual form has no recipe"),
		GetRestoredStructureRecipe(ERitualType::Invalid, InvalidRecipe));

	return true;
}

// ---------------------------------------------------------------------------------------------
// 2. Those recipes name assets that actually exist.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamRecipeAssetsResolveTest,
	"Gloamstead.Restoration.RecipeAssetPathsResolveToShippedContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRecipeAssetsResolveTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamsteadVisiblePlayabilityFixtures;

	// Hand-written /Game paths are the one part of a recipe a compiler cannot check. A mistyped one
	// degrades exactly into the silent invisibility being fixed here, so it is checked by loading.
	for (const ERitualType Form : AuthoredForms)
	{
		FGloamRestoredStructureRecipe Recipe;
		if (!GetRestoredStructureRecipe(Form, Recipe))
		{
			continue;
		}

		const FString Label = FString::Printf(TEXT("ritual form %d"), static_cast<int32>(Form));

		TestNotNull(*FString::Printf(TEXT("%s body mesh %s loads"), *Label, Recipe.BodyMeshPath),
			LoadObject<UStaticMesh>(nullptr, Recipe.BodyMeshPath));

		if (Recipe.BodyMaterialPath)
		{
			TestNotNull(*FString::Printf(TEXT("%s body material %s loads"), *Label, Recipe.BodyMaterialPath),
				LoadObject<UMaterialInterface>(nullptr, Recipe.BodyMaterialPath));
		}
		if (Recipe.CrownMeshPath)
		{
			TestNotNull(*FString::Printf(TEXT("%s crown mesh %s loads"), *Label, Recipe.CrownMeshPath),
				LoadObject<UStaticMesh>(nullptr, Recipe.CrownMeshPath));
		}
		if (Recipe.CrownMaterialPath)
		{
			TestNotNull(*FString::Printf(TEXT("%s crown material %s loads"), *Label, Recipe.CrownMaterialPath),
				LoadObject<UMaterialInterface>(nullptr, Recipe.CrownMaterialPath));
		}
	}

	return true;
}

// ---------------------------------------------------------------------------------------------
// 3. A spawned structure really builds itself.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamStructureBuildsItselfTest,
	"Gloamstead.Restoration.SpawnedStructureRaisesABodyAndALight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamStructureBuildsItselfTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamsteadVisiblePlayabilityFixtures;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("structure test world created"), World))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	for (const ERitualType Form : AuthoredForms)
	{
		FGloamRestoredStructureRecipe Recipe;
		if (!GetRestoredStructureRecipe(Form, Recipe))
		{
			continue;
		}

		AGloamsteadRestoredStructure* Structure = World->SpawnActor<AGloamsteadRestoredStructure>();
		if (!TestNotNull(TEXT("restored structure spawned"), Structure))
		{
			continue;
		}

		const FString Label = FString::Printf(TEXT("ritual form %d"), static_cast<int32>(Form));

		// Before configuring, it is deliberately empty: an unconfigured structure must never look
		// like a successfully restored one.
		TestFalse(*FString::Printf(TEXT("%s is empty before it is built"), *Label),
			Structure->HasVisibleBody());

		TestTrue(*FString::Printf(TEXT("%s builds"), *Label), Structure->ConfigureForRitualType(Form));
		TestTrue(*FString::Printf(TEXT("%s has a body after building"), *Label),
			Structure->HasVisibleBody());
		TestTrue(*FString::Printf(TEXT("%s casts its restored light"), *Label),
			Structure->HasRestoredLight());
		TestEqual(*FString::Printf(TEXT("%s remembers what it was built as"), *Label),
			Structure->GetRitualType(), Form);
		TestTrue(*FString::Printf(TEXT("%s carries its restoration tag"), *Label),
			Structure->Tags.Contains(FName(Recipe.RestorationTag)));
	}

	// The mirror is the one form whose facing is the question, so its light must be the aimed one.
	{
		AGloamsteadRestoredStructure* Mirror = World->SpawnActor<AGloamsteadRestoredStructure>();
		if (Mirror && Mirror->ConfigureForRitualType(ERitualType::MirrorPillar))
		{
			TestTrue(TEXT("the mirror pillar aims a beam rather than glowing in all directions"),
				Mirror->AimedLight && Mirror->AimedLight->IsVisible());
			TestFalse(TEXT("the mirror pillar does not also glow omnidirectionally"),
				Mirror->RestoredLight && Mirror->RestoredLight->IsVisible());
		}
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// ---------------------------------------------------------------------------------------------
// 4. Night threats have a body, and the four archetypes are told apart on sight.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamThreatHasABodyTest,
	"Gloamstead.NightThreat.EveryThreatHasABodyAndASignatureColour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamThreatHasABodyTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamsteadVisiblePlayabilityFixtures;

	// Pairwise-distinct colours, not merely "each archetype returns something". Two archetypes that
	// happened to share a colour would be indistinguishable in exactly the situation the colour
	// exists for: across a dark plaza, at night.
	for (int32 A = 0; A < UE_ARRAY_COUNT(Archetypes); ++A)
	{
		for (int32 B = A + 1; B < UE_ARRAY_COUNT(Archetypes); ++B)
		{
			const FLinearColor First = AGloamsteadNightThreat::GetArchetypeGlowColor(Archetypes[A]);
			const FLinearColor Second = AGloamsteadNightThreat::GetArchetypeGlowColor(Archetypes[B]);
			TestFalse(
				*FString::Printf(TEXT("archetypes %d and %d are not the same colour"),
					static_cast<int32>(Archetypes[A]), static_cast<int32>(Archetypes[B])),
				First.Equals(Second, 0.08f));
		}
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("threat test world created"), World))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	for (const ENightThreatArchetype Archetype : Archetypes)
	{
		AGloamsteadNightThreat* Threat = World->SpawnActor<AGloamsteadNightThreat>();
		if (!TestNotNull(TEXT("night threat spawned"), Threat))
		{
			continue;
		}

		const FString Label = FString::Printf(TEXT("archetype %d"), static_cast<int32>(Archetype));

		// The whole point: this is true straight out of SpawnActor on the raw C++ class, because
		// that is exactly how UNightConsequenceRuntime spawns them - no Blueprint in the path.
		TestTrue(*FString::Printf(TEXT("%s has a body before anything configures it"), *Label),
			Threat->HasVisibleBody());

		FNightThreatSpec Spec;
		Spec.Archetype = Archetype;
		Threat->ConfigureThreat(Spec, /*BoundPointIndex*/ INDEX_NONE);

		TestEqual(*FString::Printf(TEXT("%s wears its signature colour"), *Label),
			Threat->GloamGlow ? Threat->GloamGlow->GetLightColor() : FColor::Black,
			AGloamsteadNightThreat::GetArchetypeGlowColor(Archetype).ToFColor(true));
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// ---------------------------------------------------------------------------------------------
// 5. The HUD holds the Heart's words, and the game mode actually hands the player one.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamHUDHoldsTheWarningTest,
	"Gloamstead.HUD.TheHeartsWordsOutliveTheCaptionThatShowedThem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamHUDHoldsTheWarningTest::RunTest(const FString& /*Parameters*/)
{
	// The game mode must actually nominate a HUD, or none of the rest of this is reachable in play.
	// Checked on the CDO because the shipped game mode Blueprint overrides only its default pawn.
	const AGloamsteadGameMode* GameModeDefaults = GetDefault<AGloamsteadGameMode>();
	if (TestNotNull(TEXT("Gloamstead game mode defaults exist"), GameModeDefaults))
	{
		TestTrue(TEXT("the game mode hands every player a Gloamstead HUD"),
			GameModeDefaults->HUDClass != nullptr
				&& GameModeDefaults->HUDClass->IsChildOf(AGloamsteadHUD::StaticClass()));
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("HUD test world created"), World))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	AGloamsteadHUD* HUD = World->SpawnActor<AGloamsteadHUD>();
	if (TestNotNull(TEXT("HUD spawned"), HUD))
	{
		TestTrue(TEXT("the HUD says nothing before the Heart has spoken"),
			HUD->Test_GetStandingWarningText().IsEmpty());

		FVeilHeartWarningFragment Fragment;
		Fragment.WarningId = TEXT("GardenRot");
		Fragment.Fragment = FText::FromString(
			TEXT("Wake the roots. Wet earth shelters; bare ash feeds the Gloam."));
		HUD->HandleHeartWarning(Fragment);

		TestEqual(TEXT("the HUD holds exactly what the Heart said"),
			HUD->Test_GetStandingWarningText().ToString(), Fragment.Fragment.ToString());

		// A second broadcast of the same warning is how the caption surface is re-armed when a
		// presenter registers. The HUD must simply keep showing it, never blank between the two.
		HUD->HandleHeartWarning(Fragment);
		TestFalse(TEXT("a re-broadcast does not blank the standing warning"),
			HUD->Test_GetStandingWarningText().IsEmpty());

		FVeilHeartWarningFragment Next;
		Next.WarningId = TEXT("RoadForgotten");
		Next.Fragment = FText::FromString(
			TEXT("Give the lantern a road. Loops guard; dead ends invite hands."));
		HUD->HandleHeartWarning(Next);
		TestEqual(TEXT("a new warning replaces the old one"),
			HUD->Test_GetStandingWarningText().ToString(), Next.Fragment.ToString());

		// Outside Night there is no countdown to draw, and a HUD that invented one would be
		// counting down to nothing.
		TestTrue(TEXT("there is no night countdown outside the night"),
			HUD->Test_GetNightSecondsRemaining() < 0.f);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
