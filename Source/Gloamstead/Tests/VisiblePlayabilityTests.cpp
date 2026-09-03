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
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
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
#include "Presentation/GloamsteadCorruptionVisualizer.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ScopeExit.h"
#include "Audio/GloamsteadSoundscapeSubsystem.h"
#include "Sound/SoundBase.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/VeilHeart.h"
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

// ---------------------------------------------------------------------------------------------
// 9. The Houdini-forged gloam assets exist, are real geometry, and are wired to something.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamForgedAssetsAreRealTest,
	"Gloamstead.Forge.EveryForgedGloamAssetIsRealGeometryAndIsUsed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamForgedAssetsAreRealTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamsteadVisiblePlayabilityFixtures;

	// These are generated by procedural/houdini/forge_gloam_assets.py and imported by its companion.
	// The triangle floor is the load-bearing part: the first forge run produced seven FBX files of
	// 80-180 KB that each held a handful of PRIMITIVE tubes - one point, one prim - because the Tube
	// SOP defaults to Primitive rather than Polygon. They imported. They had non-zero everything.
	// They were not geometry. A file-exists check would have passed all seven.
	auto CheckMesh = [this](const TCHAR* Path, int32 MinTriangles)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
		if (!TestNotNull(*FString::Printf(TEXT("forged asset %s exists"), Path), Mesh))
		{
			return;
		}
		const int32 Triangles = Mesh->GetNumTriangles(0);
		TestTrue(
			*FString::Printf(TEXT("%s is real geometry (%d tris, floor %d)"), Path, Triangles, MinTriangles),
			Triangles >= MinTriangles);
	};

	// Every archetype's shroud, reached through the same accessor the threat uses - so a shroud that
	// exists but is not wired to an archetype still fails.
	for (const ENightThreatArchetype Archetype : Archetypes)
	{
		const TCHAR* ShroudPath = AGloamsteadNightThreat::GetArchetypeShroudPath(Archetype);
		if (TestNotNull(
				*FString::Printf(TEXT("archetype %d names a shroud"), static_cast<int32>(Archetype)),
				ShroudPath))
		{
			CheckMesh(ShroudPath, /*MinTriangles*/ 200);
		}
	}

	// The three growth tiers, reached through the visualiser's own severity mapping.
	const TCHAR* SmallPath = UGloamsteadCorruptionVisualizer::GetGrowthMeshPathFor(0.25f);
	const TCHAR* MediumPath = UGloamsteadCorruptionVisualizer::GetGrowthMeshPathFor(0.55f);
	const TCHAR* LargePath = UGloamsteadCorruptionVisualizer::GetGrowthMeshPathFor(0.90f);

	TestNotNull(TEXT("a lightly corrupted point grows something"), SmallPath);
	TestNotNull(TEXT("a moderately corrupted point grows something"), MediumPath);
	TestNotNull(TEXT("a badly corrupted point grows something"), LargePath);

	// A clean point must grow nothing. Rot that appears where there is no rot is worse than none.
	TestNull(TEXT("a clean point grows nothing"),
		UGloamsteadCorruptionVisualizer::GetGrowthMeshPathFor(0.05f));

	// The three tiers must be three different assets, or severity is not readable in the world.
	if (SmallPath && MediumPath && LargePath)
	{
		TestNotEqual(TEXT("small and medium rot are different growths"),
			FString(SmallPath), FString(MediumPath));
		TestNotEqual(TEXT("medium and large rot are different growths"),
			FString(MediumPath), FString(LargePath));

		CheckMesh(SmallPath, /*MinTriangles*/ 80);
		CheckMesh(MediumPath, /*MinTriangles*/ 120);
		CheckMesh(LargePath, /*MinTriangles*/ 180);

		// Severity must cost more geometry, not just a bigger transform - that is the whole reason
		// there are three assets rather than one scaled one.
		UStaticMesh* Small = LoadObject<UStaticMesh>(nullptr, SmallPath);
		UStaticMesh* Large = LoadObject<UStaticMesh>(nullptr, LargePath);
		if (Small && Large)
		{
			TestTrue(TEXT("a worse bloom is a busier crystal, not the same one enlarged"),
				Large->GetNumTriangles(0) > Small->GetNumTriangles(0));
		}
	}

	return true;
}

