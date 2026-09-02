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
#include "Components/RitualPlacementComponent.h"
#include "Data/RitualDefinition.h"
#include "GloamsteadCharacter.h"
#include "GloamsteadGameMode.h"
#include "GloamsteadPlayerController.h"
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

	/**
	 * The DA_Ritual_* basename for a form. Deliberately NOT GetRitualTypeDisplayName with the space
	 * removed: display names are player-facing and may be localised, and an asset path that follows
	 * a localised string breaks in a language nobody on this project speaks.
	 */
	FString RitualFormAssetName(ERitualType Form)
	{
		switch (Form)
		{
		case ERitualType::LanternPost:  return TEXT("LanternPost");
		case ERitualType::GardenBed:    return TEXT("GardenBed");
		case ERitualType::PathPoint:    return TEXT("PathPoint");
		case ERitualType::MirrorPillar: return TEXT("MirrorPillar");
		case ERitualType::BellShrine:   return TEXT("BellShrine");
		case ERitualType::AnchorStone:  return TEXT("AnchorStone");
		default:                        return FString();
		}
	}
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

// ---------------------------------------------------------------------------------------------
// 6. Every night verb the runtime implements is reachable by the player.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightVerbsAreReachableTest,
	"Gloamstead.NightVerbs.EveryImplementedNightVerbHasAPlayerHook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightVerbsAreReachableTest::RunTest(const FString& /*Parameters*/)
{
	// UNightConsequenceRuntime::DisruptNearestThreat was fully implemented, BlueprintCallable, and
	// called by nothing anywhere in Source/. Strike is the only thing a player can do about a
	// Gatherer already draining the lantern they raised, so the whole verb existed and could not be
	// performed - the same shape of defect as an enemy with no mesh.
	//
	// Both night verbs are bound at key level rather than through an input-mapping asset, so an
	// asset-side check cannot see them. The exec hooks are the seam that is checkable headless, and
	// each is documented as calling the identical handler the key binding calls.
	UClass* CharacterClass = AGloamsteadCharacter::StaticClass();
	if (!TestNotNull(TEXT("the Gloamstead character class exists"), CharacterClass))
	{
		return false;
	}

	for (const TCHAR* VerbHook : { TEXT("GloamStrike"), TEXT("GloamWard"),
								   TEXT("GloamRestore"), TEXT("GloamInteract"), TEXT("GloamExamine") })
	{
		TestNotNull(
			*FString::Printf(TEXT("the player can reach the %s verb"), VerbHook),
			CharacterClass->FindFunctionByName(FName(VerbHook)));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamStrikeInterruptsTest,
	"Gloamstead.NightVerbs.StrikeInterruptsWithoutResolving",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamStrikeInterruptsTest::RunTest(const FString& /*Parameters*/)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("strike test world created"), World))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	AGloamsteadNightThreat* Threat = World->SpawnActor<AGloamsteadNightThreat>();
	if (TestNotNull(TEXT("threat spawned"), Threat))
	{
		FNightThreatSpec Spec;
		Spec.Archetype = ENightThreatArchetype::Gatherer;
		Threat->ConfigureThreat(Spec, /*BoundPointIndex*/ INDEX_NONE);

		Threat->Test_SetThreatState(ENightThreatState::Working);
		TestTrue(TEXT("a working threat can be struck"), Threat->Disrupt());
		TestEqual(TEXT("striking interrupts the work"),
			Threat->GetThreatState(), ENightThreatState::Disrupted);

		// The line the design is built on: striking buys seconds and never wins the night. A strike
		// that resolved a threat would turn every night into a damage race.
		TestNotEqual(TEXT("striking never resolves a threat"),
			Threat->GetThreatState(), ENightThreatState::Resolved);

		Threat->Test_SetThreatState(ENightThreatState::Resolved);
		TestFalse(TEXT("an already-answered threat cannot be struck again"), Threat->Disrupt());
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// ---------------------------------------------------------------------------------------------
// 7. Every authored ritual form's tuning asset is actually loaded.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamEveryRitualFormLoadsItsTuningTest,
	"Gloamstead.Restoration.EveryAuthoredRitualFormLoadsItsTuningAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEveryRitualFormLoadsItsTuningTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamsteadVisiblePlayabilityFixtures;

	// URitualPlacementComponent::EnsureRitualDefinitionsLoaded carries a hand-kept table of
	// /Game/Data/DA_Ritual_* paths. It shipped covering five of the six authored forms - AnchorStone
	// landed with the six-cycle arc and was never added - so Cycle VI's tuning asset was dead
	// content, and the miss announced itself as "Loaded 5 ritual definition asset(s)", which reads
	// exactly like success. SatisfiableWarningTags has no code fallback, so the Heart's satisfied-tag
	// count came up one short on the final cycle.
	for (const ERitualType Form : AuthoredForms)
	{
		const FString Basename = RitualFormAssetName(Form);
		const FString Path = FString::Printf(
			TEXT("/Game/Data/DA_Ritual_%s.DA_Ritual_%s"), *Basename, *Basename);

		URitualDefinition* Definition = LoadObject<URitualDefinition>(nullptr, *Path);
		if (TestNotNull(*FString::Printf(TEXT("%s ships a tuning asset"), *Path), Definition))
		{
			TestEqual(*FString::Printf(TEXT("%s declares the form it is keyed as"), *Path),
				Definition->RitualType, Form);
			// The field with no fallback. A form whose tuning asset names no tag cannot contribute
			// to the Heart's clarity even when it IS loaded.
			TestTrue(*FString::Printf(TEXT("%s names a satisfiable warning tag"), *Path),
				Definition->SatisfiableWarningTags.Num() > 0);
		}
	}

	return true;
}

