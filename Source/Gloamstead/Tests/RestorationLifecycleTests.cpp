// Adversarial tests for the ritual restoration lifecycle.
//
// These attack the guards on UGloamsteadPCGSubsystem::ApplyRestoration and the state views around it:
// one failure class per test, each asserting that a rejected call mutates NOTHING rather than merely
// returning false. Worldless (NewObject'd subsystems + the Test_SeedPointStates seam) — the paths that
// genuinely need a live PIE world are listed at the bottom of this file with the reason they are absent.
#include "Misc/AutomationTest.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Components/RitualPlacementComponent.h"
#include "Data/RitualTypes.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

// Helper names here must be distinctive across the whole module, not merely file-local. UBT builds this
// module with adaptive non-unity: which files share a unity blob depends on the current git working set,
// so an anonymous-namespace helper and a `static` helper of the same signature in another test file only
// collide once the grouping happens to place them together (C2668). PCGSubsystemTests.cpp:7 already owns
// the name MakeSeededSubsystem; see commit bd6232e for the same hazard in the survey-subject JSON helpers.
namespace
{
    UGloamsteadPCGSubsystem* MakeLifecycleSubsystem(const TArray<FRitualPointState>& States)
    {
        UGloamsteadPCGSubsystem* Sub = NewObject<UGloamsteadPCGSubsystem>();
        Sub->Test_SeedPointStates(States);
        return Sub;
    }

    // Three clean, distinguishable points. Index 0 is the usual target.
    TArray<FRitualPointState> MakeCleanStates()
    {
        TArray<FRitualPointState> States;
        States.SetNum(3);
        States[0].LightLevel = 0.10f; States[0].CorruptionLevel = 0.60f;
        States[1].LightLevel = 0.20f; States[1].CorruptionLevel = 0.50f;
        States[2].LightLevel = 0.30f; States[2].CorruptionLevel = 0.40f;
        return States;
    }

    FRestorationEventPayload MakePayload(int32 PointIndex, float LightDelta, float CorruptionCleared)
    {
        FRestorationEventPayload P;
        P.PointIndex = PointIndex;
        P.LightDelta = LightDelta;
        P.CorruptionCleared = CorruptionCleared;
        return P;
    }

    // Substring of the Error the subsystem logs on an index mismatch. An unexpected Error fails an
    // automation test, so tests that deliberately trip this guard must declare it first.
    const TCHAR* IndexMismatchLog = TEXT("ApplyRestoration rejected - payload index");
}

// ===== Failure class: the payload never named a point (the -1 default) =====
// A payload that was default-constructed and handed straight to ApplyRestoration must be refused: every
// OnStructureRestored listener indexes off Payload.PointIndex, so -1 would mutate here and report nowhere.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestoreUnassignedPayloadIndexTest,
    "Gloamstead.Restoration.UnassignedPayloadIndexIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestoreUnassignedPayloadIndexTest::RunTest(const FString& /*Parameters*/)
{
    UGloamsteadPCGSubsystem* Sub = MakeLifecycleSubsystem(MakeCleanStates());
    AddExpectedErrorPlain(IndexMismatchLog, EAutomationExpectedErrorFlags::Contains, 1);

    FRestorationEventPayload Unassigned; // PointIndex left at its -1 default
    Unassigned.LightDelta = 0.5f;
    Unassigned.CorruptionCleared = 0.5f;

    TestFalse(TEXT("a payload that never named a point is rejected"), Sub->ApplyRestoration(0, Unassigned));

    const TArray<FRitualPointState>& Out = Sub->Test_PeekPointStates();
    TestEqual(TEXT("rejected: light untouched"), Out[0].LightLevel, 0.10f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("rejected: corruption untouched"), Out[0].CorruptionLevel, 0.60f, KINDA_SMALL_NUMBER);
    TestFalse(TEXT("rejected: point not flagged restored"), Out[0].bIsRestored);
    TestEqual(TEXT("rejected: restored count still zero"), Sub->GetRestoredPointCount(), 0);
    return true;
}

