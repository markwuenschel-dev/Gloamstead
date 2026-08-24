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
// This test deliberately leaves WarningCatalog null so the REAL asset load path runs, then pins the
// invariant that actually matters: every authored plan can find an exact warning row for its
// (WarningId, NightType) pair. EnsureWarningCatalog no longer synthesizes any row, so this asserts the
// shipped /Game/Data/DA_VeilHeartWarningCatalog itself - not a runtime substitute for it.
//
// It shares one authority with the runtime load path: both call
// ValidateWarningCatalogCoversAuthoredPlans, so the rule cannot drift between what ships and what is
// tested.
#include "Misc/AutomationTest.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Systems/VeilHeart.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

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

	// UWorld::CreateWorld test worlds are brought up manually, and actors spawned after that setup do not
	// automatically receive BeginPlay in every editor automation configuration (same rationale as
	// PlayableCycleTests.cpp:76-86). Without this the Heart never runs EnsureWarningCatalog, and the shipped
	// asset is never actually exercised - the test would report "no catalog" while the content was fine.
	if (!Heart->HasActorBegunPlay())
	{
		Heart->DispatchBeginPlay();
	}

	if (!Heart->WarningCatalog)
	{
		// Diagnose WHY rather than just reporting the symptom: a missing package, a package that exists but
		// will not load, and a load that succeeds but casts to the wrong class are three different defects.
		static const TCHAR* const PackageName = TEXT("/Game/Data/DA_VeilHeartWarningCatalog");
		static const TCHAR* const ObjectPath = TEXT("/Game/Data/DA_VeilHeartWarningCatalog.DA_VeilHeartWarningCatalog");
		const bool bPackageExists = FPackageName::DoesPackageExist(PackageName);
		UObject* AnyClass = StaticLoadObject(UObject::StaticClass(), nullptr, ObjectPath);
		UObject* TypedLoad = StaticLoadObject(UVeilHeartWarningCatalog::StaticClass(), nullptr, ObjectPath);
		// Discriminate the two causes that both surface as a null catalog: the Heart refused an asset that
		// loaded but failed the contract, versus the load itself returning null at BeginPlay time and only
		// succeeding on a later attempt.
		FString ContractVerdict = TEXT("could not re-load to check");
		if (UVeilHeartWarningCatalog* Reloaded = Cast<UVeilHeartWarningCatalog>(TypedLoad))
		{
			TArray<FString> Problems;
			if (ValidateWarningCatalogCoversAuthoredPlans(*Reloaded, Problems))
			{
				ContractVerdict = FString::Printf(
					TEXT("the loaded catalog SATISFIES the contract (%d rows) - so the BeginPlay load itself returned null, not a contract refusal"),
					Reloaded->Warnings.Num());
			}
			else
			{
				ContractVerdict = FString::Printf(TEXT("the loaded catalog FAILS the contract with %d defect(s): %s"),
					Problems.Num(), *FString::Join(Problems, TEXT(" | ")));
			}
		}
		AddError(FString::Printf(
			TEXT("BeginPlay resolved no warning catalog. DoesPackageExist=%s; StaticLoadObject(UObject)=%s (class=%s); typed=%s. %s"),
			bPackageExists ? TEXT("true") : TEXT("false"),
			AnyClass ? TEXT("loaded") : TEXT("null"),
			AnyClass ? *AnyClass->GetClass()->GetName() : TEXT("n/a"),
			TypedLoad ? TEXT("loaded") : TEXT("null"),
			*ContractVerdict));
		return false;
	}

	TArray<FString> Problems;
	const bool bCovered = ValidateWarningCatalogCoversAuthoredPlans(*Heart->WarningCatalog, Problems);
	for (const FString& Problem : Problems)
	{
		AddError(FString::Printf(TEXT("shipped warning catalog: %s"), *Problem));
	}
	TestTrue(TEXT("the shipped warning catalog covers every authored plan"), bCovered);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