// ---------------------------------------------------------------------------------------------
// 8. The sanctuary can be held, and every verb is documented somewhere the player can read it.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNoVerbShipsUndocumentedTest,
	"Gloamstead.Pause.NoPlayerVerbShipsUndocumented",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNoVerbShipsUndocumentedTest::RunTest(const FString& /*Parameters*/)
{
	// This game has no tutorial and no options screen, and two of its five verbs are bound at key
	// level rather than through an input-mapping asset - so an asset-side check cannot see them and
	// the pause overlay's table is the only place a player can learn they exist.
	//
	// The exec hooks are the checkable proxy for "a verb the player has": each is documented as
	// calling the identical handler its key binding calls. Requiring a matching row here means a
	// seventh verb cannot be added without either documenting it or deliberately deleting a line.
	const TArray<TPair<FString, FString>> VerbHooks = {
		{ TEXT("GloamRestore"),  TEXT("restore")  },
		{ TEXT("GloamInteract"), TEXT("interact") },
		{ TEXT("GloamExamine"),  TEXT("examine")  },
		{ TEXT("GloamStrike"),   TEXT("strike")   },
		{ TEXT("GloamWard"),     TEXT("ward")     },
	};

	UClass* CharacterClass = AGloamsteadCharacter::StaticClass();
	UClass* ControllerClass = AGloamsteadPlayerController::StaticClass();
	if (!TestNotNull(TEXT("the character class exists"), CharacterClass)
		|| !TestNotNull(TEXT("the controller class exists"), ControllerClass))
	{
		return false;
	}

	const TArrayView<const AGloamsteadHUD::FGloamControlRow> Rows = AGloamsteadHUD::GetControlRows();

	auto RowsName = [&Rows](const FString& Verb)
	{
		for (const AGloamsteadHUD::FGloamControlRow& Row : Rows)
		{
			if (FString(Row.Verb).Contains(Verb))
			{
				return true;
			}
		}
		return false;
	};

	for (const TPair<FString, FString>& Hook : VerbHooks)
	{
		TestNotNull(*FString::Printf(TEXT("the player can reach %s"), *Hook.Key),
			CharacterClass->FindFunctionByName(FName(*Hook.Key)));
		TestTrue(*FString::Printf(TEXT("the controls table documents '%s'"), *Hook.Value),
			RowsName(Hook.Value));
	}

	// Pause itself is a verb, and the screen that documents the others must document it too.
	TestNotNull(TEXT("the player can reach GloamPause"),
		ControllerClass->FindFunctionByName(FName(TEXT("GloamPause"))));
	TestTrue(TEXT("the controls table documents holding the sanctuary"), RowsName(TEXT("hold")));

	// Every row must actually say something. A blank key or verb is a row that documents nothing
	// while looking like documentation.
	for (const AGloamsteadHUD::FGloamControlRow& Row : Rows)
	{
		TestTrue(TEXT("every controls row names a key"), Row.Key != nullptr && *Row.Key != TEXT('\0'));
		TestTrue(TEXT("every controls row names a verb"), Row.Verb != nullptr && *Row.Verb != TEXT('\0'));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
