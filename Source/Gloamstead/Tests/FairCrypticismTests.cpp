// Fair-crypticism contract: the GardenRot warning is difficult by observation,
// not arbitrary by implementation. Every near miss below must remain closed.
#include "Misc/AutomationTest.h"

#include "Data/ExperienceCycleTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Actors/GloamsteadEvidenceSource.h"
#include "Actors/GloamsteadRestoredGardenBed.h"
#include "Components/RitualPlacementComponent.h"
#include "Components/GloamsteadSurveySubjectComponent.h"
#include "GloamsteadCharacter.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/NightStrategy.h"
#include "Systems/VeilHeart.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/Guid.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * The receipt route is intentionally a player-world route: the ritual
	 * placement authority emits its private native completion event to the one
	 * spawned Heart only after the point becomes restored. A NewObject'd Heart
	 * can appear configured without receiving that world-owned event, which
	 * would turn the negative fixtures below into a false proof and make the
	 * positive fixture impossible. Keep this small world scoped so every early
	 * test return tears it down before the next automation case runs.
	 */
	struct FGloamFairCrypticismScopedWorld
	{
		UWorld* World = nullptr;
		UGameInstance* GameInstance = nullptr;

		FGloamFairCrypticismScopedWorld()
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

		~FGloamFairCrypticismScopedWorld()
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

		FGloamFairCrypticismScopedWorld(const FGloamFairCrypticismScopedWorld&) = delete;
		FGloamFairCrypticismScopedWorld& operator=(const FGloamFairCrypticismScopedWorld&) = delete;
	};

	void EnsureGloamFairCrypticismActorBegunPlay(AActor* Actor)
	{
		if (IsValid(Actor) && !Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
	}

	FExperienceCyclePlan MakeGardenPlan()
	{
		UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>();
		PopulateDefaultExperienceCyclePlans(*Catalog);
		return Catalog->AuthoredPlans[1];
	}

	FVeilHeartWarningFragment MakeGardenWarning(const FExperienceCyclePlan& Plan)
	{
		FVeilHeartWarningFragment Warning;
		Warning.WarningId = Plan.WarningId;
		Warning.Fragment = FText::FromString(TEXT("What grows in darkness must be tended before the bell tolls."));
		Warning.AssociatedNightType = Plan.NightType;
		Warning.SatisfiableTags = Plan.RequiredRestorationTags;
		Warning.SemanticSubject = Plan.SemanticSubject;
		Warning.RequiredRitualType = Plan.RequiredRitualType;
		Warning.InterpretationReceiptId = Plan.InterpretationReceiptId;
		Warning.ClarityTier = 1;

		const TArray<FText> Evidence = {
			FText::FromString(TEXT("Grey leaves curl toward the eastern bed.")),
			FText::FromString(TEXT("A root-chime answers beside the cracked bed.")),
			FText::FromString(TEXT("Moths gather where soil whispers beneath the bell."))
		};
		for (int32 Index = 0; Index < Plan.RequiredSupportIds.Num(); ++Index)
		{
			FVeilHeartWarningSupportChannel& Channel = Warning.SupportChannels.AddDefaulted_GetRef();
			Channel.SupportId = Plan.RequiredSupportIds[Index];
			Channel.ChannelType = Plan.RequiredSupportChannelTypes[Index];
			Channel.EvidenceText = Evidence[Index];
		}
		return Warning;
	}

	FRestorationEventPayload MakeExactGardenRestoration(const FExperienceCyclePlan& Plan)
	{
		FRestorationEventPayload Payload;
		Payload.RitualType = Plan.RequiredRitualType;
		Payload.WarningId = Plan.WarningId;
		Payload.WarningTagSatisfied = Plan.RequiredRestorationTags[0];
		Payload.SemanticSubject = Plan.SemanticSubject;
		Payload.PointIndex = 0;
		return Payload;
	}

	struct FHeartFixture
	{
		TSharedPtr<FGloamFairCrypticismScopedWorld> LiveWorld;
		FExperienceCyclePlan Plan;
		TObjectPtr<UVeilHeartWarningCatalog> Catalog;
		TObjectPtr<AVeilHeart> Heart;
		TObjectPtr<AGloamsteadFirstNightDirector> Presenter;
		TObjectPtr<URitualPlacementComponent> Placement;
		TObjectPtr<UGloamsteadSurveySubjectComponent> PlacementEvidenceSubject;
		TObjectPtr<UGloamsteadPCGSubsystem> PCG;
		TObjectPtr<UGloamsteadDayNightSubsystem> DayNight;
		TObjectPtr<UGloamsteadExperienceCycleSubsystem> Experience;
		TObjectPtr<AGloamsteadCharacter> PlayerCharacter;
		bool bReady = false;

		bool SpawnPlayerCharacterAt(const FVector& Location)
		{
			if (!LiveWorld.IsValid() || !LiveWorld->World)
			{
				return false;
			}

			// AGloamsteadCharacter is deliberately abstract; the shipped Blueprint
			// child is the concrete player surface that owns the UI prompt and the
			// same placement component the real map uses.
			UClass* PlayerClass = LoadClass<AGloamsteadCharacter>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			if (!PlayerClass)
			{
				return false;
			}

			PlayerCharacter = LiveWorld->World->SpawnActor<AGloamsteadCharacter>(
				PlayerClass, FTransform(Location));
			EnsureGloamFairCrypticismActorBegunPlay(PlayerCharacter);
			return PlayerCharacter && PlayerCharacter->GetRitualPlacement();
		}

		bool ApplyRestorationAt(int32 PointIndex, const FRestorationEventPayload& Payload) const
		{
			return PCG && PCG->ApplyRestoration(PointIndex, Payload);
		}

		/**
		 * Automation-only access to the same private confirmation tail that
		 * ConfirmPlacement uses in a player world.  It is intentionally distinct
		 * from ApplyRestorationAt: a generic PCG mutation may restore a point but
		 * has no placement authority to notify the Heart.
		 */
		bool CommitPlacementAuthorizedRestorationAt(int32 PointIndex, FRestorationEventPayload Payload) const
		{
			if (!LiveWorld.IsValid() || !LiveWorld->World || !Placement || !PCG)
			{
				return false;
			}

			AActor* RestoredActor = LiveWorld->World->SpawnActor<AActor>();
			if (!RestoredActor)
			{
				return false;
			}
			Payload.PointIndex = PointIndex;
			Payload.RestoredActor = RestoredActor;
			return Placement->Test_CommitRestorationWithEvidence(
				PCG, PointIndex, Payload, FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
		}

		bool ReportSupportEncounter(FName WarningId, FName SupportId, FName ChannelType) const
		{
			if (!LiveWorld.IsValid() || !LiveWorld->World)
			{
				return false;
			}

			AGloamsteadEvidenceSource* Source = LiveWorld->World->SpawnActor<AGloamsteadEvidenceSource>();
			if (!Source)
			{
				return false;
			}

			Source->WarningId = WarningId;
			Source->SupportId = SupportId;
			Source->ChannelType = ChannelType;
			return Source->ReportEncounter(nullptr);
		}

		bool ResetForAnotherSupportScenario() const
		{
			if (!Heart || !PCG)
			{
				return false;
			}

			TArray<FRitualPointState> States;
			States.SetNum(5);
			PCG->Test_SeedPointStates(States);
			Heart->Test_ResetInterpretationPersistentState();
			return Heart->EmitWarningById(Plan.WarningId, Plan.NightType);
		}
	};

	FHeartFixture MakeHeartFixture(
		bool bAddSameTypeDecoy = false,
		bool bPlaceCanonicalGardenOutOfRange = false,
		bool bPlaceSameRitualDecoyCloserThanCanonical = false)
	{
		FHeartFixture Fixture;
		Fixture.LiveWorld = MakeShared<FGloamFairCrypticismScopedWorld>();
		if (!Fixture.LiveWorld->World || !Fixture.LiveWorld->GameInstance)
		{
			return Fixture;
		}

		Fixture.PCG = Fixture.LiveWorld->World->GetSubsystem<UGloamsteadPCGSubsystem>();
		Fixture.DayNight = Fixture.LiveWorld->World->GetSubsystem<UGloamsteadDayNightSubsystem>();
		Fixture.Experience = Fixture.LiveWorld->GameInstance->GetSubsystem<UGloamsteadExperienceCycleSubsystem>();
		if (!Fixture.PCG || !Fixture.DayNight || !Fixture.Experience)
		{
			return Fixture;
		}

		UExperienceCycleCatalog* CycleCatalog = NewObject<UExperienceCycleCatalog>(Fixture.LiveWorld->GameInstance);
		PopulateDefaultExperienceCyclePlans(*CycleCatalog);
		if (CycleCatalog->AuthoredPlans.Num() < 2)
		{
			return Fixture;
		}
		Fixture.Plan = CycleCatalog->AuthoredPlans[1];
		Fixture.Experience->Test_SetCatalog(CycleCatalog);

		FExperienceCyclePersistentState CycleTwoState;
		CycleTwoState.CompletedCycleSlot = 1;
		CycleTwoState.bFirstRestCompleted = true;
		CycleTwoState.ArmedPlanId = Fixture.Plan.PlanId;
		if (!Fixture.Experience->RestorePersistentState(CycleTwoState))
		{
			return Fixture;
		}

		Fixture.Catalog = NewObject<UVeilHeartWarningCatalog>(Fixture.LiveWorld->GameInstance);
		Fixture.Catalog->Warnings.Add(MakeGardenWarning(Fixture.Plan));
		if (bAddSameTypeDecoy)
		{
			FVeilHeartWarningFragment Decoy = MakeGardenWarning(Fixture.Plan);
			Decoy.WarningId = TEXT("GardenRotDecoy");
			Fixture.Catalog->Warnings.Add(Decoy);
		}

		Fixture.PCG->Test_SeedPoints({
			bPlaceCanonicalGardenOutOfRange
				? FVector(2400.f, 0.f, 0.f)
				: (bPlaceSameRitualDecoyCloserThanCanonical ? FVector(200.f, 0.f, 0.f) : FVector::ZeroVector),
			bPlaceSameRitualDecoyCloserThanCanonical ? FVector::ZeroVector : FVector(100.f, 0.f, 0.f),
			FVector(200.f, 0.f, 0.f),
			FVector(300.f, 0.f, 0.f),
			FVector(400.f, 0.f, 0.f)
		});
		TArray<FRitualPointState> States;
		States.SetNum(5);
		Fixture.PCG->Test_SeedPointStates(States);
		Fixture.PCG->Test_SetPointContractMetadata(0, Fixture.Plan.WarningId, Fixture.Plan.SemanticSubject,
			Fixture.Plan.RequiredRitualType, Fixture.Plan.RequiredRestorationTags[0]);
		Fixture.PCG->Test_SetPointContractMetadata(1, TEXT("GardenRotDecoy"), Fixture.Plan.SemanticSubject,
			Fixture.Plan.RequiredRitualType, Fixture.Plan.RequiredRestorationTags[0]);
		Fixture.PCG->Test_SetPointContractMetadata(2, Fixture.Plan.WarningId, TEXT("Cycle2_Elsewhere"),
			Fixture.Plan.RequiredRitualType, Fixture.Plan.RequiredRestorationTags[0]);
		Fixture.PCG->Test_SetPointContractMetadata(3, Fixture.Plan.WarningId, Fixture.Plan.SemanticSubject,
			Fixture.Plan.RequiredRitualType, TEXT("Growth"));
		Fixture.PCG->Test_SetPointContractMetadata(4, Fixture.Plan.WarningId, Fixture.Plan.SemanticSubject,
			ERitualType::LanternPost, Fixture.Plan.RequiredRestorationTags[0]);

		Fixture.Heart = Fixture.LiveWorld->World->SpawnActorDeferred<AVeilHeart>(AVeilHeart::StaticClass(), FTransform::Identity);
		if (!Fixture.Heart)
		{
			return Fixture;
		}
		Fixture.Heart->WarningCatalog = Fixture.Catalog;
		Fixture.Heart->FinishSpawning(FTransform::Identity);
		EnsureGloamFairCrypticismActorBegunPlay(Fixture.Heart);

		// The component is owned by a real actor in the same player world so its
		// evidence tail has the same world/registry context as ConfirmPlacement.
		AActor* PlacementOwner = Fixture.LiveWorld->World->SpawnActor<AActor>();
		Fixture.Placement = PlacementOwner ? NewObject<URitualPlacementComponent>(PlacementOwner) : nullptr;
		Fixture.PlacementEvidenceSubject = PlacementOwner
			? NewObject<UGloamsteadSurveySubjectComponent>(PlacementOwner)
			: nullptr;
		if (!Fixture.Placement || !Fixture.PlacementEvidenceSubject)
		{
			return Fixture;
		}
		Fixture.Placement->RegisterComponent();
		Fixture.PlacementEvidenceSubject->SubjectId = TEXT("courtyard.lantern.first");
		if (!Fixture.PlacementEvidenceSubject->RegisterWithRegistry())
		{
			return Fixture;
		}

		Fixture.Presenter = Fixture.LiveWorld->World->SpawnActorDeferred<AGloamsteadFirstNightDirector>(
			AGloamsteadFirstNightDirector::StaticClass(), FTransform::Identity);
		if (!Fixture.Presenter)
		{
			return Fixture;
		}
		Fixture.Presenter->FirstNightType = Fixture.Plan.NightType;
		Fixture.Presenter->FinishSpawning(FTransform::Identity);
		Fixture.Heart->OnWarningEmittedDelegate.AddDynamic(Fixture.Presenter, &AGloamsteadFirstNightDirector::HandleHeartWarning);
		Fixture.bReady = Fixture.Heart->RegisterWarningPresenter(
			Fixture.Presenter, GET_FUNCTION_NAME_CHECKED(AGloamsteadFirstNightDirector, HandleHeartWarning))
			&& Fixture.DayNight->PrepareUpcomingCycle()
			&& Fixture.DayNight->GetUpcomingPlan()
			&& Fixture.DayNight->GetUpcomingPlan()->PlanId == Fixture.Plan.PlanId;
		return Fixture;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismRejectsSameRitualDecoyBeforeMutationTest,
	"Gloamstead.FairCrypticism.Targeting.RejectsSameRitualDecoyBeforeMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismRejectsSameRitualDecoyBeforeMutationTest::RunTest(const FString& /*Parameters*/)
{
	// The only nearby GardenBed is deliberately a different warning. The
	// canonical GardenRot point exists but is beyond placement search range.
	// This must exercise the actual character -> placement route, not the
	// automation-only confirmation tail, because the player must never visibly
	// consume a same-ritual decoy before the Heart rejects it after the fact.
	FHeartFixture Fixture = MakeHeartFixture(/*bAddSameTypeDecoy*/ true, /*bPlaceCanonicalGardenOutOfRange*/ true);
	if (!TestTrue(TEXT("fixture presents the exact GardenRot warning"), Fixture.bReady)
		|| !TestTrue(TEXT("a live player character owns the actual placement component"),
			Fixture.SpawnPlayerCharacterAt(FVector::ZeroVector)))
	{
		return false;
	}

	TestTrue(TEXT("first exact support makes a receipt possible if the target were correct"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0], Fixture.Plan.RequiredSupportChannelTypes[0]));
	TestTrue(TEXT("second exact support makes a receipt possible if the target were correct"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[1], Fixture.Plan.RequiredSupportChannelTypes[1]));

	URitualPlacementComponent* PlayerPlacement = Fixture.PlayerCharacter->GetRitualPlacement();
	Fixture.PlayerCharacter->GloamRestore();
	TestTrue(TEXT("the player arms the authored GardenBed ritual"), PlayerPlacement->IsInPlacementMode());
	TestEqual(TEXT("the player remains in the authored GardenBed mode"),
		PlayerPlacement->GetPlacementRitualType(), ERitualType::GardenBed);
	TestFalse(TEXT("a nearer same-ritual decoy is not a valid confirmation target"), PlayerPlacement->IsCurrentPlacementValid());
	TestTrue(TEXT("the actual player prompt explains that the decoy does not answer the Heart warning"),
		Fixture.PlayerCharacter->GetPlayerPromptText().ToString().Contains(TEXT("does not answer the Heart's warning")));
	TestFalse(TEXT("confirmation refuses the nearby wrong GardenBed before mutation"), PlayerPlacement->ConfirmPlacement());
	TestFalse(TEXT("the canonical out-of-range GardenRot point remains unrestored"), Fixture.PCG->IsPointRestored(0));
	TestFalse(TEXT("the nearer wrong-warning GardenBed decoy remains unrestored"), Fixture.PCG->IsPointRestored(1));

	AGloamsteadRestoredGardenBed* RestoredGarden = nullptr;
	for (TActorIterator<AGloamsteadRestoredGardenBed> It(Fixture.LiveWorld->World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Gloamstead.RestoredGarden")))
		{
			RestoredGarden = *It;
			break;
		}
	}
	TestNull(TEXT("a rejected decoy never spawns a visible restored garden actor"), RestoredGarden);
	TestFalse(TEXT("a rejected decoy cannot mint a GardenRot interpretation receipt"),
		Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismRefusesChangedOrWithdrawnPlanWhileOpenTest,
	"Gloamstead.FairCrypticism.Targeting.RefusesChangedOrWithdrawnPlanWhileOpen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismRefusesChangedOrWithdrawnPlanWhileOpenTest::RunTest(const FString& /*Parameters*/)
{
	FHeartFixture Fixture = MakeHeartFixture();
	if (!TestTrue(TEXT("fixture presents GardenRot before the plan-change guard runs"), Fixture.bReady))
	{
		return false;
	}

	Fixture.Placement->EnterPlacementMode();
	TestTrue(TEXT("the original exact GardenRot plan starts with a valid target"), Fixture.Placement->IsCurrentPlacementValid());

	FExperienceCyclePersistentState ChangedPlanState;
	ChangedPlanState.ArmedPlanId = TEXT("Cycle1_Tutorial");
	TestTrue(TEXT("the live experience authority can move to a different authored plan"),
		Fixture.Experience->RestorePersistentState(ChangedPlanState));
	Fixture.Placement->ForceUpdatePreview();
	TestTrue(TEXT("placement remains open long enough to explain a changed plan"), Fixture.Placement->IsInPlacementMode());
	TestFalse(TEXT("a changed plan invalidates the old GardenRot target"), Fixture.Placement->IsCurrentPlacementValid());
	TestTrue(TEXT("the changed-plan status is readable through the placement UI seam"),
		Fixture.Placement->GetPlacementStatusText().ToString().Contains(TEXT("warning has shifted")));
	TestFalse(TEXT("a changed plan refuses confirmation before mutation"), Fixture.Placement->ConfirmPlacement());
	TestFalse(TEXT("a changed plan leaves the original GardenRot point unrestored"), Fixture.PCG->IsPointRestored(0));

	FExperienceCyclePersistentState WithdrawnPlanState;
	WithdrawnPlanState.CompletedCycleSlot = 1;
	WithdrawnPlanState.bFirstRestCompleted = true;
	TestTrue(TEXT("the live experience authority can withdraw the active plan"),
		Fixture.Experience->RestorePersistentState(WithdrawnPlanState));
	Fixture.Placement->ForceUpdatePreview();
	TestFalse(TEXT("a withdrawn plan keeps the ritual invalid"), Fixture.Placement->IsCurrentPlacementValid());
	TestTrue(TEXT("the withdrawn-plan status is readable through the placement UI seam"),
		Fixture.Placement->GetPlacementStatusText().ToString().Contains(TEXT("warning has faded")));
	TestFalse(TEXT("a withdrawn plan refuses confirmation before mutation"), Fixture.Placement->ConfirmPlacement());
	TestFalse(TEXT("a withdrawn plan leaves the original GardenRot point unrestored"), Fixture.PCG->IsPointRestored(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismRequiresDistinctKnownSupportsTest,
	"Gloamstead.FairCrypticism.Supports.RequireDistinctKnownEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismRequiresDistinctKnownSupportsTest::RunTest(const FString& /*Parameters*/)
{
	// WorldForge's state subsystem owns a process-global console command. This
	// contract needs isolated interpretation state, not multiple scoped test
	// worlds, so reset the one real player-world fixture between scenarios.
	FHeartFixture Fixture = MakeHeartFixture();
	if (!TestTrue(TEXT("fixture presents the exact GardenRot warning"), Fixture.bReady))
	{
		return false;
	}
	TestTrue(TEXT("one known support is recorded"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0], Fixture.Plan.RequiredSupportChannelTypes[0]));
	TestFalse(TEXT("one support cannot produce an interpretation receipt"),
		Fixture.ApplyRestorationAt(0, MakeExactGardenRestoration(Fixture.Plan))
			&& Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));
	TestFalse(TEXT("a duplicate support is not counted twice"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0], Fixture.Plan.RequiredSupportChannelTypes[0]));
	TestFalse(TEXT("a duplicate still cannot produce an interpretation receipt"),
		Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));

	if (!TestTrue(TEXT("wrong-medium scenario re-presents GardenRot in the same player world"),
		Fixture.ResetForAnotherSupportScenario()))
	{
		return false;
	}
	TestFalse(TEXT("a known support with the wrong authored medium is rejected"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0], TEXT("Audio")));
	TestFalse(TEXT("wrong-medium evidence cannot earn an interpretation receipt"),
		Fixture.ApplyRestorationAt(0, MakeExactGardenRestoration(Fixture.Plan))
			&& Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));

	if (!TestTrue(TEXT("unknown-support scenario re-presents GardenRot in the same player world"),
		Fixture.ResetForAnotherSupportScenario()))
	{
		return false;
	}
	TestFalse(TEXT("an unknown support id is not accepted"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, TEXT("GardenRot.InventedClue"), TEXT("Environmental")));
	TestTrue(TEXT("one valid support still records after an unknown id"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0], Fixture.Plan.RequiredSupportChannelTypes[0]));
	TestFalse(TEXT("unknown plus one known support remains insufficient"),
		Fixture.ApplyRestorationAt(0, MakeExactGardenRestoration(Fixture.Plan))
			&& Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));

	AGloamsteadEvidenceSource* ForeignSource = NewObject<AGloamsteadEvidenceSource>();
	ForeignSource->WarningId = Fixture.Plan.WarningId;
	ForeignSource->SupportId = Fixture.Plan.RequiredSupportIds[1];
	ForeignSource->ChannelType = Fixture.Plan.RequiredSupportChannelTypes[1];
	TestFalse(TEXT("a non-world or foreign evidence actor cannot report support to the Heart"),
		Fixture.Heart->RecordSupportEncounterFromEvidenceSource(ForeignSource));
	TestNull(TEXT("the raw support evaluator is not Blueprint-reflected"),
		Fixture.Heart->FindFunction(TEXT("RecordSupportEncounter")));
	TestNull(TEXT("the raw restoration receipt evaluator is not Blueprint-reflected"),
		Fixture.Heart->FindFunction(TEXT("EvaluateRestorationAgainstActivePlan")));
	TestNull(TEXT("production Heart persistence restore is not Blueprint-reflected"),
		Fixture.Heart->FindFunction(TEXT("RestoreInterpretationPersistentState")));
	TestNull(TEXT("automation-only Heart persistence restore is never Blueprint-reflected"),
		Fixture.Heart->FindFunction(TEXT("Test_RestoreInterpretationPersistentState")));
	TestNull(TEXT("PCG initialization accepts no Blueprint metadata injection route"),
		Fixture.PCG->FindFunction(TEXT("InitializeFromPCGComponent")));
	TestNull(TEXT("automation-only PCG initialization seam is never Blueprint-reflected"),
		Fixture.PCG->FindFunction(TEXT("Test_InitializeFromPCGComponent")));
	TestNull(TEXT("automation-only PCG semantic metadata writer is never Blueprint-reflected"),
		Fixture.PCG->FindFunction(TEXT("Test_SetPointContractMetadata")));
	TestNull(TEXT("automation-only placement confirmation seam is never Blueprint-reflected"),
		Fixture.Placement->FindFunction(TEXT("Test_CommitRestorationWithEvidence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismExactWarningAndRestorationTest,
	"Gloamstead.FairCrypticism.Interpretation.RequiresExactWarningAndGarden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismExactWarningAndRestorationTest::RunTest(const FString& /*Parameters*/)
{
	FHeartFixture Fixture = MakeHeartFixture(
		/*bAddSameTypeDecoy*/ true,
		/*bPlaceCanonicalGardenOutOfRange*/ false,
		/*bPlaceSameRitualDecoyCloserThanCanonical*/ true);
	if (!TestTrue(TEXT("fixture presents the exact GardenRot warning"), Fixture.bReady))
	{
		return false;
	}

	TestTrue(TEXT("a same-type decoy warning can emit through the exact emitter"),
		Fixture.Heart->EmitWarningById(TEXT("GardenRotDecoy"), Fixture.Plan.NightType));
	TestFalse(TEXT("a same-type/clarity decoy cannot substitute for GardenRot support"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0], Fixture.Plan.RequiredSupportChannelTypes[0]));
	TestTrue(TEXT("the canonical GardenRot warning can be re-presented exactly"),
		Fixture.Heart->EmitWarningById(Fixture.Plan.WarningId, Fixture.Plan.NightType));
	TestTrue(TEXT("first distinct GardenRot support is recorded"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0], Fixture.Plan.RequiredSupportChannelTypes[0]));
	TestTrue(TEXT("second distinct GardenRot support is recorded"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[1], Fixture.Plan.RequiredSupportChannelTypes[1]));
	TestTrue(TEXT("the fixture uses a spawned Heart and world-owned PCG authority"),
		Fixture.Heart && Fixture.Heart->GetWorld() == Fixture.LiveWorld->World && Fixture.PCG == Fixture.LiveWorld->World->GetSubsystem<UGloamsteadPCGSubsystem>());
	TestTrue(TEXT("the seeded Garden point carries the complete active-plan contract before restoration"),
		Fixture.PCG && Fixture.PCG->PointMatchesExperiencePlan(0, Fixture.Plan));

	// All literals below falsely claim the active plan. The Heart must ignore
	// them and read the *restored PCG point's* warning/subject/ritual/tag.
	FRestorationEventPayload ForgedPayload = MakeExactGardenRestoration(Fixture.Plan);
	ForgedPayload.PointIndex = 1;
	TestTrue(TEXT("a foreign point can be restored for the forged-payload fixture"),
		Fixture.ApplyRestorationAt(1, ForgedPayload));
	TestFalse(TEXT("a forged warning literal on a foreign PCG point cannot earn the receipt"),
		Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));

	ForgedPayload.PointIndex = 2;
	TestTrue(TEXT("a wrong-subject point can be restored for the authority fixture"),
		Fixture.ApplyRestorationAt(2, ForgedPayload));
	TestFalse(TEXT("a forged subject literal cannot substitute for foreign PCG metadata"),
		Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));

	ForgedPayload.PointIndex = 3;
	TestTrue(TEXT("a wrong-tag point can be restored for the authority fixture"),
		Fixture.ApplyRestorationAt(3, ForgedPayload));
	TestFalse(TEXT("a forged GardenBed tag cannot substitute for foreign PCG metadata"),
		Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));

	ForgedPayload.PointIndex = 4;
	TestTrue(TEXT("a wrong-ritual point can be restored for the authority fixture"),
		Fixture.ApplyRestorationAt(4, ForgedPayload));
	TestFalse(TEXT("a forged ritual literal cannot substitute for foreign PCG metadata"),
		Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));

	const FRestorationEventPayload ExactRestoration = MakeExactGardenRestoration(Fixture.Plan);
	TestTrue(TEXT("a generic matching point plus coherent literal payload can still restore ordinary PCG state"),
		Fixture.ApplyRestorationAt(0, ExactRestoration));
	TestFalse(TEXT("a generic matching point plus coherent literal payload cannot mint a Cycle II interpretation receipt"),
		Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));

	if (!TestTrue(TEXT("fresh player-world fixture can reset after the hostile generic-PCG route"),
		Fixture.ResetForAnotherSupportScenario()))
	{
		return false;
	}
	TestTrue(TEXT("first exact support is recorded for the placement-authorized route"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0], Fixture.Plan.RequiredSupportChannelTypes[0]));
	TestTrue(TEXT("second exact support is recorded for the placement-authorized route"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[1], Fixture.Plan.RequiredSupportChannelTypes[1]));

	// This is the player-facing receipt route: a live GameInstance owns the
	// Cycle II plan; the component asks DayNight to arm it, targets the authored
	// GardenBed, then performs the same Enter -> target -> Confirm sequence the
	// character input invokes. It must not use the automation confirmation tail.
	Fixture.Placement->EnterPlacementMode();
	TestTrue(TEXT("the real placement route enters with the live authored plan"), Fixture.Placement->IsInPlacementMode());
	TestEqual(TEXT("the active Cycle II plan selects GardenBed rather than a Lantern fallback"),
		Fixture.Placement->GetPlacementRitualType(), ERitualType::GardenBed);
	TestEqual(TEXT("the real target carries GardenBed semantics"),
		Fixture.Placement->GetCurrentTargetRitualType(), ERitualType::GardenBed);
	TestEqual(TEXT("the real route ignores the closer same-ritual decoy and targets the exact GardenRot point"),
		Fixture.Placement->GetCurrentTargetPointInfo().PointIndex, 0);
	TestTrue(TEXT("the authored GardenBed target is valid for the player-world confirmation"),
		Fixture.Placement->IsCurrentPlacementValid());
	TestTrue(TEXT("the real confirmation restores the exact GardenBed point"), Fixture.Placement->ConfirmPlacement());
	TestTrue(TEXT("the real confirmation restores the authoritative GardenBed point"), Fixture.PCG->IsPointRestored(0));
	TestFalse(TEXT("GardenBed confirmation is not the degraded no-actor route"), Fixture.Placement->WasLastRestoredActorMissing());

	AGloamsteadRestoredGardenBed* RestoredGarden = nullptr;
	for (TActorIterator<AGloamsteadRestoredGardenBed> It(Fixture.LiveWorld->World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Gloamstead.RestoredGarden")))
		{
			RestoredGarden = *It;
			break;
		}
	}
	TestNotNull(TEXT("GardenBed confirmation materializes a visible project-owned garden actor"), RestoredGarden);
	if (RestoredGarden)
	{
		TestTrue(TEXT("the materialized garden actor has a visible mesh"), RestoredGarden->HasVisibleGardenMesh());
	}

	TestTrue(TEXT("two exact supports plus a real GardenBed confirmation earn a receipt"),
		Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));
	const FExperienceInterpretationReceipt Receipt = Fixture.Heart->GetLastInterpretationReceipt();
	TestTrue(TEXT("the exact receipt is concrete"), Receipt.IsValid());
	TestEqual(TEXT("the receipt retains the canonical id"), Receipt.ReceiptId, Fixture.Plan.InterpretationReceiptId);
	TestEqual(TEXT("the receipt retains the GardenRot warning"), Receipt.WarningId, Fixture.Plan.WarningId);
	TestEqual(TEXT("the receipt retains the stable garden subject"), Receipt.SemanticSubject, Fixture.Plan.SemanticSubject);
	TestEqual(TEXT("the receipt retains GardenBed ritual semantics"), Receipt.RestorationRitualType, ERitualType::GardenBed);
	TestTrue(TEXT("the Heart recognizes its exact active-plan receipt"), Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismV3ReceiptRestoreRequiresExactEncounterSetTest,
	"Gloamstead.FairCrypticism.Persistence.V3ReceiptRequiresExactEncounterSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismV3ReceiptRestoreRequiresExactEncounterSetTest::RunTest(const FString& /*Parameters*/)
{
	FHeartFixture Fixture = MakeHeartFixture();
	if (!TestTrue(TEXT("fixture presents the exact GardenRot warning for persisted-state validation"), Fixture.bReady))
	{
		return false;
	}

	// The crafted v3 state is otherwise an exact GardenRot proof: the plan,
	// warning, ritual, tag, and restored PCG target all agree. Only the receipt
	// claims a second support the persisted encounter set never recorded.
	TArray<FRitualPointState> States;
	States.SetNum(5);
	States[0].bIsRestored = true;
	Fixture.PCG->Test_SeedPointStates(States);

	FVeilHeartInterpretationPersistentState CraftedState;
	CraftedState.PresentedPlanId = Fixture.Plan.PlanId;
	CraftedState.PresentedWarningId = Fixture.Plan.WarningId;
	CraftedState.EncounteredSupportIds = { Fixture.Plan.RequiredSupportIds[0] };
	FExperienceInterpretationReceipt& CraftedReceipt = CraftedState.InterpretationReceipt;
	CraftedReceipt.ReceiptId = Fixture.Plan.InterpretationReceiptId;
	CraftedReceipt.PlanId = Fixture.Plan.PlanId;
	CraftedReceipt.WarningId = Fixture.Plan.WarningId;
	CraftedReceipt.SemanticSubject = Fixture.Plan.SemanticSubject;
	CraftedReceipt.RestorationTag = Fixture.Plan.RequiredRestorationTags[0];
	CraftedReceipt.RestorationRitualType = Fixture.Plan.RequiredRitualType;
	CraftedReceipt.RestorationPointIndex = 0;
	CraftedReceipt.SupportIds = { Fixture.Plan.RequiredSupportIds[0], Fixture.Plan.RequiredSupportIds[1] };

	TestFalse(TEXT("a v3 receipt whose support set is a forged superset is rejected atomically"),
		Fixture.Heart->Test_RestoreInterpretationPersistentState(CraftedState));
	TestEqual(TEXT("failed v3 restore leaves no presented warning resident"),
		Fixture.Heart->GetLastEmittedWarningId(), NAME_None);
	TestFalse(TEXT("failed v3 restore leaves no interpretation receipt resident"),
		Fixture.Heart->GetLastInterpretationReceipt().HasAnyFacts());
	TestFalse(TEXT("failed v3 restore leaves no encountered evidence resident"),
		Fixture.Heart->Test_CaptureInterpretationPersistentState().HasAnyFacts());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismV3ReceiptCaptureRetainsExpandedEvidenceTest,
	"Gloamstead.FairCrypticism.Persistence.V3ReceiptCaptureRetainsExpandedEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismV3ReceiptCaptureRetainsExpandedEvidenceTest::RunTest(const FString& /*Parameters*/)
{
	FHeartFixture Fixture = MakeHeartFixture();
	if (!TestTrue(TEXT("fixture presents GardenRot for expanded-evidence persistence validation"), Fixture.bReady)
		|| !TestTrue(TEXT("GardenRot supplies a third authored support"), Fixture.Plan.RequiredSupportIds.Num() >= 3))
	{
		return false;
	}

	TestTrue(TEXT("first support earns part of the exact interpretation proof"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0], Fixture.Plan.RequiredSupportChannelTypes[0]));
	TestTrue(TEXT("second support earns the minimum exact interpretation proof"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[1], Fixture.Plan.RequiredSupportChannelTypes[1]));
	TestTrue(TEXT("placement authority earns the initial receipt"),
		Fixture.CommitPlacementAuthorizedRestorationAt(0, MakeExactGardenRestoration(Fixture.Plan)));
	TestEqual(TEXT("initial receipt records the two encountered supports"),
		Fixture.Heart->GetLastInterpretationReceipt().SupportIds.Num(), 2);

	TestTrue(TEXT("a later third authored support is accepted"),
		Fixture.ReportSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[2], Fixture.Plan.RequiredSupportChannelTypes[2]));
	TestTrue(TEXT("the expanded exact support set preserves the live receipt"),
		Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));

	const FVeilHeartInterpretationPersistentState Captured = Fixture.Heart->Test_CaptureInterpretationPersistentState();
	TestEqual(TEXT("capture preserves all three encountered supports"), Captured.EncounteredSupportIds.Num(), 3);
	TestEqual(TEXT("capture refreshes the receipt to the exact three-support set"), Captured.InterpretationReceipt.SupportIds.Num(), 3);
	Fixture.Heart->Test_ResetInterpretationPersistentState();
	TestTrue(TEXT("a valid expanded v3 receipt restores"), Fixture.Heart->Test_RestoreInterpretationPersistentState(Captured));
	TestTrue(TEXT("the restored expanded receipt remains exact"), Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismDataContractRejectsNegativeFixturesTest,
	"Gloamstead.FairCrypticism.Data.RejectsSparseDuplicateUnknownAndMismatchedWarnings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismDataContractRejectsNegativeFixturesTest::RunTest(const FString& /*Parameters*/)
{
	const FExperienceCyclePlan Plan = MakeGardenPlan();
	FString Error;
	const FVeilHeartWarningFragment ValidWarning = MakeGardenWarning(Plan);
	TestTrue(TEXT("canonical GardenRot warning matches its plan"), ValidWarning.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment Sparse = ValidWarning;
	Sparse.SupportChannels.SetNum(1);
	TestFalse(TEXT("sparse support arrays are rejected"), Sparse.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment Duplicate = ValidWarning;
	Duplicate.SupportChannels[1].SupportId = Duplicate.SupportChannels[0].SupportId;
	TestFalse(TEXT("duplicate support identifiers are rejected"), Duplicate.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment Unknown = ValidWarning;
	Unknown.SupportChannels[2].SupportId = TEXT("GardenRot.InventedClue");
	TestFalse(TEXT("unknown support identifiers are rejected"), Unknown.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment WrongSubject = ValidWarning;
	WrongSubject.SemanticSubject = TEXT("Cycle2_Elsewhere");
	TestFalse(TEXT("plan-warning subject mismatches are rejected"), WrongSubject.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment WrongTag = ValidWarning;
	WrongTag.SatisfiableTags = { TEXT("Growth") };
	TestFalse(TEXT("plan-warning tag mismatches are rejected"), WrongTag.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment WrongMedium = ValidWarning;
	WrongMedium.SupportChannels[2].ChannelType = TEXT("Environmental");
	TestFalse(TEXT("plan-warning wrong-medium support mismatches are rejected"), WrongMedium.MatchesExactPlanContract(Plan, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismMissingSubjectNeverFallsBackTest,
	"Gloamstead.FairCrypticism.Runtime.MissingSubjectNeverFallsBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismMissingSubjectNeverFallsBackTest::RunTest(const FString& /*Parameters*/)
{
	AddExpectedErrorPlain(TEXT("NightRuntime: authored subject Cycle2_Garden has no PCG mapping"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("NightRuntime: no PCG point satisfies the full authored target contract for Cycle2_Garden"), EAutomationExpectedErrorFlags::Contains, 1);

	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPoints({ FVector::ZeroVector, FVector(100.f, 0.f, 0.f) });
	TArray<FRitualPointState> States;
	States.SetNum(2);
	States[0].CorruptionLevel = 0.10f;
	States[1].CorruptionLevel = 0.95f;
	PCG->Test_SeedPointStates(States);

	UNightConsequenceRuntime* Runtime = NewObject<UNightConsequenceRuntime>();
	const int32 GenericMostCorrupted = PCG->FindMostCorruptedPointIndex(/*bOnlyUnrestored*/ true);
	TestEqual(TEXT("fixture exposes a tempting generic bloom"), GenericMostCorrupted, 1);
	const int32 ResolvedSubject = Runtime->ResolveSemanticSubjectToPoint(TEXT("Cycle2_Garden"), PCG);
	TestEqual(TEXT("missing Cycle2_Garden metadata resolves no target"), ResolvedSubject, INDEX_NONE);
	TestNotEqual(TEXT("missing semantic mapping never falls back to the generic bloom"), ResolvedSubject, GenericMostCorrupted);

	FNightRuntimeContext ExactTargetContext;
	ExactTargetContext.NightType = ENightConsequenceType::Corruption;
	ExactTargetContext.bRequiresExactSemanticTarget = true;
	ExactTargetContext.RequiredSemanticSubject = TEXT("Cycle2_Garden");
	ExactTargetContext.TargetPointIndex = ResolvedSubject;
	UNightCorruptionStrategy* Strategy = NewObject<UNightCorruptionStrategy>();
	Strategy->EnterNight(ExactTargetContext, PCG);
	TestEqual(TEXT("the Corruption strategy keeps a missing exact subject untargeted"),
		Strategy->GetObjective().TargetPointIndex, INDEX_NONE);
	TestEqual(TEXT("the Corruption strategy turns a missing exact subject into a quiet objective"),
		Strategy->GetObjective().Kind, ENightObjectiveKind::None);

	const FExperienceCyclePlan GardenPlan = MakeGardenPlan();
	TestTrue(TEXT("test can author one full GardenRot PCG target"),
		PCG->Test_SetPointContractMetadata(0, GardenPlan.WarningId, GardenPlan.SemanticSubject,
			GardenPlan.RequiredRitualType, GardenPlan.RequiredRestorationTags[0]));
	TestTrue(TEXT("test can author a same-subject wrong-tag decoy"),
		PCG->Test_SetPointContractMetadata(1, GardenPlan.WarningId, GardenPlan.SemanticSubject,
			GardenPlan.RequiredRitualType, TEXT("Growth")));
	const int32 FullContractTarget = Runtime->ResolvePlanTargetToPoint(GardenPlan, PCG);
	TestEqual(TEXT("the full contract resolves the real garden rather than the most-corrupted decoy"), FullContractTarget, 0);
	TestNotEqual(TEXT("the full contract target is not the tempting generic bloom"), FullContractTarget, GenericMostCorrupted);

	TestTrue(TEXT("test can corrupt the final required PCG tag"),
		PCG->Test_SetPointContractMetadata(0, GardenPlan.WarningId, GardenPlan.SemanticSubject,
			GardenPlan.RequiredRitualType, TEXT("Growth")));
	const int32 MismatchedFullTarget = Runtime->ResolvePlanTargetToPoint(GardenPlan, PCG);
	TestEqual(TEXT("a target with only semantic-subject agreement stays untargeted"), MismatchedFullTarget, INDEX_NONE);
	TestNotEqual(TEXT("a mismatched full contract never falls back to a generic bloom"), MismatchedFullTarget, GenericMostCorrupted);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