// ===== Failure class: the payload names a different (valid) point than the target =====
// Neither the target nor the point the payload names may be mutated — a mismatch is a confused caller,
// not a routing hint.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestoreCrossIndexPayloadTest,
    "Gloamstead.Restoration.CrossIndexPayloadIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestoreCrossIndexPayloadTest::RunTest(const FString& /*Parameters*/)
{
    UGloamsteadPCGSubsystem* Sub = MakeLifecycleSubsystem(MakeCleanStates());
    AddExpectedErrorPlain(IndexMismatchLog, EAutomationExpectedErrorFlags::Contains, 1);

    // Payload describes point 1; the call targets point 0.
    const FRestorationEventPayload ForPointOne = MakePayload(1, /*LightDelta*/ 0.5f, /*CorruptionCleared*/ 0.5f);
    TestFalse(TEXT("a payload for another point is rejected"), Sub->ApplyRestoration(0, ForPointOne));

    const TArray<FRitualPointState>& Out = Sub->Test_PeekPointStates();
    TestEqual(TEXT("target point 0 light untouched"), Out[0].LightLevel, 0.10f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("target point 0 corruption untouched"), Out[0].CorruptionLevel, 0.60f, KINDA_SMALL_NUMBER);
    TestFalse(TEXT("target point 0 not flagged restored"), Out[0].bIsRestored);
    TestEqual(TEXT("named point 1 light untouched"), Out[1].LightLevel, 0.20f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("named point 1 corruption untouched"), Out[1].CorruptionLevel, 0.50f, KINDA_SMALL_NUMBER);
    TestFalse(TEXT("named point 1 not flagged restored"), Out[1].bIsRestored);
    TestEqual(TEXT("nothing entered the restored set"), Sub->GetRestoredPointCount(), 0);
    return true;
}

// ===== Failure class: the same point restored twice through the normal path =====
// The second call must not stack another LightDelta or clear corruption a second time.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestoreDoubleRestorationTest,
    "Gloamstead.Restoration.DoubleRestorationIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestoreDoubleRestorationTest::RunTest(const FString& /*Parameters*/)
{
    UGloamsteadPCGSubsystem* Sub = MakeLifecycleSubsystem(MakeCleanStates());

    const FRestorationEventPayload First = MakePayload(0, /*LightDelta*/ 0.20f, /*CorruptionCleared*/ 0.10f);
    TestTrue(TEXT("the first restoration is accepted"), Sub->ApplyRestoration(0, First));

    const float LightAfterFirst = Sub->GetLightLevel(0);
    const float CorruptionAfterFirst = Sub->GetCorruptionLevel(0);
    TestEqual(TEXT("first restoration added its light"), LightAfterFirst, 0.30f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("first restoration cleared its corruption"), CorruptionAfterFirst, 0.50f, KINDA_SMALL_NUMBER);

    // A second, individually well-formed restoration of the same point.
    const FRestorationEventPayload Second = MakePayload(0, /*LightDelta*/ 0.50f, /*CorruptionCleared*/ 0.50f);
    TestFalse(TEXT("restoring an already-restored point is rejected"), Sub->ApplyRestoration(0, Second));

    TestEqual(TEXT("no second helping of light"), Sub->GetLightLevel(0), LightAfterFirst, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("no second corruption clear"), Sub->GetCorruptionLevel(0), CorruptionAfterFirst, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("the point is counted once, not twice"), Sub->GetRestoredPointCount(), 1);
    return true;
}

