// Cycle II persistence invariants: legacy saves must never invent authored progression.
#include "Misc/AutomationTest.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Save/GloamsteadSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    bool AssertPointStatesEqual(FAutomationTestBase& Test, const TArray<FRitualPointState>& Actual, const TArray<FRitualPointState>& Expected)
    {
        if (!Test.TestEqual(TEXT("PCG point-state count is preserved"), Actual.Num(), Expected.Num()))
        {
            return false;
        }

        for (int32 Index = 0; Index < Expected.Num(); ++Index)
        {
            Test.TestEqual(FString::Printf(TEXT("PCG light[%d] is preserved"), Index), Actual[Index].LightLevel, Expected[Index].LightLevel, KINDA_SMALL_NUMBER);
            Test.TestEqual(FString::Printf(TEXT("PCG corruption[%d] is preserved"), Index), Actual[Index].CorruptionLevel, Expected[Index].CorruptionLevel, KINDA_SMALL_NUMBER);
            Test.TestEqual(FString::Printf(TEXT("PCG restored[%d] is preserved"), Index), Actual[Index].bIsRestored, Expected[Index].bIsRestored);
        }

        return true;
    }

    bool AssertExperienceCycleStateEqual(
        FAutomationTestBase& Test,
        const FExperienceCyclePersistentState& Actual,
        const FExperienceCyclePersistentState& Expected,
        const TCHAR* Context)
    {
        Test.TestEqual(FString::Printf(TEXT("%s completed cycle slot is preserved"), Context), Actual.CompletedCycleSlot, Expected.CompletedCycleSlot);
        Test.TestEqual(FString::Printf(TEXT("%s armed plan id is preserved"), Context), Actual.ArmedPlanId, Expected.ArmedPlanId);
        Test.TestEqual(FString::Printf(TEXT("%s last plan id is preserved"), Context), Actual.LastPlanId, Expected.LastPlanId);
        Test.TestEqual(FString::Printf(TEXT("%s last outcome tag is preserved"), Context), Actual.LastOutcomeResultTag, Expected.LastOutcomeResultTag);
        Test.TestEqual(FString::Printf(TEXT("%s scar count is preserved"), Context), Actual.ScarTags.Num(), Expected.ScarTags.Num());
        for (int32 Index = 0; Index < Expected.ScarTags.Num(); ++Index)
        {
            Test.TestEqual(FString::Printf(TEXT("%s scar[%d] is preserved"), Context, Index), Actual.ScarTags[Index], Expected.ScarTags[Index]);
        }
        Test.TestEqual(FString::Printf(TEXT("%s first-rest state is preserved"), Context), Actual.bFirstRestCompleted, Expected.bFirstRestCompleted);
        Test.TestEqual(FString::Printf(TEXT("%s saved phase ordinal is preserved"), Context), Actual.SavedPhaseOrdinal, Expected.SavedPhaseOrdinal);
        Test.TestEqual(FString::Printf(TEXT("%s legacy reconciliation state is preserved"), Context), Actual.bRequiresLegacyReconciliation, Expected.bRequiresLegacyReconciliation);
		Test.TestEqual(FString::Printf(TEXT("%s presented warning is preserved"), Context), Actual.HeartInterpretationState.PresentedWarningId, Expected.HeartInterpretationState.PresentedWarningId);
		Test.TestEqual(FString::Printf(TEXT("%s encountered support count is preserved"), Context), Actual.HeartInterpretationState.EncounteredSupportIds.Num(), Expected.HeartInterpretationState.EncounteredSupportIds.Num());
		for (int32 Index = 0; Index < Expected.HeartInterpretationState.EncounteredSupportIds.Num(); ++Index)
		{
			Test.TestEqual(FString::Printf(TEXT("%s encountered support[%d] is preserved"), Context, Index), Actual.HeartInterpretationState.EncounteredSupportIds[Index], Expected.HeartInterpretationState.EncounteredSupportIds[Index]);
		}
		Test.TestEqual(FString::Printf(TEXT("%s interpretation receipt id is preserved"), Context), Actual.HeartInterpretationState.InterpretationReceipt.ReceiptId, Expected.HeartInterpretationState.InterpretationReceipt.ReceiptId);
		Test.TestEqual(FString::Printf(TEXT("%s interpretation receipt warning is preserved"), Context), Actual.HeartInterpretationState.InterpretationReceipt.WarningId, Expected.HeartInterpretationState.InterpretationReceipt.WarningId);
        return true;
    }

	FVeilHeartInterpretationPersistentState MakeGardenInterpretationState()
	{
		FVeilHeartInterpretationPersistentState State;
		State.PresentedWarningId = TEXT("GardenRot");
		State.EncounteredSupportIds = { TEXT("GardenRot.WitheredVines"), TEXT("GardenRot.ColdSoil") };
		State.InterpretationReceipt.ReceiptId = TEXT("GardenRot.Interpreted");
		State.InterpretationReceipt.PlanId = TEXT("Cycle2_Garden");
		State.InterpretationReceipt.WarningId = TEXT("GardenRot");
		State.InterpretationReceipt.SemanticSubject = TEXT("Cycle2_Garden");
		State.InterpretationReceipt.RestorationTag = TEXT("GardenBed");
		State.InterpretationReceipt.RestorationRitualType = ERitualType::GardenBed;
		State.InterpretationReceipt.RestorationPointIndex = 3;
		State.InterpretationReceipt.SupportIds = State.EncounteredSupportIds;
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamExperienceLegacyV1MigratesSafelyTest,
    "Gloamstead.Experience.Persistence.LegacyV1MigratesSafely",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperienceLegacyV1MigratesSafelyTest::RunTest(const FString& /*Parameters*/)
{
    UGloamsteadSaveGame* SaveGame = NewObject<UGloamsteadSaveGame>();
    SaveGame->SaveVersion = 1;
    SaveGame->WorldSeed = 793;
    SaveGame->RestoredPointIndices = { 1, 3 };
    SaveGame->PointStates.SetNum(2);
    SaveGame->PointStates[0].LightLevel = 0.25f;
    SaveGame->PointStates[0].CorruptionLevel = 0.75f;
    SaveGame->PointStates[1].LightLevel = 0.9f;
    SaveGame->PointStates[1].bIsRestored = true;
    const TArray<FRitualPointState> ExpectedPointStates = SaveGame->PointStates;
    const TArray<int32> ExpectedRestoredIndices = SaveGame->RestoredPointIndices;

    FExperienceCyclePersistentState LegacyAuthoredState;
    LegacyAuthoredState.CompletedCycleSlot = 2;
    LegacyAuthoredState.ArmedPlanId = TEXT("Cycle2_Garden");
    LegacyAuthoredState.LastPlanId = TEXT("Cycle1_Lantern");
    LegacyAuthoredState.LastOutcomeResultTag = TEXT("Cycle2_Failure");
    LegacyAuthoredState.ScarTags = { TEXT("garden_spread") };
    LegacyAuthoredState.bFirstRestCompleted = true;
    LegacyAuthoredState.SavedPhaseOrdinal = 3;
    SaveGame->Test_SetExperienceCycleState(LegacyAuthoredState);

    TestTrue(TEXT("legacy migration succeeds"), SaveGame->MigrateToCurrentVersion());

    TestEqual(TEXT("legacy version advances to current"), SaveGame->SaveVersion, UGloamsteadSaveGame::CurrentSaveVersion);
    TestEqual(TEXT("legacy world seed is preserved"), SaveGame->WorldSeed, 793);
    TestEqual(TEXT("legacy restored-index count is preserved"), SaveGame->RestoredPointIndices.Num(), ExpectedRestoredIndices.Num());
    for (int32 Index = 0; Index < ExpectedRestoredIndices.Num(); ++Index)
    {
        TestEqual(FString::Printf(TEXT("legacy restored index[%d] is preserved"), Index), SaveGame->RestoredPointIndices[Index], ExpectedRestoredIndices[Index]);
    }
    AssertPointStatesEqual(*this, SaveGame->PointStates, ExpectedPointStates);

    const FExperienceCyclePersistentState& MigratedState = SaveGame->GetExperienceCycleState();
    TestEqual(TEXT("legacy migration clears completed cycle slot"), MigratedState.CompletedCycleSlot, 0);
    TestEqual(TEXT("legacy migration clears armed plan"), MigratedState.ArmedPlanId, NAME_None);
    TestEqual(TEXT("legacy migration clears last plan"), MigratedState.LastPlanId, NAME_None);
    TestEqual(TEXT("legacy migration clears outcome tag"), MigratedState.LastOutcomeResultTag, NAME_None);
    TestEqual(TEXT("legacy migration clears scars"), MigratedState.ScarTags.Num(), 0);
    TestFalse(TEXT("legacy migration clears first-rest state"), MigratedState.bFirstRestCompleted);
    TestEqual(TEXT("legacy migration clears saved phase"), MigratedState.SavedPhaseOrdinal, INDEX_NONE);
    TestTrue(TEXT("legacy migration requires explicit reconciliation"), MigratedState.bRequiresLegacyReconciliation);
	TestFalse(TEXT("legacy migration clears unprovable Heart interpretation state"), MigratedState.HeartInterpretationState.HasAnyFacts());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamExperienceCurrentV3RoundTripsTest,
    "Gloamstead.Experience.Persistence.CurrentV3RoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperienceCurrentV3RoundTripsTest::RunTest(const FString& /*Parameters*/)
{
    UGloamsteadSaveGame* SaveGame = NewObject<UGloamsteadSaveGame>();
    TestEqual(TEXT("new saves begin at current version"), SaveGame->SaveVersion, UGloamsteadSaveGame::CurrentSaveVersion);

    FExperienceCyclePersistentState ExpectedState;
    ExpectedState.CompletedCycleSlot = 2;
    ExpectedState.ArmedPlanId = TEXT("Cycle2_Garden");
    ExpectedState.LastPlanId = TEXT("Cycle1_Lantern");
    ExpectedState.LastOutcomeResultTag = TEXT("Cycle2_Partial");
    ExpectedState.ScarTags = { TEXT("garden_spread"), TEXT("heart_dimmed") };
    ExpectedState.bFirstRestCompleted = true;
    ExpectedState.SavedPhaseOrdinal = 4;
    ExpectedState.bRequiresLegacyReconciliation = false;
	ExpectedState.HeartInterpretationState = MakeGardenInterpretationState();
    SaveGame->Test_SetExperienceCycleState(ExpectedState);

    TestTrue(TEXT("current v3 migration succeeds"), SaveGame->MigrateToCurrentVersion());

    const FExperienceCyclePersistentState& ActualState = SaveGame->GetExperienceCycleState();
    TestEqual(TEXT("current version remains current"), SaveGame->SaveVersion, UGloamsteadSaveGame::CurrentSaveVersion);
    TestEqual(TEXT("completed cycle slot round-trips"), ActualState.CompletedCycleSlot, ExpectedState.CompletedCycleSlot);
    TestEqual(TEXT("armed plan id round-trips"), ActualState.ArmedPlanId, ExpectedState.ArmedPlanId);
    TestEqual(TEXT("last plan id round-trips"), ActualState.LastPlanId, ExpectedState.LastPlanId);
    TestEqual(TEXT("last outcome tag round-trips"), ActualState.LastOutcomeResultTag, ExpectedState.LastOutcomeResultTag);
    TestEqual(TEXT("scar count round-trips"), ActualState.ScarTags.Num(), ExpectedState.ScarTags.Num());
    for (int32 Index = 0; Index < ExpectedState.ScarTags.Num(); ++Index)
    {
        TestEqual(FString::Printf(TEXT("scar[%d] round-trips"), Index), ActualState.ScarTags[Index], ExpectedState.ScarTags[Index]);
    }
    TestEqual(TEXT("first-rest state round-trips"), ActualState.bFirstRestCompleted, ExpectedState.bFirstRestCompleted);
    TestEqual(TEXT("saved phase ordinal round-trips"), ActualState.SavedPhaseOrdinal, ExpectedState.SavedPhaseOrdinal);
    TestEqual(TEXT("legacy reconciliation state round-trips"), ActualState.bRequiresLegacyReconciliation, ExpectedState.bRequiresLegacyReconciliation);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamExperienceV2MigrationClearsInterpretationOnlyTest,
	"Gloamstead.Experience.Persistence.V2MigrationClearsInterpretationOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperienceV2MigrationClearsInterpretationOnlyTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadSaveGame* SaveGame = NewObject<UGloamsteadSaveGame>();
	SaveGame->SaveVersion = 2;
	SaveGame->WorldSeed = 947;
	SaveGame->PointStates.SetNum(1);
	SaveGame->PointStates[0].LightLevel = 0.7f;
	FExperienceCyclePersistentState ExpectedState;
	ExpectedState.CompletedCycleSlot = 1;
	ExpectedState.ArmedPlanId = TEXT("Cycle2_Garden");
	ExpectedState.LastPlanId = TEXT("Cycle1_Tutorial");
	ExpectedState.LastOutcomeResultTag = TEXT("TutorialHeld");
	ExpectedState.bFirstRestCompleted = true;
	ExpectedState.SavedPhaseOrdinal = 0;
	ExpectedState.HeartInterpretationState = MakeGardenInterpretationState();
	SaveGame->Test_SetExperienceCycleState(ExpectedState);

	TestTrue(TEXT("v2 migration succeeds"), SaveGame->MigrateToCurrentVersion());
	TestEqual(TEXT("v2 migration advances to v3"), SaveGame->SaveVersion, UGloamsteadSaveGame::CurrentSaveVersion);
	TestEqual(TEXT("v2 migration retains the PCG seed"), SaveGame->WorldSeed, 947);
	TestEqual(TEXT("v2 migration retains the armed authored plan"), SaveGame->GetExperienceCycleState().ArmedPlanId, ExpectedState.ArmedPlanId);
	TestEqual(TEXT("v2 migration retains the completed slot"), SaveGame->GetExperienceCycleState().CompletedCycleSlot, ExpectedState.CompletedCycleSlot);
	TestFalse(TEXT("v2 migration clears unprovable presented warning/evidence/receipt state"), SaveGame->GetExperienceCycleState().HeartInterpretationState.HasAnyFacts());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamExperiencePCGCaptureRetainsCurrentV3PayloadTest,
    "Gloamstead.Experience.Persistence.PCGCaptureRetainsCurrentV3Payload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperiencePCGCaptureRetainsCurrentV3PayloadTest::RunTest(const FString& /*Parameters*/)
{
    UGloamsteadPCGSubsystem* SourceSubsystem = NewObject<UGloamsteadPCGSubsystem>();
    TArray<FRitualPointState> SourcePointStates;
    SourcePointStates.SetNum(2);
    SourcePointStates[0].LightLevel = 0.75f;
    SourcePointStates[0].CorruptionLevel = 0.20f;
    SourcePointStates[0].bIsRestored = true;
    SourcePointStates[1].LightLevel = 0.15f;
    SourcePointStates[1].CorruptionLevel = 0.90f;
    SourceSubsystem->Test_SeedPointStates(SourcePointStates);

    UGloamsteadSaveGame* SaveGame = NewObject<UGloamsteadSaveGame>();
    FExperienceCyclePersistentState ExpectedState;
    ExpectedState.CompletedCycleSlot = 3;
    ExpectedState.ArmedPlanId = TEXT("Cycle3_Heart");
    ExpectedState.LastPlanId = TEXT("Cycle2_Garden");
    ExpectedState.LastOutcomeResultTag = TEXT("Cycle2_Partial");
    ExpectedState.ScarTags = { TEXT("garden_spread"), TEXT("heart_dimmed") };
    ExpectedState.bFirstRestCompleted = true;
    ExpectedState.SavedPhaseOrdinal = 5;
    ExpectedState.bRequiresLegacyReconciliation = true;
	ExpectedState.HeartInterpretationState = MakeGardenInterpretationState();
    SaveGame->Test_SetExperienceCycleState(ExpectedState);

    SourceSubsystem->CaptureToSaveGame(SaveGame);

    TestEqual(TEXT("PCG capture retains the current save version"), SaveGame->SaveVersion, UGloamsteadSaveGame::CurrentSaveVersion);
    AssertExperienceCycleStateEqual(*this, SaveGame->GetExperienceCycleState(), ExpectedState, TEXT("PCG capture"));
    AssertPointStatesEqual(*this, SaveGame->PointStates, SourcePointStates);

    TestTrue(TEXT("a current v3 payload with reconciliation remains migratable"), SaveGame->MigrateToCurrentVersion());
    AssertExperienceCycleStateEqual(*this, SaveGame->GetExperienceCycleState(), ExpectedState, TEXT("second v3 migration"));

    UGloamsteadPCGSubsystem* RestoredSubsystem = NewObject<UGloamsteadPCGSubsystem>();
    TestTrue(TEXT("PCG restore accepts the migrated current payload"), RestoredSubsystem->RestoreFromSaveGame(SaveGame));
    AssertPointStatesEqual(*this, RestoredSubsystem->Test_PeekPointStates(), SourcePointStates);
    AssertExperienceCycleStateEqual(*this, SaveGame->GetExperienceCycleState(), ExpectedState, TEXT("PCG restore"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamExperienceUnsupportedVersionsAreRejectedTest,
    "Gloamstead.Experience.Persistence.UnsupportedVersionsAreRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperienceUnsupportedVersionsAreRejectedTest::RunTest(const FString& /*Parameters*/)
{
	AddExpectedErrorPlain(TEXT("UGloamsteadSaveGame: rejected invalid save version 0."), EAutomationExpectedErrorFlags::Contains, 2);
	AddExpectedErrorPlain(TEXT("UGloamsteadPCGSubsystem: refusing to restore unsupported save version 0."), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("UGloamsteadSaveGame: rejected newer save version"), EAutomationExpectedErrorFlags::Contains, 1);

    UGloamsteadSaveGame* InvalidSave = NewObject<UGloamsteadSaveGame>();
    InvalidSave->SaveVersion = 0;
    InvalidSave->PointStates.SetNum(1);
    InvalidSave->PointStates[0].LightLevel = 0.95f;
    TestFalse(TEXT("version zero is rejected"), InvalidSave->MigrateToCurrentVersion());
    TestEqual(TEXT("version zero remains identifiable after rejection"), InvalidSave->SaveVersion, 0);

    UGloamsteadPCGSubsystem* Subsystem = NewObject<UGloamsteadPCGSubsystem>();
    TArray<FRitualPointState> ExistingPointStates;
    ExistingPointStates.SetNum(1);
    ExistingPointStates[0].LightLevel = 0.25f;
    Subsystem->Test_SeedPointStates(ExistingPointStates);
    TestFalse(TEXT("PCG restore rejects an invalid payload before reading its point state"), Subsystem->RestoreFromSaveGame(InvalidSave));
    AssertPointStatesEqual(*this, Subsystem->Test_PeekPointStates(), ExistingPointStates);

    UGloamsteadSaveGame* FutureSave = NewObject<UGloamsteadSaveGame>();
    FutureSave->SaveVersion = UGloamsteadSaveGame::CurrentSaveVersion + 1;
    TestFalse(TEXT("future version is rejected"), FutureSave->MigrateToCurrentVersion());
    TestEqual(TEXT("future version remains identifiable after rejection"), FutureSave->SaveVersion, UGloamsteadSaveGame::CurrentSaveVersion + 1);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
