// Gloamstead PCG subsystem invariants — grounded checks over already-shipped logic.
#include "Misc/AutomationTest.h"
#include "PCG/GloamsteadPCGSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

static UGloamsteadPCGSubsystem* MakeSeededSubsystem(const TArray<FRitualPointState>& States)
{
    UGloamsteadPCGSubsystem* Sub = NewObject<UGloamsteadPCGSubsystem>();
    Sub->Test_SeedPointStates(States);
    return Sub;
}

// A. Aggregates are safe with no points (header contract: "safe defaults when uninitialized").
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamPCGEmptyDefaultsTest,
    "Gloamstead.PCG.EmptyStateSafeDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamPCGEmptyDefaultsTest::RunTest(const FString& /*Parameters*/)
{
    UGloamsteadPCGSubsystem* Sub = NewObject<UGloamsteadPCGSubsystem>();
    TestEqual(TEXT("restored count is 0 when empty"), Sub->GetRestoredPointCount(), 0);
    TestEqual(TEXT("avg light is 0 when empty"), Sub->GetSanctuaryAverageLightLevel(), 0.0f);
    TestEqual(TEXT("avg corruption is 0 when empty"), Sub->GetSanctuaryAverageCorruptionLevel(), 0.0f);
    Sub->BuildSanctuarySnapshot(); // must not divide-by-zero on an empty set
    return true;
}

// B. Corruption stays in [0,1] under repeated spread (doc: "clamped 0-1").
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamPCGCorruptionClampTest,
    "Gloamstead.PCG.CorruptionClamped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamPCGCorruptionClampTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States;
    States.SetNum(16);
    for (FRitualPointState& S : States) { S.CorruptionLevel = 0.9f; }

    UGloamsteadPCGSubsystem* Sub = MakeSeededSubsystem(States);
    for (int32 i = 0; i < 50; ++i) { Sub->ApplyCorruptionSpread(0.5f, 32); }

    const TArray<FRitualPointState>& Out = Sub->Test_PeekPointStates();
    bool bAnyAtCap = false;
    for (int32 i = 0; i < Out.Num(); ++i)
    {
        TestTrue(FString::Printf(TEXT("corruption[%d] <= 1.0"), i), Out[i].CorruptionLevel <= 1.0f);
        TestTrue(FString::Printf(TEXT("corruption[%d] >= 0.0"), i), Out[i].CorruptionLevel >= 0.0f);
        if (Out[i].CorruptionLevel >= 1.0f - KINDA_SMALL_NUMBER) { bAnyAtCap = true; }
    }
    // Guards against a vacuous pass: if spread did nothing, nothing would hit the cap.
    TestTrue(TEXT("spread actually ran and clamp engaged"), bAnyAtCap);
    return true;
}

// C. Corruption spread never flips restoration flags (doc: "does not alter restoration flags").
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamPCGSpreadPreservesRestoredTest,
    "Gloamstead.PCG.SpreadPreservesRestoredFlags",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamPCGSpreadPreservesRestoredTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States;
    States.SetNum(10);
    TArray<bool> Expected;
    Expected.SetNum(States.Num());
    for (int32 i = 0; i < States.Num(); ++i)
    {
        const bool bRestored = (i % 2 == 0);
        States[i].bIsRestored = bRestored;
        Expected[i] = bRestored;
    }

    UGloamsteadPCGSubsystem* Sub = MakeSeededSubsystem(States);
    for (int32 i = 0; i < 20; ++i) { Sub->ApplyCorruptionSpread(0.3f, 32); }

    const TArray<FRitualPointState>& Out = Sub->Test_PeekPointStates();
    if (!TestEqual(TEXT("point count unchanged"), Out.Num(), Expected.Num())) { return false; }
    for (int32 i = 0; i < Out.Num(); ++i)
    {
        TestTrue(FString::Printf(TEXT("bIsRestored[%d] preserved"), i), Out[i].bIsRestored == Expected[i]);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