// ===== Failure class: Blueprint bypass onto a point restored outside ApplyRestoration =====
// ApplyRestoration is BlueprintCallable, so Blueprint can reach it without passing through placement.
// The guard must read the point's own restored flag, not the restored-index set — a point whose flag came
// from seeded/loaded state (with the index set empty) is still protected.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestoreBlueprintBypassTest,
    "Gloamstead.Restoration.BlueprintBypassOnRestoredPointIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestoreBlueprintBypassTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States = MakeCleanStates();
    States[0].bIsRestored = true; // flag installed directly; the restored-index set stays empty
    UGloamsteadPCGSubsystem* Sub = MakeLifecycleSubsystem(States);

    TestTrue(TEXT("the point reads as restored before the bypass"), Sub->IsPointRestored(0));
    TestFalse(TEXT("the restored-index set does not know about it yet"), Sub->GetRestoredPointIndices().Contains(0));

    const FRestorationEventPayload Bypass = MakePayload(0, /*LightDelta*/ 0.50f, /*CorruptionCleared*/ 0.50f);
    TestFalse(TEXT("a direct call onto an already-restored point is rejected"), Sub->ApplyRestoration(0, Bypass));

    const TArray<FRitualPointState>& Out = Sub->Test_PeekPointStates();
    TestEqual(TEXT("bypass added no light"), Out[0].LightLevel, 0.10f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("bypass cleared no corruption"), Out[0].CorruptionLevel, 0.60f, KINDA_SMALL_NUMBER);
    TestTrue(TEXT("the point is still restored"), Out[0].bIsRestored);
    TestFalse(TEXT("the rejected call did not smuggle the index into the restored set"),
        Sub->GetRestoredPointIndices().Contains(0));
    return true;
}

// ===== Failure class: a ritual type with no definition asset yields a zeroed restoration =====
// BuildRestorationPayload seeds LightDelta/CorruptionCleared from these free functions and only overwrites
// them when a URitualDefinition is present (RitualPlacementComponent.cpp:210-216). If these ever return 0
// for the Phase 1 restorable types, a missing DA_Ritual_* asset silently produces an inert restoration.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestoreMissingDefinitionFallbackTest,
    "Gloamstead.Restoration.MissingDefinitionFallsBackToTypeDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestoreMissingDefinitionFallbackTest::RunTest(const FString& /*Parameters*/)
{
    TestEqual(TEXT("LanternPost light default"), GetDefaultLightContribution(ERitualType::LanternPost), 0.35f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("LanternPost corruption default"), GetDefaultCorruptionClearance(ERitualType::LanternPost), 0.20f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("GardenBed light default"), GetDefaultLightContribution(ERitualType::GardenBed), 0.15f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("GardenBed corruption default"), GetDefaultCorruptionClearance(ERitualType::GardenBed), 0.35f, KINDA_SMALL_NUMBER);

    // The point of the test: the fallback is not a zero, so a missing definition asset still mends.
    TestTrue(TEXT("LanternPost fallback is non-zero light"), GetDefaultLightContribution(ERitualType::LanternPost) > 0.f);
    TestTrue(TEXT("GardenBed fallback is non-zero light"), GetDefaultLightContribution(ERitualType::GardenBed) > 0.f);

    // Types that are not directly restorable in Phase 1 contribute nothing, by design.
    // Cycle III is the cycle that asks for a road, so PathPoint is now a real restorable ritual. It
    // carries light rather than making it: enough to link two lanterns, less than either of them.
    TestTrue(TEXT("PathPoint carries some light"), GetDefaultLightContribution(ERitualType::PathPoint) > 0.f);
    TestTrue(TEXT("a road carries less light than the lantern it serves"),
        GetDefaultLightContribution(ERitualType::PathPoint) < GetDefaultLightContribution(ERitualType::LanternPost));
    TestEqual(TEXT("Invalid contributes no light"), GetDefaultLightContribution(ERitualType::Invalid), 0.f, KINDA_SMALL_NUMBER);

    // Every authored ritual form the arc asks for must be placeable by the player, or its own cycle
    // could never resolve its objective.
    TestTrue(TEXT("PathPoint is directly restorable"), IsDirectlyRestorable(ERitualType::PathPoint));
    TestTrue(TEXT("MirrorPillar is directly restorable"), IsDirectlyRestorable(ERitualType::MirrorPillar));
    TestTrue(TEXT("BellShrine is directly restorable"), IsDirectlyRestorable(ERitualType::BellShrine));
    TestTrue(TEXT("AnchorStone is directly restorable"), IsDirectlyRestorable(ERitualType::AnchorStone));
    TestFalse(TEXT("Invalid is never restorable"), IsDirectlyRestorable(ERitualType::Invalid));

    // An anchor binds lights that already exist, so it is authored as the smallest contribution.
    TestTrue(TEXT("AnchorStone contributes the least light of the six"),
        GetDefaultLightContribution(ERitualType::AnchorStone) < GetDefaultLightContribution(ERitualType::PathPoint));
    return true;
}

