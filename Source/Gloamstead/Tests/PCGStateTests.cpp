// Gloamstead PCG subsystem state/persistence invariants — grounded over shipped logic.
#include "Misc/AutomationTest.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Save/GloamsteadSaveGame.h"
#include "Data/RitualTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    UGloamsteadPCGSubsystem* MakeSeeded(const TArray<FRitualPointState>& States)
    {
        UGloamsteadPCGSubsystem* Sub = NewObject<UGloamsteadPCGSubsystem>();
        Sub->Test_SeedPointStates(States);
        return Sub;
    }
}

// Round-trip of the lightweight restored-set persistence: ApplyRestoration -> GetRestoredPointIndices
// -> (wipe) -> ReapplyRestoredState restores the same indices/flags.
// NOTE: ReapplyRestoredState only restores bIsRestored by contract — light/corruption are NOT
// round-tripped here (that gap is what the full SaveGame path below covers).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamPCGRestoredSetRoundTripTest,
    "Gloamstead.PCG.RestoredSetRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamPCGRestoredSetRoundTripTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States;
    States.SetNum(6);

    UGloamsteadPCGSubsystem* Sub = MakeSeeded(States);
    const FRestorationEventPayload Payload;
    Sub->ApplyRestoration(1, Payload);
    Sub->ApplyRestoration(4, Payload);

    const TSet<int32> Snapshot = Sub->GetRestoredPointIndices();
    TestEqual(TEXT("two indices captured"), Snapshot.Num(), 2);

    // Wipe: a fresh subsystem with clean state, then reapply the snapshot.
    TArray<FRitualPointState> Fresh;
    Fresh.SetNum(6);
    UGloamsteadPCGSubsystem* Reloaded = MakeSeeded(Fresh);
    Reloaded->ReapplyRestoredState(Snapshot);

    TestTrue(TEXT("index 1 restored after reapply"), Reloaded->IsPointRestored(1));
    TestTrue(TEXT("index 4 restored after reapply"), Reloaded->IsPointRestored(4));
    TestFalse(TEXT("index 0 not restored"), Reloaded->IsPointRestored(0));
    TestEqual(TEXT("restored set size round-trips"), Reloaded->GetRestoredPointIndices().Num(), 2);
    return true;
}

// Exact-value aggregate math over a hand-built point set with known answers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamPCGAggregateExactTest,
    "Gloamstead.PCG.AggregatesExactValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamPCGAggregateExactTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States;
    States.SetNum(4);
    States[0].LightLevel = 0.1f; States[0].CorruptionLevel = 0.0f;
    States[1].LightLevel = 0.2f; States[1].CorruptionLevel = 0.5f;
    States[2].LightLevel = 0.3f; States[2].CorruptionLevel = 0.5f;
    States[3].LightLevel = 0.4f; States[3].CorruptionLevel = 1.0f;

    UGloamsteadPCGSubsystem* Sub = MakeSeeded(States);
    TestEqual(TEXT("avg light = (0.1+0.2+0.3+0.4)/4 = 0.25"), Sub->GetSanctuaryAverageLightLevel(), 0.25f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("avg corruption = (0+0.5+0.5+1)/4 = 0.5"), Sub->GetSanctuaryAverageCorruptionLevel(), 0.5f, KINDA_SMALL_NUMBER);
    return true;
}

// ApplyRestoration mutates the target point per the documented formula, including the corruption floor at 0.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamPCGApplyRestorationMutatesTest,
    "Gloamstead.PCG.ApplyRestorationMutates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamPCGApplyRestorationMutatesTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States;
    States.SetNum(2);
    States[0].LightLevel = 0.1f; States[0].CorruptionLevel = 0.5f;
    States[1].LightLevel = 0.0f; States[1].CorruptionLevel = 0.5f;

    UGloamsteadPCGSubsystem* Sub = MakeSeeded(States);

    FRestorationEventPayload AddLight;
    AddLight.LightDelta = 0.2f;
    AddLight.CorruptionCleared = 0.3f;
    TestTrue(TEXT("restore index 0 succeeds"), Sub->ApplyRestoration(0, AddLight));

    FRestorationEventPayload OverClear;
    OverClear.CorruptionCleared = 0.9f; // exceeds current 0.5 -> must clamp at 0, not go negative
    TestTrue(TEXT("restore index 1 succeeds"), Sub->ApplyRestoration(1, OverClear));

    const TArray<FRitualPointState>& Out = Sub->Test_PeekPointStates();
    TestEqual(TEXT("light += delta (0.1+0.2)"), Out[0].LightLevel, 0.3f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("corruption -= cleared (0.5-0.3)"), Out[0].CorruptionLevel, 0.2f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("corruption floored at 0"), Out[1].CorruptionLevel, 0.0f, KINDA_SMALL_NUMBER);
    TestTrue(TEXT("index 0 flagged restored"), Out[0].bIsRestored);
    TestEqual(TEXT("restored count = 2"), Sub->GetRestoredPointCount(), 2);
    return true;
}

// Full per-point persistence round-trip via UGloamsteadSaveGame: capture -> restore into a fresh
// subsystem -> ALL fields equal (light + corruption + flags), plus the restored index set.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamPCGSaveGameRoundTripTest,
    "Gloamstead.PCG.SaveGameFullRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamPCGSaveGameRoundTripTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States;
    States.SetNum(5);
    for (int32 i = 0; i < States.Num(); ++i)
    {
        States[i].LightLevel = 0.1f * static_cast<float>(i);
        States[i].CorruptionLevel = 0.05f * static_cast<float>(i);
        States[i].bIsRestored = (i % 2 == 0);
    }

    UGloamsteadPCGSubsystem* Sub = MakeSeeded(States);
    FRestorationEventPayload Payload;
    Payload.LightDelta = 0.15f;
    Sub->ApplyRestoration(2, Payload); // mutate one point and register it as restored

    UGloamsteadSaveGame* SaveGame = NewObject<UGloamsteadSaveGame>();
    Sub->CaptureToSaveGame(SaveGame);

    UGloamsteadPCGSubsystem* Reloaded = NewObject<UGloamsteadPCGSubsystem>();
    Reloaded->RestoreFromSaveGame(SaveGame);

    const TArray<FRitualPointState>& A = Sub->Test_PeekPointStates();
    const TArray<FRitualPointState>& B = Reloaded->Test_PeekPointStates();
    if (!TestEqual(TEXT("point count round-trips"), B.Num(), A.Num()))
    {
        return false;
    }
    for (int32 i = 0; i < A.Num(); ++i)
    {
        TestEqual(FString::Printf(TEXT("light[%d] round-trips"), i), B[i].LightLevel, A[i].LightLevel, KINDA_SMALL_NUMBER);
        TestEqual(FString::Printf(TEXT("corruption[%d] round-trips"), i), B[i].CorruptionLevel, A[i].CorruptionLevel, KINDA_SMALL_NUMBER);
        TestEqual(FString::Printf(TEXT("restored[%d] round-trips"), i), B[i].bIsRestored, A[i].bIsRestored);
    }
    TestEqual(TEXT("restored set size round-trips"), Reloaded->GetRestoredPointIndices().Num(), Sub->GetRestoredPointIndices().Num());
    TestTrue(TEXT("restored index 2 present after load"), Reloaded->GetRestoredPointIndices().Contains(2));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