// ---------------------------------------------------------------------------------------------

/**
 * Every dawn answers the night, and the end of the arc is a screen.
 *
 * The night outcome is the payoff the whole game is built to deliver: whether the objective
 * resolved, whether the warning was heeded, what the second reading graded out as, and the scar or
 * boon carried forward. All of it was computed and broadcast on OnDawnReflectionDelegate, and the
 * only thing listening was a UE_LOG. A player could finish all six nights without once being told
 * whether they had understood any of them - with the whole suite green, because every test asked
 * whether the outcome was CORRECT and none asked whether it was ever SHOWN.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamDawnAnswersTheNightTest,
	"Gloamstead.HUD.EveryDawnAnswersTheNightAndTheArcEndsOnAScreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamDawnAnswersTheNightTest::RunTest(const FString& /*Parameters*/)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("dawn test world created"), World))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	AGloamsteadHUD* HUD = World->SpawnActor<AGloamsteadHUD>();
	if (TestNotNull(TEXT("HUD spawned"), HUD))
	{
		TestTrue(TEXT("no night has resolved, so there is no verdict to draw"),
			HUD->Test_GetDawnVerdictLine().IsEmpty());
		TestEqual(TEXT("the ledger starts empty"), HUD->Test_GetNightLedger().Num(), 0);
		TestFalse(TEXT("and nothing would be drawn"), HUD->ShouldDrawDawnSummary());

		// A night the player read right.
		FNightRuntimeOutcome Held;
		Held.Result = ENightOutcomeResult::Success;
		Held.NightType = ENightConsequenceType::Retrieval;
		Held.bWarningHeeded = true;
		Held.ResultTag = TEXT("Boon.PathLoop");
		Held.SecondReadingGrade = EExperienceReadingGrade::Insight;
		Held.SecondReadingTag = TEXT("Boon.PathLoop");
		Held.TargetCorruptionDelta = -0.34f;
		HUD->Test_RecordNight(3, Held);

		TestEqual(TEXT("the dawn says the sanctuary held"),
			HUD->Test_GetDawnVerdictLine(), FString(TEXT("THE SANCTUARY HELD")));

		if (TestEqual(TEXT("the night is recorded once"), HUD->Test_GetNightLedger().Num(), 1))
		{
			const AGloamsteadHUD::FGloamNightRecord& Row = HUD->Test_GetNightLedger()[0];
			TestEqual(TEXT("the ledger keeps the night type"),
				static_cast<int32>(Row.NightType), static_cast<int32>(ENightConsequenceType::Retrieval));
			TestEqual(TEXT("the ledger keeps the reading grade"),
				static_cast<int32>(Row.Grade), static_cast<int32>(EExperienceReadingGrade::Insight));
			TestTrue(TEXT("the ledger keeps whether the warning was heeded"), Row.bWarningHeeded);
			TestEqual(TEXT("the ledger keeps the tag carried forward"),
				Row.ResultTag, FName(TEXT("Boon.PathLoop")));
		}

		// The BP-compat entry point reflects with a default outcome, so one night can be reflected
		// more than once. The ending must be an arc, not a tally of how often something called
		// reflect - so a second reflection for the same cycle replaces the row rather than adding one.
		FNightRuntimeOutcome Corrected;
		Corrected.Result = ENightOutcomeResult::Failure;
		Corrected.NightType = ENightConsequenceType::Retrieval;
		Corrected.ResultTag = TEXT("Scar.DeadEnd");
		Corrected.TargetCorruptionDelta = 0.21f;
		HUD->Test_RecordNight(3, Corrected);

		TestEqual(TEXT("reflecting the same cycle twice does not lengthen the arc"),
			HUD->Test_GetNightLedger().Num(), 1);
		TestEqual(TEXT("the later reflection is the one the ending remembers"),
			HUD->Test_GetDawnVerdictLine(), FString(TEXT("A SCAR REMAINS")));

		// Every result the outcome enum can carry must have a sentence. A verdict that fell through
		// to an empty string would draw a blank headline over a real panel.
		FNightRuntimeOutcome Probe;
		Probe.NightType = ENightConsequenceType::Corruption;
		const ENightOutcomeResult AllResults[] = {
			ENightOutcomeResult::None,
			ENightOutcomeResult::Success,
			ENightOutcomeResult::Partial,
			ENightOutcomeResult::Failure,
		};
		for (const ENightOutcomeResult Result : AllResults)
		{
			Probe.Result = Result;
			HUD->Test_RecordNight(3, Probe);
			TestFalse(
				FString::Printf(TEXT("outcome %s has a verdict sentence"),
					*GetNightOutcomeResultDisplayName(Result)),
				HUD->Test_GetDawnVerdictLine().IsEmpty());
		}

		// The panel is gated on standing in a dawn, not merely on having survived a night. This
		// world has no phase authority walked to Dawn, so it must refuse to draw.
		TestFalse(TEXT("the dawn panel does not draw outside a dawn"), HUD->ShouldDrawDawnSummary());
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// ---------------------------------------------------------------------------------------------

/**
 * The ending reads the arc back, and distinct arcs do not end the same way.
 *
 * IsExperienceComplete() had one production reader - a single line of small text on the left
 * panel - so finishing all six nights left the readout counting toward a seventh that does not
 * exist. This asserts the ledger the ending draws from actually distinguishes the runs it is
 * supposed to distinguish, which is what stops the closing screen from being a participation notice.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamEndingReadsTheWholeArcTest,
	"Gloamstead.HUD.TheEndingReadsTheArcBackRatherThanCongratulating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEndingReadsTheWholeArcTest::RunTest(const FString& /*Parameters*/)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("ending test world created"), World))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	AGloamsteadHUD* HUD = World->SpawnActor<AGloamsteadHUD>();
	if (TestNotNull(TEXT("HUD spawned"), HUD))
	{
		// Six nights, deliberately mixed: the arc the ending must be able to describe is not a
		// clean sweep, and a reckoning that cannot tell a scarred run from a held one is decoration.
		const ENightOutcomeResult Arc[] = {
			ENightOutcomeResult::Success,
			ENightOutcomeResult::Success,
			ENightOutcomeResult::Partial,
			ENightOutcomeResult::Failure,
			ENightOutcomeResult::Success,
			ENightOutcomeResult::Success,
		};

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Arc); ++Index)
		{
			FNightRuntimeOutcome Outcome;
			Outcome.Result = Arc[Index];
			Outcome.NightType = ENightConsequenceType::Corruption;
			HUD->Test_RecordNight(Index + 1, Outcome);
		}

		// The production path resolves the cycle from the experience subsystem, which a bare test
		// world does not have. Drive it once anyway: it must still record rather than drop the
		// night on the floor, because a HUD that silently ignored a dawn would look identical to
		// one that had nothing to report.
		FNightRuntimeOutcome ViaProductionPath;
		ViaProductionPath.Result = ENightOutcomeResult::Partial;
		ViaProductionPath.NightType = ENightConsequenceType::Fracture;
		HUD->HandleDawnReflection(ViaProductionPath);
		TestEqual(TEXT("a dawn with no cycle authority is still recorded"),
			HUD->Test_GetNightLedger().Num(), 7);

		if (TestEqual(TEXT("six authored nights plus the unattributed one make seven rows"),
				HUD->Test_GetNightLedger().Num(), 7))
		{
			int32 Held = 0;
			int32 Lingering = 0;
			int32 Scarred = 0;
			TSet<int32> Cycles;
			for (const AGloamsteadHUD::FGloamNightRecord& Row : HUD->Test_GetNightLedger())
			{
				Cycles.Add(Row.Cycle);
				switch (Row.Result)
				{
				case ENightOutcomeResult::Success: ++Held; break;
				case ENightOutcomeResult::Partial: ++Lingering; break;
				case ENightOutcomeResult::Failure: ++Scarred; break;
				default: break;
				}
			}

			TestEqual(TEXT("every night has its own cycle number"), Cycles.Num(), 7);
			TestEqual(TEXT("four nights held"), Held, 4);
			TestEqual(TEXT("two nights lingered"), Lingering, 2);
			TestEqual(TEXT("one night scarred"), Scarred, 1);
		}
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// ---------------------------------------------------------------------------------------------