// ===== Failure class: the two restored views drift apart under live mutation =====
// IsPointRestored() answers from PointStates; GetRestoredPointCount()/CaptureToSaveGame() answer from the
// restored-index set. Every mutating path must keep them saying the same thing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestoreViewsAgreeLiveTest,
    "Gloamstead.Restoration.RestoredViewsAgreeAfterMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestoreViewsAgreeLiveTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States;
    States.SetNum(5);
    UGloamsteadPCGSubsystem* Sub = MakeLifecycleSubsystem(States);

    TestTrue(TEXT("restore point 0"), Sub->ApplyRestoration(0, MakePayload(0, 0.2f, 0.1f)));
    TestTrue(TEXT("restore point 3"), Sub->ApplyRestoration(3, MakePayload(3, 0.2f, 0.1f)));
    TestTrue(TEXT("the night reclaims point 0"), Sub->RevertRestoration(0));
    TestTrue(TEXT("restore point 1"), Sub->ApplyRestoration(1, MakePayload(1, 0.2f, 0.1f)));

    const TSet<int32> Restored = Sub->GetRestoredPointIndices();
    const TArray<FRitualPointState>& Out = Sub->Test_PeekPointStates();
    int32 FlaggedCount = 0;
    for (int32 i = 0; i < Out.Num(); ++i)
    {
        if (Out[i].bIsRestored)
        {
            ++FlaggedCount;
        }
        TestEqual(FString::Printf(TEXT("point %d: flag and restored set agree"), i),
            Out[i].bIsRestored, Restored.Contains(i));
    }
    TestEqual(TEXT("restored count matches the number of flagged points"), Sub->GetRestoredPointCount(), FlaggedCount);
    TestEqual(TEXT("exactly the two surviving restorations are counted"), FlaggedCount, 2);
    TestFalse(TEXT("the reclaimed point left the set"), Restored.Contains(0));
    return true;
}

// ===== Failure class: the two restored views drift apart across the persistence round-trip =====
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestoreViewsAgreeAfterLoadTest,
    "Gloamstead.Restoration.RestoredViewsAgreeAfterLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestoreViewsAgreeAfterLoadTest::RunTest(const FString& /*Parameters*/)
{
    const FString Slot = TEXT("GloamsteadTest_RestorationViewsAgree");
    if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        UGameplayStatics::DeleteGameInSlot(Slot, 0);
    }

    TArray<FRitualPointState> States;
    States.SetNum(4);
    UGloamsteadPCGSubsystem* Sub = MakeLifecycleSubsystem(States);
    TestTrue(TEXT("restore point 1"), Sub->ApplyRestoration(1, MakePayload(1, 0.3f, 0.2f)));
    TestTrue(TEXT("restore point 2"), Sub->ApplyRestoration(2, MakePayload(2, 0.3f, 0.2f)));
    TestTrue(TEXT("the night reclaims point 2"), Sub->RevertRestoration(2));
    TestTrue(TEXT("state saves"), Sub->SaveToSlot(Slot, 0));

    UGloamsteadPCGSubsystem* Reloaded = NewObject<UGloamsteadPCGSubsystem>();
    TestTrue(TEXT("state loads"), Reloaded->LoadFromSlot(Slot, 0));

    const TSet<int32> Restored = Reloaded->GetRestoredPointIndices();
    const TArray<FRitualPointState>& Out = Reloaded->Test_PeekPointStates();
    int32 FlaggedCount = 0;
    for (int32 i = 0; i < Out.Num(); ++i)
    {
        if (Out[i].bIsRestored)
        {
            ++FlaggedCount;
        }
        TestEqual(FString::Printf(TEXT("loaded point %d: flag and restored set agree"), i),
            Out[i].bIsRestored, Restored.Contains(i));
    }
    TestEqual(TEXT("loaded restored count matches the flagged points"), Reloaded->GetRestoredPointCount(), FlaggedCount);
    TestEqual(TEXT("only point 1 survived the reclaim"), FlaggedCount, 1);
    TestTrue(TEXT("point 1 is the survivor"), Reloaded->IsPointRestored(1));

    UGameplayStatics::DeleteGameInSlot(Slot, 0); // teardown
    return true;
}

