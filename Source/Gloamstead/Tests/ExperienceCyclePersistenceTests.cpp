// Cycle II persistence invariants: legacy saves must never invent authored progression.
#include "Misc/AutomationTest.h"
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
    SaveGame->SetExperienceCycleState(LegacyAuthoredState);

    SaveGame->MigrateToCurrentVersion();

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
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamExperienceCurrentV2RoundTripsTest,
    "Gloamstead.Experience.Persistence.CurrentV2RoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamExperienceCurrentV2RoundTripsTest::RunTest(const FString& /*Parameters*/)
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
    SaveGame->SetExperienceCycleState(ExpectedState);

    SaveGame->MigrateToCurrentVersion();

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

#endif // WITH_DEV_AUTOMATION_TESTS