/**
 * The clues the player has gathered are shown, and the ones they have not are not spoiled.
 *
 * FVeilHeartWarningSupportChannel::EvidenceText is documented in the type itself as "player-facing
 * evidence text for journal/caption presentation". It was authored for every warning in the shipped
 * catalog and presented nowhere: the only production reader of SupportChannels was the predicate
 * deciding whether an encounter counted. So the game set a hard gate - gather N distinct clues
 * before the cycle will accept a restoration - authored what each clue says, and then showed the
 * player neither the sentence nor how close they were to the gate.
 *
 * This runs against the real shipped catalog rather than a fixture, because the failure being
 * guarded is content reaching the player, and a hand-built fragment would prove nothing about
 * whether the shipped rows have anything to say.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamEvidenceIsShownWithoutSpoilingItTest,
	"Gloamstead.HUD.FoundEvidenceIsShownAndUnfoundEvidenceIsNotSpoiled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEvidenceIsShownWithoutSpoilingItTest::RunTest(const FString& /*Parameters*/)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("evidence test world created"), World))
	{
		return false;
	}
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	AVeilHeart* Heart = World->SpawnActor<AVeilHeart>();
	if (!TestNotNull(TEXT("the Heart spawned"), Heart))
	{
		return false;
	}
	// Same rationale as ShippedWarningCatalogTests: actors spawned into a manually created test
	// world do not reliably receive BeginPlay, and without it the Heart never loads its catalog.
	if (!Heart->HasActorBegunPlay())
	{
		Heart->DispatchBeginPlay();
	}

	TArray<FVeilHeartEvidenceLine> Lines;
	int32 Required = 0;

	// Nothing armed: there is no question yet, so there is no clue list to hand over.
	TestFalse(TEXT("no plan means no evidence to report"), Heart->GetStandingEvidence(Lines, Required));
	TestEqual(TEXT("and nothing is returned"), Lines.Num(), 0);

	UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>();
	PopulateDefaultExperienceCyclePlans(*Catalog);
	const FExperienceCyclePlan* Garden = Catalog->AuthoredPlans.FindByPredicate(
		[](const FExperienceCyclePlan& Plan) { return Plan.PlanId == FName(TEXT("Cycle2_Garden")); });
	if (!TestNotNull(TEXT("the garden cycle is authored"), Garden))
	{
		return false;
	}

	Heart->Test_SetActivePlan(*Garden);

	// Armed but silent. The Heart has not spoken yet, and listing the clues now would hand the
	// player the shape of a question they have not been asked.
	TestFalse(TEXT("an unspoken warning reports no evidence"),
		Heart->GetStandingEvidence(Lines, Required));

	// The Heart refuses to present an authored warning unless something is actually registered to
	// show it - the fair-crypticism gate that stops a cycle advancing on a sentence nobody saw. So
	// a presenter has to exist here for the same reason one has to exist in the game.
	AGloamsteadHUD* Presenter = World->SpawnActor<AGloamsteadHUD>();
	if (!TestNotNull(TEXT("a warning presenter spawned"), Presenter))
	{
		return false;
	}
	Heart->OnWarningEmittedDelegate.AddDynamic(Presenter, &AGloamsteadHUD::HandleHeartWarning);
	if (!TestTrue(TEXT("the presenter registered with the Heart"),
			Heart->RegisterWarningPresenter(
				Presenter, GET_FUNCTION_NAME_CHECKED(AGloamsteadHUD, HandleHeartWarning))))
	{
		return false;
	}

	// EmitWarningForNight is the generic emitter and deliberately clears LastEmittedPlanId, so it
	// does NOT count as presenting a plan's exact warning. EmitWarningForPlan is the plan-bound
	// path the cycle actually uses, and the one the journal is gated behind.
	if (!TestTrue(TEXT("the plan-bound warning was presented"), Heart->EmitWarningForPlan(*Garden)))
	{
		// Without a presented warning the rest of this test asserts against a state the game never
		// reaches. Say so plainly rather than reporting a cascade of downstream failures.
		AddError(TEXT("the garden warning was not presented, so the evidence journal cannot be exercised"));
		return false;
	}
	TestEqual(TEXT("and the presenter is holding what it said"),
		Presenter->Test_GetStandingWarningText().IsEmpty(), false);

	if (!TestTrue(TEXT("a presented warning reports its evidence"),
			Heart->GetStandingEvidence(Lines, Required)))
	{
		return false;
	}

	TestTrue(TEXT("the shipped garden warning authors at least one clue"), Lines.Num() > 0);
	TestEqual(TEXT("the gate reported is the plan's own minimum"),
		Required, Garden->MinimumDistinctSupportCount);

	// Before the player has gone looking, every clue is listed as existing and none says anything.
	// Both halves matter: the count is the gate and must be visible; the sentences are the answer
	// and must not be.
	for (const FVeilHeartEvidenceLine& Line : Lines)
	{
		TestFalse(TEXT("no clue is found before the player looks"), Line.bFound);
		TestTrue(TEXT("an unfound clue gives away nothing it says"), Line.EvidenceText.IsEmpty());
		TestTrue(TEXT("but it does name its medium, which is direction, not answer"),
			Line.ChannelType != NAME_None);
	}

	// Find exactly one, by the same path a world evidence source uses.
	const FName FirstSupport = Lines[0].SupportId;
	const FName FirstChannel = Lines[0].ChannelType;
	if (!TestTrue(TEXT("the first clue can be encountered"),
			Heart->Test_RecordSupportEncounter(Garden->WarningId, FirstSupport, FirstChannel)))
	{
		return false;
	}

	if (!TestTrue(TEXT("the journal still reports"), Heart->GetStandingEvidence(Lines, Required)))
	{
		return false;
	}

	int32 FoundCount = 0;
	for (const FVeilHeartEvidenceLine& Line : Lines)
	{
		if (Line.SupportId == FirstSupport)
		{
			++FoundCount;
			TestTrue(TEXT("the clue the player found is marked found"), Line.bFound);
			TestFalse(TEXT("and it now says what the catalog authored for it"),
				Line.EvidenceText.IsEmpty());
		}
		else
		{
			TestFalse(TEXT("a clue not yet found stays unfound"), Line.bFound);
			TestTrue(TEXT("and still gives nothing away"), Line.EvidenceText.IsEmpty());
		}
	}
	TestEqual(TEXT("exactly one clue was found"), FoundCount, 1);

	Heart->Test_ClearActivePlan();
	return true;
}