// ===== Failure class: a failed re-initialization wipes live restoration state =====
// InitializeFromPCGComponent bails on a null component before touching anything. Repeating it must not
// clear PointStates or the restored set — a re-init that finds no PCG data has to leave the sanctuary
// exactly as it found it, not silently reset the night's progress.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestoreFailedReinitPreservesStateTest,
    "Gloamstead.Restoration.FailedReinitializationPreservesState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestoreFailedReinitPreservesStateTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States;
    States.SetNum(3);
    UGloamsteadPCGSubsystem* Sub = MakeLifecycleSubsystem(States);
    TestTrue(TEXT("restore point 2"), Sub->ApplyRestoration(2, MakePayload(2, 0.4f, 0.1f)));

    const int32 PointsBefore = Sub->Test_PeekPointStates().Num();
    const int32 RestoredBefore = Sub->GetRestoredPointCount();
    const float LightBefore = Sub->GetLightLevel(2);

    Sub->Test_InitializeFromPCGComponent(nullptr, /*WorldSeed*/ 1234);
    Sub->Test_InitializeFromPCGComponent(nullptr, /*WorldSeed*/ 5678); // repeating it changes nothing either

    TestEqual(TEXT("point count survives a failed re-init"), Sub->Test_PeekPointStates().Num(), PointsBefore);
    TestEqual(TEXT("restored count survives a failed re-init"), Sub->GetRestoredPointCount(), RestoredBefore);
    TestEqual(TEXT("light survives a failed re-init"), Sub->GetLightLevel(2), LightBefore, KINDA_SMALL_NUMBER);
    TestTrue(TEXT("the restored point is still restored"), Sub->IsPointRestored(2));
    TestTrue(TEXT("the restored set still holds the index"), Sub->GetRestoredPointIndices().Contains(2));
    return true;
}

// ===== Failure class: re-applying the same restored set changes the answer =====
// The load-on-start path can reapply the same snapshot more than once; a second application must be a
// no-op rather than double-counting or dropping indices.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestoreReapplyIsIdempotentTest,
    "Gloamstead.Restoration.ReapplyRestoredStateIsIdempotent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestoreReapplyIsIdempotentTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FRitualPointState> States;
    States.SetNum(4);
    UGloamsteadPCGSubsystem* Sub = MakeLifecycleSubsystem(States);

    TSet<int32> Snapshot;
    Snapshot.Add(1);
    Snapshot.Add(3);

    Sub->ReapplyRestoredState(Snapshot);
    const int32 CountAfterFirst = Sub->GetRestoredPointCount();
    const TArray<FRitualPointState> StatesAfterFirst = Sub->Test_PeekPointStates(); // by value: a snapshot to compare against

    Sub->ReapplyRestoredState(Snapshot);

    TestEqual(TEXT("restored count is unchanged by the second apply"), Sub->GetRestoredPointCount(), CountAfterFirst);
    TestEqual(TEXT("restored count is the snapshot size"), CountAfterFirst, 2);

    const TArray<FRitualPointState>& StatesAfterSecond = Sub->Test_PeekPointStates();
    TestEqual(TEXT("point count is unchanged"), StatesAfterSecond.Num(), StatesAfterFirst.Num());
    for (int32 i = 0; i < StatesAfterFirst.Num(); ++i)
    {
        TestEqual(FString::Printf(TEXT("point %d restored flag is unchanged"), i),
            StatesAfterSecond[i].bIsRestored, StatesAfterFirst[i].bIsRestored);
        TestEqual(FString::Printf(TEXT("point %d light is unchanged"), i),
            StatesAfterSecond[i].LightLevel, StatesAfterFirst[i].LightLevel, KINDA_SMALL_NUMBER);
        TestEqual(FString::Printf(TEXT("point %d corruption is unchanged"), i),
            StatesAfterSecond[i].CorruptionLevel, StatesAfterFirst[i].CorruptionLevel, KINDA_SMALL_NUMBER);
    }
    return true;
}

