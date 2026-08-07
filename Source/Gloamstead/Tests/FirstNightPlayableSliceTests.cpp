#include "Misc/AutomationTest.h"
#include "Components/RitualPlacementComponent.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "PCG/GloamsteadSanctuaryBootstrap.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GloamFirstNightPlayableSlice
{
	struct FScopedWorld
	{
		UWorld* World = nullptr;

		FScopedWorld()
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

		~FScopedWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	struct FScopedDefaultSaveSlot
	{
		const FString Slot = UGloamsteadPCGSubsystem::DefaultSaveSlot;
		const bool bHadSave;
		TStrongObjectPtr<USaveGame> Backup;

		FScopedDefaultSaveSlot()
			: bHadSave(UGameplayStatics::DoesSaveGameExist(Slot, 0))
			, Backup(bHadSave ? UGameplayStatics::LoadGameFromSlot(Slot, 0) : nullptr)
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}

		~FScopedDefaultSaveSlot()
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
			if (bHadSave && Backup.IsValid())
			{
				UGameplayStatics::SaveGameToSlot(Backup.Get(), Slot, 0);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamConfiguredLanternMaterializesTest,
	"Gloamstead.FirstNight.PlayableSlice.ConfiguredLanternMaterializes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamConfiguredLanternMaterializesTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamFirstNightPlayableSlice;

	FScopedWorld Scope;
	if (!TestNotNull(TEXT("live world created"), Scope.World))
	{
		return false;
	}

	UGloamsteadPCGSubsystem* PCG = Scope.World->GetSubsystem<UGloamsteadPCGSubsystem>();
	if (!TestNotNull(TEXT("world PCG subsystem exists"), PCG))
	{
		return false;
	}
	const FVector RitualLocation(320.0f, -180.0f, 40.0f);
	PCG->Test_SeedPoints({RitualLocation});
	PCG->Test_SeedPointStates({FRitualPointState()});

	AActor* Owner = Scope.World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("placement owner spawned"), Owner))
	{
		return false;
	}
	URitualPlacementComponent* Placement = NewObject<URitualPlacementComponent>(Owner);
	Placement->RegisterComponent();

	FClassProperty* RestoredClassProperty = FindFProperty<FClassProperty>(
		URitualPlacementComponent::StaticClass(), TEXT("LanternPostRestoredClass"));
	if (!TestNotNull(TEXT("designer-facing lantern class exists"), RestoredClassProperty))
	{
		return false;
	}
	RestoredClassProperty->SetPropertyValue_InContainer(Placement, AStaticMeshActor::StaticClass());

	AActor* SpawnedActor = nullptr;
	Placement->SpawnRestoredActor(0, SpawnedActor);

	if (TestNotNull(TEXT("configured lantern materializes"), SpawnedActor))
	{
		TestEqual(TEXT("lantern uses the configured class"), SpawnedActor->GetClass(), AStaticMeshActor::StaticClass());
		TestTrue(TEXT("lantern is placed at the ritual point with presentation offset"),
			SpawnedActor->GetActorLocation().Equals(RitualLocation + FVector(0.0f, 0.0f, 12.0f), KINDA_SMALL_NUMBER));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamMissingLanternClassStaysDegradedTest,
	"Gloamstead.FirstNight.PlayableSlice.MissingLanternClassSpawnsNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamMissingLanternClassStaysDegradedTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamFirstNightPlayableSlice;

	FScopedWorld Scope;
	if (!TestNotNull(TEXT("live world created"), Scope.World))
	{
		return false;
	}
	UGloamsteadPCGSubsystem* PCG = Scope.World->GetSubsystem<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPoints({FVector::ZeroVector});
	PCG->Test_SeedPointStates({FRitualPointState()});

	AActor* Owner = Scope.World->SpawnActor<AActor>();
	URitualPlacementComponent* Placement = NewObject<URitualPlacementComponent>(Owner);
	Placement->LanternPostRestoredClass = nullptr;
	Placement->bUseProjectDefaultLanternPostClass = false;
	Placement->RegisterComponent();

	AActor* SpawnedActor = nullptr;
	Placement->SpawnRestoredActor(0, SpawnedActor);
	TestNull(TEXT("missing configuration produces no restored actor"), SpawnedActor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLanternConfirmationIsSingleShotTest,
	"Gloamstead.FirstNight.PlayableSlice.LanternConfirmationMaterializesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLanternConfirmationIsSingleShotTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamFirstNightPlayableSlice;

	FScopedWorld Scope;
	if (!TestNotNull(TEXT("live world created"), Scope.World))
	{
		return false;
	}
	UGloamsteadPCGSubsystem* PCG = Scope.World->GetSubsystem<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPoints({FVector(250.0f, 0.0f, 0.0f)});
	PCG->Test_SeedPointStates({FRitualPointState()});

	AStaticMeshActor* Owner = Scope.World->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	URitualPlacementComponent* Placement = NewObject<URitualPlacementComponent>(Owner);
	Placement->LanternPostRestoredClass = AStaticMeshActor::StaticClass();
	Placement->RegisterComponent();

	Placement->EnterPlacementMode();
	TestTrue(TEXT("the seeded lantern point is a valid target"), Placement->IsCurrentPlacementValid());
	TestTrue(TEXT("the first confirmation restores the lantern"), Placement->ConfirmPlacement());
	TestTrue(TEXT("the point is restored"), PCG->IsPointRestored(0));
	TestFalse(TEXT("a materialized lantern is not reported as GSS016"), Placement->WasLastRestoredActorMissing());
	TestFalse(TEXT("the evidence does not carry GSS016"),
		Placement->GetLastEvidenceFailureCodes().Contains(URitualPlacementComponent::GSSRestoredActorMissing));

	int32 RestoredLanternCount = 0;
	for (TActorIterator<AActor> It(Scope.World); It; ++It)
	{
		RestoredLanternCount += It->ActorHasTag(TEXT("Gloamstead.RestoredLantern")) ? 1 : 0;
	}
	TestEqual(TEXT("exactly one lantern materialized"), RestoredLanternCount, 1);

	Placement->EnterPlacementMode();
	TestFalse(TEXT("a spent point cannot be confirmed again"), Placement->ConfirmPlacement());
	int32 CountAfterRetry = 0;
	for (TActorIterator<AActor> It(Scope.World); It; ++It)
	{
		CountAfterRetry += It->ActorHasTag(TEXT("Gloamstead.RestoredLantern")) ? 1 : 0;
	}
	TestEqual(TEXT("retry does not spawn a second lantern"), CountAfterRetry, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamDisabledDawnAutosaveWritesNothingTest,
	"Gloamstead.FirstNight.PlayableSlice.DisabledDawnAutosaveWritesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamDisabledDawnAutosaveWritesNothingTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamFirstNightPlayableSlice;

	FScopedDefaultSaveSlot SaveGuard;
	FScopedWorld Scope;
	if (!TestNotNull(TEXT("live world created"), Scope.World))
	{
		return false;
	}

	UGloamsteadPCGSubsystem* PCG = Scope.World->GetSubsystem<UGloamsteadPCGSubsystem>();
	UGloamsteadDayNightSubsystem* DayNight = Scope.World->GetSubsystem<UGloamsteadDayNightSubsystem>();
	if (!TestNotNull(TEXT("PCG subsystem exists"), PCG)
		|| !TestNotNull(TEXT("day/night subsystem exists"), DayNight))
	{
		return false;
	}
	PCG->Test_SeedPoints({FVector::ZeroVector});
	PCG->Test_SeedPointStates({FRitualPointState()});

	UFunction* Setter = DayNight->FindFunction(TEXT("SetDawnAutosaveEnabled"));
	if (!TestNotNull(TEXT("day/night exposes the dawn-autosave policy seam"), Setter))
	{
		return false;
	}
	struct FSetDawnAutosaveEnabledParams
	{
		bool bEnabled = false;
	} Params;
	DayNight->ProcessEvent(Setter, &Params);
	DayNight->SetPhase(EGloamsteadDayPhase::Dawn);

	TestFalse(TEXT("disabled dawn autosave creates no sanctuary slot"),
		UGameplayStatics::DoesSaveGameExist(SaveGuard.Slot, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamDemoBootstrapDisablesPersistenceTest,
	"Gloamstead.FirstNight.PlayableSlice.DemoBootstrapDisablesPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamDemoBootstrapDisablesPersistenceTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamFirstNightPlayableSlice;

	FScopedWorld Scope;
	if (!TestNotNull(TEXT("live world created"), Scope.World))
	{
		return false;
	}
	UGloamsteadDayNightSubsystem* DayNight = Scope.World->GetSubsystem<UGloamsteadDayNightSubsystem>();
	if (!TestNotNull(TEXT("day/night subsystem exists"), DayNight))
	{
		return false;
	}
	TestTrue(TEXT("persistence is enabled by default"), DayNight->IsDawnAutosaveEnabled());

	FActorSpawnParameters SpawnParams;
	SpawnParams.bDeferConstruction = true;
	AGloamsteadSanctuaryBootstrap* Bootstrap = Scope.World->SpawnActor<AGloamsteadSanctuaryBootstrap>(
		AGloamsteadSanctuaryBootstrap::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("demo bootstrap spawned"), Bootstrap))
	{
		return false;
	}
	FBoolProperty* PersistenceProperty = FindFProperty<FBoolProperty>(
		AGloamsteadSanctuaryBootstrap::StaticClass(), TEXT("bEnablePersistence"));
	if (!TestNotNull(TEXT("bootstrap exposes the persistence policy"), PersistenceProperty))
	{
		return false;
	}
	PersistenceProperty->SetPropertyValue_InContainer(Bootstrap, false);
	Bootstrap->FinishSpawning(FTransform::Identity);
	Bootstrap->ApplyPersistencePolicy();

	TestFalse(TEXT("the demo bootstrap disables dawn autosave for its world"),
		DayNight->IsDawnAutosaveEnabled());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
