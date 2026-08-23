// Cycle II WorldForge projection contract: Gloamstead owns semantic truth;
// WorldForge receives only the derived generic restoration-level mirror.
#include "Misc/AutomationTest.h"

#include "Data/ExperienceCycleTypes.h"
#include "Data/RitualTypes.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Save/GloamsteadSaveGame.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/GloamsteadWorldStateProjectionSubsystem.h"
#include "WorldStateSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float ExpectedUntouchedRestorationLevel = 0.0f;
	constexpr float ExpectedRestoredRestorationLevel = 1.0f;
	const FName Cycle2GardenRegionId(TEXT("Cycle2_Garden"));
	const FName RestorationLevelKey(TEXT("restoration_level"));

	struct FWorldStateProjectionScopedWorld
	{
		UWorld* World = nullptr;
		UGameInstance* GameInstance = nullptr;

		FWorldStateProjectionScopedWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
			if (World && GEngine)
			{
				GameInstance = NewObject<UGameInstance>(GEngine);
				GameInstance->Init();
				World->SetGameInstance(GameInstance);
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.OwningGameInstance = GameInstance;
				Context.SetCurrentWorld(World);
				FURL URL;
				World->InitializeActorsForPlay(URL);
				World->BeginPlay();
			}
		}

		~FWorldStateProjectionScopedWorld()
		{
			if (World)
			{
				if (GEngine)
				{
					GEngine->DestroyWorldContext(World);
				}
				World->DestroyWorld(false);
				World = nullptr;
			}
		}

		FWorldStateProjectionScopedWorld(const FWorldStateProjectionScopedWorld&) = delete;
		FWorldStateProjectionScopedWorld& operator=(const FWorldStateProjectionScopedWorld&) = delete;
	};

	FExperienceCyclePlan MakeCycle2GardenPlan()
	{
		UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>();
		PopulateDefaultExperienceCyclePlans(*Catalog);
		return Catalog->AuthoredPlans[1];
	}

	FRestorationEventPayload MakeForgedRestorationPayload(int32 PointIndex)
	{
		// The projection must ignore every literal here. PCG metadata and restored
		// state are the only source for the generic WorldForge mirror.
		FRestorationEventPayload Payload;
		Payload.PointIndex = PointIndex;
		Payload.WarningId = TEXT("Forged.Warning");
		Payload.SemanticSubject = TEXT("Forged.Subject");
		Payload.WarningTagSatisfied = TEXT("Forged.Tag");
		Payload.RitualType = ERitualType::LanternPost;
		return Payload;
	}

	struct FWorldStateProjectionFixture
	{
		FWorldStateProjectionScopedWorld ScopedWorld;
		UGloamsteadPCGSubsystem* PCG = nullptr;
		UGloamsteadWorldStateProjectionSubsystem* Projection = nullptr;
		UWorldStateSubsystem* WorldState = nullptr;
		UGloamsteadExperienceCycleSubsystem* Experience = nullptr;
		FExperienceCyclePlan GardenPlan;
		bool bReady = false;

		FWorldStateProjectionFixture()
		{
			if (!ScopedWorld.World || !ScopedWorld.GameInstance)
			{
				return;
			}

			PCG = ScopedWorld.World->GetSubsystem<UGloamsteadPCGSubsystem>();
			Projection = ScopedWorld.World->GetSubsystem<UGloamsteadWorldStateProjectionSubsystem>();
			WorldState = ScopedWorld.World->GetSubsystem<UWorldStateSubsystem>();
			Experience = ScopedWorld.GameInstance->GetSubsystem<UGloamsteadExperienceCycleSubsystem>();
			if (!PCG || !Projection || !WorldState || !Experience)
			{
				return;
			}

			GardenPlan = MakeCycle2GardenPlan();
			UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>(ScopedWorld.GameInstance);
			PopulateDefaultExperienceCyclePlans(*Catalog);
			Experience->Test_SetCatalog(Catalog);
			FExperienceCyclePersistentState CycleTwoState;
			CycleTwoState.CompletedCycleSlot = 1;
			CycleTwoState.bFirstRestCompleted = true;
			CycleTwoState.ArmedPlanId = GardenPlan.PlanId;
			if (!Experience->RestorePersistentState(CycleTwoState))
			{
				return;
			}

			PCG->Test_SeedPoints({ FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f) });
			TArray<FRitualPointState> States;
			States.SetNum(2);
			PCG->Test_SeedPointStates(States);
			const bool bExactTargetAuthored = PCG->Test_SetPointContractMetadata(
				0, GardenPlan.WarningId, GardenPlan.SemanticSubject,
				GardenPlan.RequiredRitualType, GardenPlan.RequiredRestorationTags[0]);
			const bool bDecoyAuthored = PCG->Test_SetPointContractMetadata(
				1, TEXT("GardenRot.Decoy"), GardenPlan.SemanticSubject,
				GardenPlan.RequiredRitualType, GardenPlan.RequiredRestorationTags[0]);
			bReady = bExactTargetAuthored && bDecoyAuthored;
		}

		float GetMirroredRestorationLevel() const
		{
			return WorldState->GetStateValue(
				EWorldForgeStateScope::Region,
				Cycle2GardenRegionId,
				RestorationLevelKey,
				-1.0f);
		}
	};

	bool LoadCycle2WorldSpecification(FString& OutSpecification)
	{
		return FFileHelper::LoadFileToString(
			OutSpecification,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("specs/world/cycle-2-corruption-neglect.world.json")));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamWorldStateProjectionMirrorsOnlyExactCycle2GardenTargetTest,
	"Gloamstead.WorldForge.Projection.MirrorsOnlyExactCycle2GardenTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamWorldStateProjectionMirrorsOnlyExactCycle2GardenTargetTest::RunTest(const FString& /*Parameters*/)
{
	FWorldStateProjectionFixture Fixture;
	if (!TestTrue(TEXT("fixture owns real PCG, projection, WorldForge, and experience subsystems"), Fixture.bReady))
	{
		return false;
	}

	Fixture.Projection->RebuildFromAuthoritativeState();
	TestEqual(TEXT("an untouched exact Cycle II garden projects zero"),
		Fixture.GetMirroredRestorationLevel(), ExpectedUntouchedRestorationLevel);

	const FExperienceCyclePlan ActivePlanBeforeProjection = Fixture.Experience->GetActivePlan();
	const FExperienceCyclePersistentState ExperienceStateBeforeProjection = Fixture.Experience->CapturePersistentState();
	TestTrue(TEXT("a same-ritual GardenBed decoy can be restored as ordinary PCG state"),
		Fixture.PCG->ApplyRestoration(1, MakeForgedRestorationPayload(1)));
	TestEqual(TEXT("an unrelated GardenBed decoy leaves the Cycle II mirror at zero"),
		Fixture.GetMirroredRestorationLevel(), ExpectedUntouchedRestorationLevel);
	TestTrue(TEXT("the exact Cycle II PCG target can be restored through the actual restoration event"),
		Fixture.PCG->ApplyRestoration(0, MakeForgedRestorationPayload(0)));
	TestEqual(TEXT("the restored exact Cycle II garden projects one"),
		Fixture.GetMirroredRestorationLevel(), ExpectedRestoredRestorationLevel);
	TestTrue(TEXT("a reclaimed exact Cycle II garden changes the authoritative restoration flag"),
		Fixture.PCG->RevertRestoration(0));
	TestEqual(TEXT("a real reclamation immediately projects the exact Cycle II garden back to zero"),
		Fixture.GetMirroredRestorationLevel(), ExpectedUntouchedRestorationLevel);
	TestFalse(TEXT("an ordinary native write cannot override the projection's reserved mirror"),
		Fixture.WorldState->SetStateValue(EWorldForgeStateScope::Region, Cycle2GardenRegionId,
			RestorationLevelKey, ExpectedRestoredRestorationLevel));
	TestFalse(TEXT("reclaiming an already-unrestored exact Cycle II garden makes no mutation"),
		Fixture.PCG->RevertRestoration(0));
	TestEqual(TEXT("a rejected ordinary write and rejected reclamation preserve the reserved mirror"),
		Fixture.GetMirroredRestorationLevel(), ExpectedUntouchedRestorationLevel);
	Fixture.Projection->RebuildFromAuthoritativeState();
	TestEqual(TEXT("the public projection rebuild still derives zero from the reclaimed authoritative target"),
		Fixture.GetMirroredRestorationLevel(), ExpectedUntouchedRestorationLevel);
	TestTrue(TEXT("a reclaimed exact Cycle II garden can be restored again through the actual restoration event"),
		Fixture.PCG->ApplyRestoration(0, MakeForgedRestorationPayload(0)));
	TestEqual(TEXT("the restored exact Cycle II garden returns to one after the new restoration"),
		Fixture.GetMirroredRestorationLevel(), ExpectedRestoredRestorationLevel);
	const TArray<FRitualPointState> AuthoritativeSnapshotBeforeRebuild = Fixture.PCG->Test_PeekPointStates();

	TestFalse(TEXT("an ordinary native write cannot reset the reserved WorldForge mirror"),
		Fixture.WorldState->SetStateValue(EWorldForgeStateScope::Region, Cycle2GardenRegionId,
			RestorationLevelKey, ExpectedUntouchedRestorationLevel));
	TestEqual(TEXT("the rejected ordinary write preserves the restored mirror"),
		Fixture.GetMirroredRestorationLevel(), ExpectedRestoredRestorationLevel);
	Fixture.Projection->RebuildFromAuthoritativeState();
	TestEqual(TEXT("the public rebuild seam restores the mirror from authoritative PCG state"),
		Fixture.GetMirroredRestorationLevel(), ExpectedRestoredRestorationLevel);
	bool bProjectionPreservedPCGSnapshot = Fixture.PCG->Test_PeekPointStates().Num() == AuthoritativeSnapshotBeforeRebuild.Num();
	for (int32 Index = 0; bProjectionPreservedPCGSnapshot && Index < AuthoritativeSnapshotBeforeRebuild.Num(); ++Index)
	{
		const FRitualPointState& Before = AuthoritativeSnapshotBeforeRebuild[Index];
		const FRitualPointState& After = Fixture.PCG->Test_PeekPointStates()[Index];
		bProjectionPreservedPCGSnapshot = Before.bIsRestored == After.bIsRestored
			&& FMath::IsNearlyEqual(Before.LightLevel, After.LightLevel)
			&& FMath::IsNearlyEqual(Before.CorruptionLevel, After.CorruptionLevel);
	}
	TestTrue(TEXT("the public projection rebuild never alters the authoritative PCG snapshot"), bProjectionPreservedPCGSnapshot);

	UGloamsteadSaveGame* SavedPCGState = NewObject<UGloamsteadSaveGame>();
	Fixture.PCG->CaptureToSaveGame(SavedPCGState);
	const TSet<int32> RestoredIndicesBeforeReconstruction = Fixture.PCG->GetRestoredPointIndices();
	TestTrue(TEXT("an authoritative reclamation establishes zero before the versioned load callback"),
		Fixture.PCG->RevertRestoration(0));
	TestEqual(TEXT("the authoritative reclamation projects zero before the versioned load callback"),
		Fixture.GetMirroredRestorationLevel(), ExpectedUntouchedRestorationLevel);
	TestFalse(TEXT("an ordinary native write remains rejected while load starts from zero"),
		Fixture.WorldState->SetStateValue(EWorldForgeStateScope::Region, Cycle2GardenRegionId,
			RestorationLevelKey, ExpectedRestoredRestorationLevel));
	TestTrue(TEXT("a versioned authoritative PCG load succeeds"), Fixture.PCG->RestoreFromSaveGame(SavedPCGState));
	TestEqual(TEXT("an authoritative PCG load projects one from the restored snapshot"),
		Fixture.GetMirroredRestorationLevel(), ExpectedRestoredRestorationLevel);

	TestTrue(TEXT("an authoritative reclamation establishes zero before the reconstruction callback"),
		Fixture.PCG->RevertRestoration(0));
	TestEqual(TEXT("the authoritative reclamation projects zero before the reconstruction callback"),
		Fixture.GetMirroredRestorationLevel(), ExpectedUntouchedRestorationLevel);
	TestFalse(TEXT("an ordinary native write remains rejected while reconstruction starts from zero"),
		Fixture.WorldState->SetStateValue(EWorldForgeStateScope::Region, Cycle2GardenRegionId,
			RestorationLevelKey, ExpectedRestoredRestorationLevel));
	Fixture.PCG->ReapplyRestoredState(RestoredIndicesBeforeReconstruction);
	TestEqual(TEXT("an authoritative PCG reconstruction projects one from the restored-index snapshot"),
		Fixture.GetMirroredRestorationLevel(), ExpectedRestoredRestorationLevel);
	TestTrue(TEXT("projection rebuilds never alter the active authored plan"),
		Fixture.Experience->GetActivePlan().PlanId == ActivePlanBeforeProjection.PlanId
		&& Fixture.Experience->GetActivePlan().WarningId == ActivePlanBeforeProjection.WarningId);
	const FExperienceCyclePersistentState ExperienceStateAfterProjection = Fixture.Experience->CapturePersistentState();
	TestEqual(TEXT("projection rebuilds never choose or rewrite the persistent night outcome"),
		ExperienceStateAfterProjection.LastOutcomeResultTag, ExperienceStateBeforeProjection.LastOutcomeResultTag);
	TestEqual(TEXT("projection rebuilds never advance the completed authored-cycle slot"),
		ExperienceStateAfterProjection.CompletedCycleSlot, ExperienceStateBeforeProjection.CompletedCycleSlot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamWorldStateProjectionValidatesCycle2WorldSpecificationTest,
	"Gloamstead.WorldForge.Specification.RejectsAmbiguousOrIncompleteCycle2World",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamWorldStateProjectionValidatesCycle2WorldSpecificationTest::RunTest(const FString& /*Parameters*/)
{
	FString Specification;
	if (!TestTrue(TEXT("the authored Cycle II world specification is available to automation"), LoadCycle2WorldSpecification(Specification)))
	{
		return false;
	}

	FString Error;
	TestTrue(TEXT("the authored Cycle II world specification passes the deterministic semantic validator"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(Specification, &Error));
	TestTrue(TEXT("the authored Cycle II world specification names its bounded Gloamstead-owned POI"),
		Specification.Contains(TEXT("\"poi\"")));
	TestTrue(TEXT("the authored Cycle II world specification names its deterministic generation input"),
		Specification.Contains(TEXT("\"generationInput\"")));

	const FString OutputEscape = Specification.Replace(
		TEXT("/Game/Generated/WorldForge/Cycle2/"), TEXT("/Game/Generated/WorldForge/Outside/"));
	TestFalse(TEXT("an output-root escape is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(OutputEscape, &Error));

	const FString UnknownRootProperty = Specification.Replace(
		TEXT("\n}"), TEXT(",\n  \"unexpectedRootProperty\": true\n}"), ESearchCase::CaseSensitive);
	TestFalse(TEXT("an unknown root property is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(UnknownRootProperty, &Error));

	const FString UnknownNestedProperty = Specification.Replace(
		TEXT("\"root\": \"/Game/Generated/WorldForge/Cycle2/\"\n  }"),
		TEXT("\"root\": \"/Game/Generated/WorldForge/Cycle2/\",\n    \"unexpectedOutputProperty\": true\n  }"),
		ESearchCase::CaseSensitive);
	TestFalse(TEXT("an unknown nested object property is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(UnknownNestedProperty, &Error));

	const FString ExtraAnchor = Specification.Replace(
		TEXT("    }\n  ],\n  \"poi\""),
		TEXT("    },\n    {\n      \"anchorId\": \"Cycle2_Garden.DecoyAnchor\",\n      \"mapAsset\": \"/Game/Maps/Lvl_Gloamstead\",\n      \"surveyId\": \"cycle2-garden-decoy-anchor\"\n    }\n  ],\n  \"poi\""),
		ESearchCase::CaseSensitive);
	TestFalse(TEXT("an extra or decoy anchor is rejected instead of being ignored"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(ExtraAnchor, &Error));

	const FString ExtraSubject = Specification.Replace(
		TEXT("    }\n  ],\n  \"evidence\""),
		TEXT("    },\n    {\n      \"subjectId\": \"Cycle2_Garden.DecoySubject\",\n      \"warningId\": \"GardenRot.Decoy\",\n      \"ritualType\": \"GardenBed\",\n      \"restorationTag\": \"GardenBed\",\n      \"surveyId\": \"cycle2-garden-decoy-subject\"\n    }\n  ],\n  \"evidence\""),
		ESearchCase::CaseSensitive);
	TestFalse(TEXT("an extra or decoy subject is rejected instead of being ignored"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(ExtraSubject, &Error));

	const FString DuplicateSurveyId = Specification.Replace(
		TEXT("cycle2-garden-subject"), TEXT("cycle2-garden-anchor"));
	TestFalse(TEXT("duplicate or ambiguous survey identifiers are rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(DuplicateSurveyId, &Error));

	const FString MissingMap = Specification.Replace(
		TEXT("/Game/Maps/Lvl_Gloamstead"), TEXT(""));
	TestFalse(TEXT("a missing target map is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(MissingMap, &Error));

	const FString MissingAnchor = Specification.Replace(
		TEXT("Cycle2_Garden.Anchor"), TEXT(""));
	TestFalse(TEXT("a missing authored anchor is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(MissingAnchor, &Error));

	const FString MissingPoi = Specification.Replace(TEXT("\"poi\""), TEXT("\"missingPoi\""), ESearchCase::CaseSensitive);
	TestFalse(TEXT("a missing bounded POI is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(MissingPoi, &Error));

	const FString MissingGenerationInput = Specification.Replace(TEXT("\"generationInput\""), TEXT("\"missingGenerationInput\""), ESearchCase::CaseSensitive);
	TestFalse(TEXT("missing deterministic generation input is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(MissingGenerationInput, &Error));

	const FString InvalidGenerationSeed = Specification.Replace(TEXT("\"seed\": 42"), TEXT("\"seed\": 43"), ESearchCase::CaseSensitive);
	TestFalse(TEXT("an invalid deterministic generation seed is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(InvalidGenerationSeed, &Error));

	const FString InvalidGenerationInputVersion = Specification.Replace(
		TEXT("gloamstead-cycle2-corruption-neglect.v1"), TEXT("gloamstead-cycle2-corruption-neglect.v2"), ESearchCase::CaseSensitive);
	TestFalse(TEXT("an invalid deterministic generation input version is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(InvalidGenerationInputVersion, &Error));

	const FString MalformedPoiTranslation = Specification.Replace(
		TEXT("[480.0, 160.0, 0.0]"), TEXT("[480.0, 160.0]"), ESearchCase::CaseSensitive);
	TestFalse(TEXT("a malformed POI anchor translation is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(MalformedPoiTranslation, &Error));

	const FString OutOfBoundsPoiTranslation = Specification.Replace(
		TEXT("[480.0, 160.0, 0.0]"), TEXT("[640.0, 160.0, 0.0]"), ESearchCase::CaseSensitive);
	TestFalse(TEXT("a POI that escapes the source-owned sanctuary bounds is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(OutOfBoundsPoiTranslation, &Error));

	const FString NonPositivePoiBounds = Specification.Replace(
		TEXT("[240.0, 280.0, 160.0]"), TEXT("[0.0, 280.0, 160.0]"), ESearchCase::CaseSensitive);
	TestFalse(TEXT("non-positive POI box half extents are rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(NonPositivePoiBounds, &Error));

	const FString MissingEvidence = Specification.Replace(
		TEXT("GardenRot.BellMoths"), TEXT("GardenRot.MissingEvidence"));
	TestFalse(TEXT("a missing GardenRot evidence binding is rejected"),
		UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(MissingEvidence, &Error));

	for (const TCHAR* RequiredCategory : { TEXT("foliage"), TEXT("ruins"), TEXT("paths"), TEXT("lighting_materials") })
	{
		const FString MissingReactiveCategory = Specification.Replace(RequiredCategory, TEXT("omitted_reactive_category"));
		TestFalse(FString::Printf(TEXT("missing required reactive category %s is rejected"), RequiredCategory),
			UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(MissingReactiveCategory, &Error));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