// ===== Failure class: placement leaves preview state behind when it cannot start =====
// A component whose subsystem never resolved (BeginPlay ran without a PCG subsystem, or teardown already
// dropped it) must not report a live preview target, and cancelling must not resurrect one. Nothing here
// may be confirmable.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamRestorePlacementLeavesNoPreviewTest,
    "Gloamstead.Restoration.CancelledPlacementLeavesNoPreviewState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamRestorePlacementLeavesNoPreviewTest::RunTest(const FString& /*Parameters*/)
{
    URitualPlacementComponent* Placement = NewObject<URitualPlacementComponent>();

    // No BeginPlay, so no cached subsystem — the "world is not (or no longer) there" shape.
    Placement->EnterPlacementMode();
    TestFalse(TEXT("placement mode does not start without a subsystem"), Placement->IsInPlacementMode());
    TestEqual(TEXT("no preview target after a refused entry"), Placement->GetCurrentTargetPointInfo().PointIndex, -1);
    TestFalse(TEXT("no valid placement after a refused entry"), Placement->IsCurrentPlacementValid());

    Placement->ExitPlacementMode(); // cancellation/teardown must be a safe no-op
    TestFalse(TEXT("still not in placement mode after cancelling"), Placement->IsInPlacementMode());
    TestEqual(TEXT("no preview target after cancelling"), Placement->GetCurrentTargetPointInfo().PointIndex, -1);
    TestTrue(TEXT("no preview ritual type is claimed"),
        Placement->GetCurrentTargetRitualType() == ERitualType::Invalid);

    FVector OutLocation = FVector(1.f, 2.f, 3.f);
    FRotator OutRotation = FRotator(4.f, 5.f, 6.f);
    TestFalse(TEXT("no target transform is offered after cancelling"),
        Placement->GetCurrentTargetTransform(OutLocation, OutRotation));

    TestFalse(TEXT("nothing can be confirmed after cancelling"), Placement->ConfirmPlacement());
    return true;
}

// ===== Not covered here, and why =====
//
// * "final actor spawn failure leaves no orphan actor" — configured spawn and one-shot confirmation are
//   covered by Gloamstead.FirstNight.PlayableSlice; rejection cleanup remains a separate failure-injection
//   concern because ApplyRestoration must be made to reject after a successful spawn.
//
// * "restored-set divergence after the InitializeFromPCGComponent rebuild" — the rebuild at
//   GloamsteadPCGSubsystem.cpp:105-116 only runs after a UPCGComponent yields point data
//   (GloamsteadPCGSubsystem.cpp:52-72). Constructing generated PCG graph output worldlessly is not
//   possible; the reachable slice (a failed re-init must not wipe state) is covered above by
//   Gloamstead.Restoration.FailedReinitializationPreservesState, and the invariant the rebuild exists to
//   uphold is covered by Gloamstead.Restoration.RestoredViewsAgreeAfterMutation / ...AfterLoad.
//
// * "cancellation mid-placement" — the never-entered branch is covered here; the UI/preview teardown
//   presentation still needs PIE.

#endif // WITH_DEV_AUTOMATION_TESTS
