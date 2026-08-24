// An authored ritual site must give a generated point its semantic contract.
//
// Cycles 2-4 resolve their target through PointMatchesExperiencePlan, which requires
// RecommendedForWarning, SemanticSubject, RitualType and RestorationTag to ALL agree with the authored
// plan. Before UGloamsteadRitualSiteComponent existed, the only writer of those attributes was
// Test_SetPointContractMetadata - compiled out of a player build - so those cycles could pass every test
// and still find nothing in a real game.
//
// These tests pin the binding pass itself, because a mechanism with no coverage is how that gap survived
// four cycles in the first place.
#include "Misc/AutomationTest.h"
#include "Components/GloamsteadRitualSiteComponent.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/RitualTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/NightConsequenceRuntime.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FScopedSiteWorld
	{
		UWorld* World = nullptr;

		FScopedSiteWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
			if (World)
			{
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World);
				FURL URL;
				World->InitializeActorsForPlay(URL);
				World->BeginPlay();
			}
		}

		~FScopedSiteWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
		}

		FScopedSiteWorld(const FScopedSiteWorld&) = delete;
		FScopedSiteWorld& operator=(const FScopedSiteWorld&) = delete;
	};

	AActor* SpawnSite(
		UWorld* World,
		const FVector& Location,
		FName SemanticSubject,
		FName WarningId,
		ERitualType RitualType,
		FName RestorationTag,
		double BindRadius = 2000.0)
	{
		AActor* Actor = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
		if (!Actor)
		{
			return nullptr;
		}

		// A bare AActor has no root component, so it cannot hold a transform: GetActorLocation() would
		// answer the origin no matter what was passed to SpawnActor, and every site would look like it
		// stood on top of the nearest point. Give it a root, then place it.
		USceneComponent* Root = NewObject<USceneComponent>(Actor);
		Root->RegisterComponent();
		Actor->SetRootComponent(Root);
		Actor->SetActorLocation(Location);
		UGloamsteadRitualSiteComponent* Site = NewObject<UGloamsteadRitualSiteComponent>(Actor);
		Site->SemanticSubject = SemanticSubject;
		Site->RecommendedForWarning = WarningId;
		Site->RitualType = RitualType;
		Site->RestorationTag = RestorationTag;
		Site->BindRadius = BindRadius;
		Site->RegisterComponent();
		return Actor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamAuthoredRitualSiteBindsContractTest,
	"Gloamstead.PCG.AuthoredSite.StampsTheFullContractOntoAPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamAuthoredRitualSiteBindsContractTest::RunTest(const FString& /*Parameters*/)
{
	// The shipped DA_RitualSiteCatalog is loaded by the same pass. It anchors the garden to the Veil
	// Heart, and these synthetic worlds contain no Heart, so it refuses - which is the correct fail-closed
	// answer, not a defect. Declaring both refusals keeps each test about its own subject.
	AddExpectedError(TEXT("is already claimed by a placed actor"), EAutomationExpectedErrorFlags::Contains, 0);

	FScopedSiteWorld Scoped;
	if (!Scoped.World)
	{
		AddError(TEXT("could not create a game world"));
		return false;
	}

	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>(Scoped.World);
	PCG->Test_SeedPoints({ FVector(0.0, 0.0, 0.0), FVector(5000.0, 0.0, 0.0) });

	// The seeded points are LanternPost (the attribute default), exactly like the shipping graph's output.
	// A GardenBed site must therefore still be able to claim its place.
	AActor* Site = SpawnSite(
		Scoped.World,
		FVector(100.0, 0.0, 0.0),
		TEXT("Cycle2_Garden"),
		TEXT("GardenRot"),
		ERitualType::GardenBed,
		TEXT("GardenBed"));
	if (!Site)
	{
		AddError(TEXT("could not spawn the ritual site actor"));
		return false;
	}

	PCG->Test_ApplyAuthoredSiteContracts();

	FPCGPoint Bound;
	if (!PCG->GetPointByIndex(0, Bound))
	{
		AddError(TEXT("point 0 disappeared"));
		return false;
	}

	TestEqual(TEXT("the nearest point carries the authored semantic subject"),
		PCG->GetNameAttribute(Bound, TEXT("SemanticSubject"), NAME_None), FName(TEXT("Cycle2_Garden")));
	TestEqual(TEXT("the nearest point carries the authored warning"),
		PCG->GetNameAttribute(Bound, TEXT("RecommendedForWarning"), NAME_None), FName(TEXT("GardenRot")));
	TestEqual(TEXT("the nearest point carries the authored restoration tag"),
		PCG->GetNameAttribute(Bound, TEXT("RestorationTag"), NAME_None), FName(TEXT("GardenBed")));

	// The whole point of the contract: the authored plan must now resolve this place.
	UExperienceCycleCatalog* Plans = NewObject<UExperienceCycleCatalog>();
	PopulateDefaultExperienceCyclePlans(*Plans);
	const FExperienceCyclePlan* Garden = Plans->AuthoredPlans.FindByPredicate(
		[](const FExperienceCyclePlan& P) { return P.PlanId == FName(TEXT("Cycle2_Garden")); });
	if (!Garden)
	{
		AddError(TEXT("the authored catalog has no Cycle2_Garden plan"));
		return false;
	}
	TestTrue(TEXT("the bound point satisfies the authored Cycle 2 plan"),
		PCG->PointMatchesExperiencePlan(0, *Garden, /*bRequireRestored*/ false));

	// The far point must be untouched - a site claims one place, not the map.
	FPCGPoint Far;
	if (PCG->GetPointByIndex(1, Far))
	{
		TestEqual(TEXT("the distant point keeps its empty subject"),
			PCG->GetNameAttribute(Far, TEXT("SemanticSubject"), NAME_None), FName(NAME_None));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamAuthoredRitualSiteResolvesForNightTest,
	"Gloamstead.PCG.AuthoredSite.NightRuntimeResolvesTheSubjectItStamped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamAuthoredRitualSiteResolvesForNightTest::RunTest(const FString& /*Parameters*/)
{
	// The shipped DA_RitualSiteCatalog is loaded by the same pass. It anchors the garden to the Veil
	// Heart, and these synthetic worlds contain no Heart, so it refuses - which is the correct fail-closed
	// answer, not a defect. Declaring both refusals keeps each test about its own subject.
	AddExpectedError(TEXT("is already claimed by a placed actor"), EAutomationExpectedErrorFlags::Contains, 0);

	// This closes the chain that was actually broken. Every other test of semantic targeting seeds the
	// contract through Test_SetPointContractMetadata, which is compiled out of a player build - so the
	// night could resolve its target under automation and find nothing in a real game. Here the ONLY thing
	// that writes the subject is an authored site going through the shipping binding pass.
	FScopedSiteWorld Scoped;
	if (!Scoped.World)
	{
		AddError(TEXT("could not create a game world"));
		return false;
	}

	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>(Scoped.World);
	PCG->Test_SeedPoints({ FVector(0.0, 0.0, 0.0), FVector(4000.0, 0.0, 0.0), FVector(9000.0, 0.0, 0.0) });

	if (!SpawnSite(
			Scoped.World,
			FVector(4100.0, 0.0, 0.0),
			TEXT("Cycle2_Garden"),
			TEXT("GardenRot"),
			ERitualType::GardenBed,
			TEXT("GardenBed")))
	{
		AddError(TEXT("could not spawn the garden site"));
		return false;
	}

	PCG->Test_ApplyAuthoredSiteContracts();

	UNightConsequenceRuntime* Runtime = Scoped.World->GetSubsystem<UNightConsequenceRuntime>();
	if (!Runtime)
	{
		AddError(TEXT("the world has no night consequence runtime"));
		return false;
	}

	const int32 Resolved = Runtime->ResolveSemanticSubjectToPoint(TEXT("Cycle2_Garden"), PCG);
	TestEqual(TEXT("the night runtime resolves the authored subject to the point the site claimed"), Resolved, 1);

	// An unauthored subject must still resolve to nothing - the binding pass grants exactly one place.
	AddExpectedError(TEXT("has no PCG mapping"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("an unauthored subject resolves to nothing"),
		Runtime->ResolveSemanticSubjectToPoint(TEXT("Cycle9_Nowhere"), PCG), INDEX_NONE);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamAuthoredRitualSiteRefusalsTest,
	"Gloamstead.PCG.AuthoredSite.RefusesIncompleteAndOutOfRangeDeclarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamAuthoredRitualSiteRefusalsTest::RunTest(const FString& /*Parameters*/)
{
	// The shipped DA_RitualSiteCatalog is loaded by the same pass. It anchors the garden to the Veil
	// Heart, and these synthetic worlds contain no Heart, so it refuses - which is the correct fail-closed
	// answer, not a defect. Declaring both refusals keeps each test about its own subject.
	AddExpectedError(TEXT("names a landmark this map does not contain"), EAutomationExpectedErrorFlags::Contains, 0);

	// An incomplete declaration names every missing field rather than binding something half-formed.
	UGloamsteadRitualSiteComponent* Bare = NewObject<UGloamsteadRitualSiteComponent>();
	TArray<FString> Problems;
	TestFalse(TEXT("a blank declaration is not complete"), Bare->IsCompleteDeclaration(Problems));
	TestEqual(TEXT("a blank declaration reports all four missing fields"), Problems.Num(), 4);

	UGloamsteadRitualSiteComponent* Partial = NewObject<UGloamsteadRitualSiteComponent>();
	Partial->SemanticSubject = TEXT("Cycle2_Garden");
	Partial->RecommendedForWarning = TEXT("GardenRot");
	Partial->RitualType = ERitualType::GardenBed;
	Problems.Reset();
	TestFalse(TEXT("a declaration missing only the restoration tag is still refused"),
		Partial->IsCompleteDeclaration(Problems));
	TestEqual(TEXT("it reports exactly the one missing field"), Problems.Num(), 1);

	UGloamsteadRitualSiteComponent* Complete = NewObject<UGloamsteadRitualSiteComponent>();
	Complete->SemanticSubject = TEXT("Cycle2_Garden");
	Complete->RecommendedForWarning = TEXT("GardenRot");
	Complete->RitualType = ERitualType::GardenBed;
	Complete->RestorationTag = TEXT("GardenBed");
	Problems.Reset();
	TestTrue(TEXT("a fully authored declaration is complete"), Complete->IsCompleteDeclaration(Problems));

	// Out of range binds nothing at all, rather than reaching across the map for a point.
	FScopedSiteWorld Scoped;
	if (!Scoped.World)
	{
		AddError(TEXT("could not create a game world"));
		return false;
	}

	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>(Scoped.World);
	PCG->Test_SeedPoints({ FVector(0.0, 0.0, 0.0) });

	if (!SpawnSite(
			Scoped.World,
			FVector(100000.0, 0.0, 0.0),
			TEXT("Cycle2_Garden"),
			TEXT("GardenRot"),
			ERitualType::GardenBed,
			TEXT("GardenBed"),
			/*BindRadius*/ 500.0))
	{
		AddError(TEXT("could not spawn the distant ritual site actor"));
		return false;
	}

	// Refusing loudly is the POINT of this path, so the error is expected evidence, not a failure. Declaring
	// it also asserts the refusal actually reached the log: if the binding silently did nothing, this
	// expectation would go unmet and the test would fail for the opposite reason.
	AddExpectedError(
		TEXT("binds nothing - no unclaimed generated point"),
		EAutomationExpectedErrorFlags::Contains,
		1);

	PCG->Test_ApplyAuthoredSiteContracts();

	FPCGPoint Untouched;
	if (PCG->GetPointByIndex(0, Untouched))
	{
		TestEqual(TEXT("a site outside BindRadius stamps nothing"),
			PCG->GetNameAttribute(Untouched, TEXT("SemanticSubject"), NAME_None), FName(NAME_None));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
