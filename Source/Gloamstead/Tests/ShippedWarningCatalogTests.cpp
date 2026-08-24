// The shipped warning catalog must answer every authored cycle plan.
//
// Every other warning test injects a synthetic UVeilHeartWarningCatalog directly into
// AVeilHeart::WarningCatalog, which makes EnsureWarningCatalog() short-circuit and never
// consult /Game/Data/DA_VeilHeartWarningCatalog. That blind spot let a shipped-content
// regression through: the asset carried no TutorialLostPath/Tutorial row, so
// CanPresentWarningForPlan() refused Cycle 1, PresentedPlanId stayed unset, and
// UGloamsteadDayNightSubsystem::CanRestNow() denied rest forever - the first lantern could
// be restored but the first night could never begin, with all other tests green.
//
// This test deliberately leaves WarningCatalog null so the real load path plus its in-memory
// fallbacks run, then pins the invariant that actually matters: every authored plan can find
// an exact warning row for its (WarningId, NightType) pair.
#include "Misc/AutomationTest.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Systems/VeilHeart.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FScopedWarningCatalogWorld
	{
		UWorld* World = nullptr;

		FScopedWarningCatalogWorld()
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

		~FScopedWarningCatalogWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
		}

		FScopedWarningCatalogWorld(const FScopedWarningCatalogWorld&) = delete;
		FScopedWarningCatalogWorld& operator=(const FScopedWarningCatalogWorld&) = delete;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamShippedWarningCatalogCoversAuthoredPlansTest,
	"Gloamstead.Experience.Warning.ShippedCatalogCoversEveryAuthoredPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamShippedWarningCatalogCoversAuthoredPlansTest::RunTest(const FString& /*Parameters*/)
{
	FScopedWarningCatalogWorld Scoped;
	if (!Scoped.World)
	{
		AddError(TEXT("could not create a game world for the shipped-catalog check"));
		return false;
	}

	// Left null on purpose: this is the one test that exercises the real asset load path.
	AVeilHeart* Heart = Scoped.World->SpawnActor<AVeilHeart>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Heart)
	{
		AddError(TEXT("could not spawn a Veil Heart"));
		return false;
	}

	if (!Heart->WarningCatalog)
	{
		AddError(TEXT("BeginPlay resolved no warning catalog at all - /Game/Data/DA_VeilHeartWarningCatalog failed to load, so no authored night can present its warning."));
		return false;
	}

	UExperienceCycleCatalog* Plans = NewObject<UExperienceCycleCatalog>();
	PopulateDefaultExperienceCyclePlans(*Plans);
	TestTrue(TEXT("there is at least one authored plan to cover"), Plans->AuthoredPlans.Num() > 0);

	for (const FExperienceCyclePlan& Plan : Plans->AuthoredPlans)
	{
		if (!Plan.IsAuthoredPlan())
		{
			continue;
		}

		int32 MatchCount = 0;
		for (const FVeilHeartWarningFragment& Warning : Heart->WarningCatalog->Warnings)
		{
			if (Warning.WarningId == Plan.WarningId && Warning.AssociatedNightType == Plan.NightType)
			{
				++MatchCount;
			}
		}

		// FindExactWarningById refuses an ambiguous match, so two rows fail exactly like zero.
		TestEqual(
			*FString::Printf(
				TEXT("slot %d (%s) resolves exactly one shipped warning row for %s/%s"),
				Plan.Slot,
				*Plan.PlanId.ToString(),
				*Plan.WarningId.ToString(),
				*GetNightConsequenceTypeDisplayName(Plan.NightType)),
			MatchCount,
			1);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