// ---------------------------------------------------------------------------------------------

/**
 * The night is as long as the night has something in it.
 *
 * A single ceiling for all six nights was the wrong shape. The reasoning that set it derives ~100 s
 * from a threat lifecycle of 25-30 s, wanting three or four of them - but Cycles I and II field no
 * threat at all, so they were spending that budget delivering three or four lifecycles of nothing,
 * and Cycle VI fields three threats at once on the same clock.
 *
 * This pins the properties that must hold no matter how the base constant is later tuned: the curve
 * is monotone in pressure, no night is ever unbounded, and the whole arc's night time stays inside a
 * band that leaves room for the interpretation the rest of the half hour is supposed to be.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightLengthFollowsWhatIsOutThereTest,
	"Gloamstead.Cadence.NightLengthFollowsWhatTheNightActuallyFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightLengthFollowsWhatIsOutThereTest::RunTest(const FString& /*Parameters*/)
{
	using FDayNight = UGloamsteadDayNightSubsystem;

	// Nights that spawn no threat at all must be shorter than the single-threat night the base
	// ceiling was derived for. Otherwise the quiet nights are dead air by construction.
	const float Tutorial   = FDayNight::NightDurationScaleForType(ENightConsequenceType::Tutorial);
	const float Corruption = FDayNight::NightDurationScaleForType(ENightConsequenceType::Corruption);
	const float Omen       = FDayNight::NightDurationScaleForType(ENightConsequenceType::Omen);
	const float Retrieval  = FDayNight::NightDurationScaleForType(ENightConsequenceType::Retrieval);
	const float Possession = FDayNight::NightDurationScaleForType(ENightConsequenceType::SilencePossession);
	const float Bargain    = FDayNight::NightDurationScaleForType(ENightConsequenceType::Bargain);
	const float Siege      = FDayNight::NightDurationScaleForType(ENightConsequenceType::TrueSiege);

	TestTrue(TEXT("the tutorial night, which fields nothing, is shorter than a threat night"),
		Tutorial < Retrieval);
	TestTrue(TEXT("the corruption night, which fields nothing, is shorter than a threat night"),
		Corruption < Retrieval);
	TestTrue(TEXT("the omen night, which fields nothing, is shorter than a threat night"),
		Omen < Retrieval);

	// And the curve rises with what the player has to answer.
	TestTrue(TEXT("a night that must be exposed before it can be answered runs longer"),
		Possession > Retrieval);
	TestTrue(TEXT("the bargain runs longer still"), Bargain >= Possession);
	TestTrue(TEXT("and the three-threat siege is the longest night in the arc"),
		Siege > Bargain);

	// No night may be unbounded or instant, including one whose type never resolved. A zero-length
	// ceiling would end the night the frame it began; a negative one never fires at all.
	const ENightConsequenceType EveryType[] = {
		ENightConsequenceType::Invalid,
		ENightConsequenceType::Tutorial,
		ENightConsequenceType::Corruption,
		ENightConsequenceType::Omen,
		ENightConsequenceType::Retrieval,
		ENightConsequenceType::SilencePossession,
		ENightConsequenceType::Mirror,
		ENightConsequenceType::Bargain,
		ENightConsequenceType::Fracture,
		ENightConsequenceType::TrueSiege,
	};
	for (const ENightConsequenceType Type : EveryType)
	{
		const float Scale = FDayNight::NightDurationScaleForType(Type);
		TestTrue(FString::Printf(TEXT("%s has a positive night ceiling"),
				*GetNightConsequenceTypeDisplayName(Type)),
			Scale > 0.f);
		TestTrue(FString::Printf(TEXT("%s does not run absurdly long"),
				*GetNightConsequenceTypeDisplayName(Type)),
			Scale <= 3.0f);
	}

	// The arc's actual night budget, from the six authored night types in slot order. This is the
	// only number in the project that says how much of the target half hour is spent in the dark,
	// so it is asserted rather than left to be rediscovered by stopwatch.
	UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>();
	PopulateDefaultExperienceCyclePlans(*Catalog);

	constexpr float BaseSeconds = 300.f; // UGloamsteadDayNightSubsystem::NightDurationSeconds default
	float ArcSeconds = 0.f;
	int32 AuthoredNights = 0;
	for (const FExperienceCyclePlan& Plan : Catalog->AuthoredPlans)
	{
		if (!Plan.IsAuthoredPlan())
		{
			continue;
		}
		++AuthoredNights;
		ArcSeconds += BaseSeconds * FDayNight::NightDurationScaleForType(Plan.NightType);
	}

	TestEqual(TEXT("the arc is six authored nights"), AuthoredNights, 6);

	// A floor and a ceiling rather than an exact number: the point is that the dark is a real
	// portion of the experience without becoming the whole of it. Nights are a CEILING - a player
	// who answers the objective ends one early - so this is the upper bound on time spent at night.
	// The band is the delivery target, not a guess: the condition this arc is built to satisfy is
	// "a solid 30 minutes", and the night is most of every cycle. Asserted so a later scalar tweak
	// cannot quietly halve the experience the way the 100s base did.
	TestTrue(FString::Printf(TEXT("the arc's night ceiling reaches at least 25 minutes (%.0fs)"), ArcSeconds),
		ArcSeconds >= 1500.f);
	TestTrue(FString::Printf(TEXT("and stays under 40, so no single night becomes a vigil (%.0fs)"), ArcSeconds),
		ArcSeconds <= 2400.f);

	return true;
}

// ---------------------------------------------------------------------------------------------

/**
 * The forged geometry is shaded, rather than wearing the engine grid.
 *
 * Both forged families were imported with `import_materials=False` - correct, a Houdini FBX should
 * not carry shading into this project - and then nothing assigned one. The kit meshes never showed
 * this because `AGloamsteadRestoredStructure` assigns an `MI_Sanctuary_*` to every body it builds;
 * the gloam growths and the threat shrouds went through a different path that only ever called
 * `SetStaticMesh`. An unassigned slot falls back to `WorldGridMaterial`, so the corruption the whole
 * night phase is about, and the silhouette the shroud exists to create, both rendered as a grey
 * checkerboard - which is worse than the bare mannequin the shroud was added to replace.
 *
 * A canvas-drawn or shaded surface cannot be asserted on by an automation suite, so this pins the
 * reachable half: that a real material instance is bound, that it is NOT the engine default, and
 * that the tint actually varies with the thing it is supposed to encode.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamForgedGeometryIsShadedTest,
	"Gloamstead.Forge.ForgedGeometryIsShadedRatherThanWearingTheEngineGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamForgedGeometryIsShadedTest::RunTest(const FString& /*Parameters*/)
{
	// The one material both families are shaded from must actually exist. This is a /Game path in a
	// string, which is exactly the failure a compiler cannot catch.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(
		nullptr, UGloamsteadCorruptionVisualizer::GetGrowthBaseMaterialPath());
	if (!TestNotNull(
			FString::Printf(TEXT("the shading base %s loads"),
				UGloamsteadCorruptionVisualizer::GetGrowthBaseMaterialPath()),
			Base))
	{
		return false;
	}

	// It must be a real Gloamstead material, not the engine fallback wearing a project path.
	TestFalse(TEXT("the shading base is not the engine grid material"),
		Base->GetName().Contains(TEXT("WorldGrid")));

	// Severity has to be legible in the tint, or three forged severity tiers all read the same.
	const FLinearColor Faint  = UGloamsteadCorruptionVisualizer::GetGrowthTintFor(0.20f);
	const FLinearColor Middle = UGloamsteadCorruptionVisualizer::GetGrowthTintFor(0.50f);
	const FLinearColor Deep   = UGloamsteadCorruptionVisualizer::GetGrowthTintFor(0.95f);

	TestFalse(TEXT("a faint bloom and a deep one are not the same colour"),
		Faint.Equals(Deep, 0.05f));
	// Monotone toward the dark end: worse corruption must never come back brighter, or the player
	// learns a colour rule the game then breaks.
	const auto Luma = [](const FLinearColor& C) { return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B; };
	TestTrue(TEXT("worse corruption is darker, never brighter"),
		Luma(Faint) > Luma(Middle) && Luma(Middle) > Luma(Deep));

	// And it stays a colour, not an accidental emissive: the art direction reserves luminous accents
	// for restoration, so rot that glowed would read as something the player wants.
	for (const FLinearColor& Tint : { Faint, Middle, Deep })
	{
		TestTrue(TEXT("the growth tint stays within an unlit range"),
			Tint.R <= 1.f && Tint.G <= 1.f && Tint.B <= 1.f
			&& Tint.R >= 0.f && Tint.G >= 0.f && Tint.B >= 0.f);
	}

	// The shroud path: every archetype must resolve a signature colour distinct enough to tell the
	// four apart once darkened onto the cloth.
	const ENightThreatArchetype Archetypes[] = {
		ENightThreatArchetype::Gatherer,
		ENightThreatArchetype::Borrowed,
		ENightThreatArchetype::Bargainer,
		ENightThreatArchetype::Echo,
	};
	for (int32 A = 0; A < UE_ARRAY_COUNT(Archetypes); ++A)
	{
		for (int32 B = A + 1; B < UE_ARRAY_COUNT(Archetypes); ++B)
		{
			const FLinearColor CoatA = AGloamsteadNightThreat::GetArchetypeGlowColor(Archetypes[A]) * 0.22f;
			const FLinearColor CoatB = AGloamsteadNightThreat::GetArchetypeGlowColor(Archetypes[B]) * 0.22f;
			TestFalse(
				FString::Printf(TEXT("%s and %s do not wear the same shroud colour"),
					*GetNightThreatArchetypeDisplayName(Archetypes[A]),
					*GetNightThreatArchetypeDisplayName(Archetypes[B])),
				CoatA.Equals(CoatB, 0.02f));
		}
	}

	return true;
}

// ---------------------------------------------------------------------------------------------

/**
 * The sanctuary has sound assets, and every phase can name one.
 *
 * `Content/` contained zero sound assets for the project's entire life. The C++ synth meant the game
 * was not silent, which is a different claim from having audio - and it let the absence sit
 * unnoticed, because nothing could fail. These are `/Game` paths in string literals, which is
 * exactly the failure a compiler cannot catch and a logic test does not think to ask about.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSanctuaryHasAVoiceInAssetsTest,
	"Gloamstead.Audio.EveryPhaseNamesABedAndEveryBedExists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSanctuaryHasAVoiceInAssetsTest::RunTest(const FString& /*Parameters*/)
{
	// Every phase the player can stand in must have a bed. Invalid deliberately has none.
	const EGloamsteadDayPhase Phases[] = {
		EGloamsteadDayPhase::Day,
		EGloamsteadDayPhase::Dusk,
		EGloamsteadDayPhase::Night,
		EGloamsteadDayPhase::Dawn,
	};

	TSet<FString> SeenPaths;
	for (const EGloamsteadDayPhase Phase : Phases)
	{
		const TCHAR* Path = UGloamsteadSoundscapeSubsystem::BedPathFor(Phase);
		const FString Name = GetGloamsteadDayPhaseDisplayName(Phase);

		if (!TestNotNull(FString::Printf(TEXT("%s names a bed"), *Name), Path))
		{
			continue;
		}

		// Distinct per phase: four phases sharing one bed would make the day/night cycle audibly
		// static, which is the thing the beds exist to prevent.
		bool bAlreadySeen = false;
		SeenPaths.Add(FString(Path), &bAlreadySeen);
		TestFalse(FString::Printf(TEXT("%s does not reuse another phase's bed"), *Name), bAlreadySeen);

		USoundBase* Bed = LoadObject<USoundBase>(nullptr, Path);
		if (TestNotNull(FString::Printf(TEXT("%s bed loads: %s"), *Name, Path), Bed))
		{
			// A bed of zero length would import, resolve, play nothing, and look correct from every
			// angle except listening.
			TestTrue(FString::Printf(TEXT("%s bed has real duration"), *Name),
				Bed->GetDuration() > 1.f);
		}
	}

	// The one-shots the loop fires. Same argument: a path that no longer resolves is silent, and
	// silence is exactly what this project could not previously distinguish from working.
	static const TCHAR* OneShots[] = {
		TEXT("/Game/Gloamstead/Audio/S_SFX_Heart_Warning.S_SFX_Heart_Warning"),
		TEXT("/Game/Gloamstead/Audio/S_SFX_Restoration.S_SFX_Restoration"),
		TEXT("/Game/Gloamstead/Audio/S_SFX_Threat_Near.S_SFX_Threat_Near"),
	};
	for (const TCHAR* Path : OneShots)
	{
		USoundBase* Cue = LoadObject<USoundBase>(nullptr, Path);
		if (TestNotNull(FString::Printf(TEXT("one-shot loads: %s"), Path), Cue))
		{
			TestTrue(FString::Printf(TEXT("one-shot has real duration: %s"), Path),
				Cue->GetDuration() > 0.1f);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
