// Confirmation evidence: what the ritual confirm path is allowed to claim about itself.
//
// The property under test throughout is the ORDER OF AUTHORITY. Restoration is gameplay and is
// authoritative; the request-bound survey artifact is a report about gameplay and is downstream of it.
// So: a report that cannot be written must not undo the restoration, a restoration that never happened
// must not produce a success report, one confirmation must produce exactly one request id that every
// layer — resolution, publication and the HUD getters — agrees on, and a restoration that consumed the
// point without materialising an actor must not be reported as a clean one.
//
// These drive URitualPlacementComponent::Test_CommitRestorationWithEvidence, which is the same function
// ConfirmPlacement calls (RitualPlacementComponent.cpp:127). What cannot be driven from C++ at all
// is listed at the bottom of this file with the reason.
#include "Misc/AutomationTest.h"
#include "Components/RitualPlacementComponent.h"
#include "Components/GloamsteadSurveySubjectComponent.h"
#include "Systems/GloamsteadSurveySubjectRegistry.h"
#include "Systems/VeilHeart.h"
#include "Data/GloamsteadSurveySubjectTypes.h"
#include "Data/RitualTypes.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

// Named, not anonymous. An anonymous namespace does NOT keep helper names apart once files land in the
// same unity translation unit — the C2264 already paid for once in GloamsteadSurveySubjectTypes.cpp:170-174.
namespace GloamConfirmationEvidence
{
    /** The subject a URitualPlacementComponent stamps its confirmations with by default. */
    static const TCHAR* const SubjectId = TEXT("courtyard.lantern.first");

    /** A live world, torn down however the test returns. Same construction as SurveySubjectRegistryTests. */
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
                World = nullptr;
            }
        }
    };

    /**
     * A placement component owned by an actor in World, with NO BeginPlay run.
     *
     * Deliberate: BeginPlay would cache the PCG subsystem and load the DA_Ritual_* assets, neither of
     * which this file is testing. The collaborator that matters is passed in explicitly, and GetWorld()
     * already resolves through the owning actor, which is all the evidence step needs.
     */
    URitualPlacementComponent* MakePlacementIn(UWorld* World)
    {
        AActor* Owner = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
        return Owner ? NewObject<URitualPlacementComponent>(Owner) : nullptr;
    }

    /**
     * An actor that NAMES ITSELF as the survey subject — the only way that place-name ever resolves.
     *
     * AVeilHeart is used purely as a stand-in for "some actor the map placed": the registry resolves a
     * component registration by the CLAIM, never by class, so the class is irrelevant to what is being
     * tested. It is chosen over a bare AActor only because it brings a root component, and therefore a
     * real transform rather than the identity a component-less actor would report.
     */
    AActor* MakeSubjectActorIn(UWorld* World, const FVector& Location)
    {
        AActor* Actor = World->SpawnActor<AVeilHeart>(Location, FRotator::ZeroRotator);
        if (!Actor)
        {
            return nullptr;
        }
        UGloamsteadSurveySubjectComponent* Claim = NewObject<UGloamsteadSurveySubjectComponent>(Actor);
        Claim->SubjectId = SubjectId;
        // Registered explicitly rather than via BeginPlay, so the test does not depend on when the
        // engine chooses to run a component's BeginPlay.
        return Claim->RegisterWithRegistry() ? Actor : nullptr;
    }

    // Distinctively named (not MakeStates/MakePayload) because RestorationLifecycleTests.cpp declares
    // near-identical helpers, and both files can land in one unity translation unit.
    TArray<FRitualPointState> MakeEvidenceStates()
    {
        TArray<FRitualPointState> States;
        States.SetNum(3);
        States[0].LightLevel = 0.10f; States[0].CorruptionLevel = 0.60f;
        States[1].LightLevel = 0.20f; States[1].CorruptionLevel = 0.50f;
        States[2].LightLevel = 0.30f; States[2].CorruptionLevel = 0.40f;
        return States;
    }

    UGloamsteadPCGSubsystem* MakeSeededPCG()
    {
        UGloamsteadPCGSubsystem* Sub = NewObject<UGloamsteadPCGSubsystem>();
        Sub->Test_SeedPointStates(MakeEvidenceStates());
        return Sub;
    }

    /**
     * Stands in for whatever SpawnRestoredActor would have produced. A bare AActor is enough: the rule
     * under test only asks whether an actor EXISTS, never what it is.
     */
    AActor* MakeRestoredActorIn(UWorld* World)
    {
        return World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
    }

    /**
     * RestoredActor is an explicit parameter, never defaulted, because null is the whole subject of
     * Gloamstead.ConfirmationEvidence.RestorationWithNoActorIsMarked — a default would let a test opt
     * into the degraded path by accident and quietly change what it proves.
     */
    FRestorationEventPayload MakeEvidencePayload(int32 PointIndex, AActor* RestoredActor)
    {
        FRestorationEventPayload P;
        P.PointIndex = PointIndex;
        P.RitualType = ERitualType::LanternPost;
        P.LightDelta = 0.20f;
        P.CorruptionCleared = 0.10f;
        P.RestoredActor = RestoredActor;
        return P;
    }

    /** Unique per run, so a leftover artifact from an earlier run can never masquerade as a collision. */
    FString MakeRequestId(const TCHAR* Tag)
    {
        return FString::Printf(TEXT("gloamtest-confirm-evidence-%s-%s"),
            Tag, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    }

    FString ArtifactPathFor(const FString& RequestId)
    {
        return GloamsteadSurveySubjectReport::RequestPath(
            GloamsteadSurveySubjectReport::DefaultReportDir(), RequestId);
    }

    void DeleteArtifactFor(const FString& RequestId)
    {
        const FString Path = ArtifactPathFor(RequestId);
        if (!Path.IsEmpty())
        {
            IFileManager::Get().Delete(*Path, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
        }
    }

    /** A valid, honestly-unresolved record — enough to occupy a request id on disk. */
    FGloamsteadSurveyRequest MakeStandInRecord(const FString& RequestId)
    {
        FGloamsteadSurveyRequest R;
        R.RequestId = RequestId;
        R.SubjectId = SubjectId;
        R.SchemaVersion = GSSRequestSchemaVersion();
        R.ProducerVersion = GSSProducerVersion();
        R.CreatedAt = TEXT("2020-01-01T00:00:00Z");
        R.MapName = TEXT("GloamTest_SomeOtherMap");
        R.MapPackageName = TEXT("/Game/Maps/GloamTest_SomeOtherMap");
        R.WorldInstanceId = TEXT("/Game/Maps/GloamTest_SomeOtherMap.GloamTest_SomeOtherMap");
        R.Status = EGSSRequestStatus::Unresolved;
        R.FailureCodes.Add(TEXT("GSS001"));
        return R;
    }

    // Error substrings the publisher emits on the refusal paths. An unexpected Error fails an automation
    // test, so a test that deliberately trips one has to declare it first.
    static const TCHAR* const CollisionLog     = TEXT("[GSS012] Survey request id");
    static const TCHAR* const NotPublishedLog  = TEXT("was NOT published");
    static const TCHAR* const StaysRestoredLog = TEXT("STAYS restored");
    static const TCHAR* const MissingActorLog  = TEXT("[GSS016] Point");
}

// ===== Failure class: a report that cannot be written drags the restoration down with it =====
// Reporting is downstream of gameplay. The point was mended; a publish that is refused must leave the
// mending, the light and the restored set exactly as the restoration left them, and must not damage the
// artifact that was already filed under the colliding id either.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamEvidenceFailureDoesNotRollBackTest,
    "Gloamstead.ConfirmationEvidence.ReportFailureDoesNotRollBackRestoration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEvidenceFailureDoesNotRollBackTest::RunTest(const FString& /*Parameters*/)
{
    using namespace GloamConfirmationEvidence;

    FScopedWorld Scope;
    if (!TestNotNull(TEXT("live world created"), Scope.World))
    {
        return false;
    }

    URitualPlacementComponent* Placement = MakePlacementIn(Scope.World);
    if (!TestNotNull(TEXT("placement component created in the world"), Placement))
    {
        return false;
    }

    // Occupy the request id with a DIFFERENT record, so the publish below is refused as a collision.
    // This is the cleanest way to force a real publish failure without breaking the filesystem.
    const FString RequestId = MakeRequestId(TEXT("norollback"));
    const FGloamsteadSurveyRequest StandIn = MakeStandInRecord(RequestId);
    FString StandInPath;
    TArray<FString> StandInCodes;
    if (!TestTrue(TEXT("a stand-in artifact occupies the request id"),
            GloamsteadSurveySubjectReport::WriteRequest(
                StandIn, GloamsteadSurveySubjectReport::DefaultReportDir(), StandInPath, StandInCodes)))
    {
        return false;
    }
    FString StandInBytes;
    TestTrue(TEXT("the stand-in artifact is readable"), FFileHelper::LoadFileToString(StandInBytes, *StandInPath));

    AddExpectedErrorPlain(CollisionLog, EAutomationExpectedErrorFlags::Contains, 0);
    AddExpectedErrorPlain(NotPublishedLog, EAutomationExpectedErrorFlags::Contains, 0);
    AddExpectedErrorPlain(StaysRestoredLog, EAutomationExpectedErrorFlags::Contains, 0);

    // A real restored actor, so these tests exercise the CLEAN path and stay on their own failure class.
    // The null case has its own test below.
    AActor* RestoredActor = MakeRestoredActorIn(Scope.World);
    TestNotNull(TEXT("a restored actor stands in for the Blueprint spawn"), RestoredActor);

    UGloamsteadPCGSubsystem* PCG = MakeSeededPCG();
    const bool bRestored = Placement->Test_CommitRestorationWithEvidence(PCG, 0, MakeEvidencePayload(0, RestoredActor), RequestId);

    // The whole point: the restoration answers true even though the report did not survive.
    TestTrue(TEXT("the restoration succeeded and says so, despite the failed report"), bRestored);
    TestTrue(TEXT("the point is still flagged restored"), PCG->IsPointRestored(0));
    TestEqual(TEXT("the light the restoration added is still there"), PCG->GetLightLevel(0), 0.30f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("the corruption it cleared is still cleared"), PCG->GetCorruptionLevel(0), 0.50f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("the restored set still counts it"), PCG->GetRestoredPointCount(), 1);

    // ...and the failure is surfaced rather than swallowed.
    TestFalse(TEXT("the evidence is reported as unpublished"), Placement->WasLastEvidencePublished());
    TestTrue(TEXT("the collision code is surfaced to the HUD"),
        Placement->GetLastEvidenceFailureCodes().Contains(TEXT("GSS012")));
    TestEqual(TEXT("the failed attempt still names its request"), Placement->GetLastEvidenceRequestId(), RequestId);
    TestEqual(TEXT("the failed attempt still names its point"), Placement->GetLastEvidencePointIndex(), 0);

    // Refusing to publish means refusing to overwrite: the incumbent artifact is untouched.
    FString AfterBytes;
    TestTrue(TEXT("the stand-in artifact is still there"), FFileHelper::LoadFileToString(AfterBytes, *StandInPath));
    TestEqual(TEXT("the stand-in artifact was not overwritten"), AfterBytes, StandInBytes);

    DeleteArtifactFor(RequestId);
    return true;
}

// ===== Failure class: confirming twice files a second success report for one restoration =====
// The second confirmation restores nothing (the point is already mended), so it must publish nothing.
// A second artifact would double-count one lantern in every downstream audit.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamEvidenceNoSecondReportTest,
    "Gloamstead.ConfirmationEvidence.RepeatedConfirmationEmitsNoSecondReport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEvidenceNoSecondReportTest::RunTest(const FString& /*Parameters*/)
{
    using namespace GloamConfirmationEvidence;

    FScopedWorld Scope;
    if (!TestNotNull(TEXT("live world created"), Scope.World))
    {
        return false;
    }
    URitualPlacementComponent* Placement = MakePlacementIn(Scope.World);
    if (!TestNotNull(TEXT("placement component created in the world"), Placement))
    {
        return false;
    }
    TestNotNull(TEXT("an actor claims the lantern place-name"), MakeSubjectActorIn(Scope.World, FVector(500.f, 0.f, 0.f)));

    // A real restored actor, so these tests exercise the CLEAN path and stay on their own failure class.
    // The null case has its own test below.
    AActor* RestoredActor = MakeRestoredActorIn(Scope.World);
    TestNotNull(TEXT("a restored actor stands in for the Blueprint spawn"), RestoredActor);

    UGloamsteadPCGSubsystem* PCG = MakeSeededPCG();

    // --- First confirmation: restores, and files exactly one artifact. ---
    const FString FirstId = MakeRequestId(TEXT("first"));
    TestTrue(TEXT("the first confirmation restores the point"),
        Placement->Test_CommitRestorationWithEvidence(PCG, 0, MakeEvidencePayload(0, RestoredActor), FirstId));
    TestTrue(TEXT("the first confirmation published its evidence"), Placement->WasLastEvidencePublished());
    TestTrue(TEXT("the first artifact is on disk"), FPaths::FileExists(ArtifactPathFor(FirstId)));

    const float LightAfterFirst = PCG->GetLightLevel(0);

    // --- Second confirmation of the same point, with its own fresh request id. ---
    const FString SecondId = MakeRequestId(TEXT("second"));
    TestFalse(TEXT("the second confirmation restores nothing"),
        Placement->Test_CommitRestorationWithEvidence(PCG, 0, MakeEvidencePayload(0, RestoredActor), SecondId));

    // The assertion this test exists for: no artifact was filed for the second attempt. A fresh request
    // id would NOT have collided with the first, so nothing but the ordering (publish only after a true
    // ApplyRestoration) is stopping a second success report here.
    TestFalse(TEXT("no second artifact was filed"), FPaths::FileExists(ArtifactPathFor(SecondId)));

    // The diagnostics still describe the confirmation that actually happened, not the refused one.
    TestEqual(TEXT("the HUD still shows the first request id"), Placement->GetLastEvidenceRequestId(), FirstId);
    TestTrue(TEXT("the HUD still shows a published report"), Placement->WasLastEvidencePublished());
    TestEqual(TEXT("the HUD still shows the restored point"), Placement->GetLastEvidencePointIndex(), 0);

    // ...and gameplay did not take a second helping either.
    TestEqual(TEXT("no second helping of light"), PCG->GetLightLevel(0), LightAfterFirst, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("the point is counted once"), PCG->GetRestoredPointCount(), 1);

    DeleteArtifactFor(FirstId);
    DeleteArtifactFor(SecondId);
    return true;
}

// ===== Failure class: the request id changes somewhere between minting and the artifact =====
// One confirmation, one id. If publication or the HUD invented its own, the artifact on disk could not
// be tied back to the restoration that produced it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamEvidenceRequestIdIsStableTest,
    "Gloamstead.ConfirmationEvidence.RequestIdIsStableAcrossTheConfirmPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEvidenceRequestIdIsStableTest::RunTest(const FString& /*Parameters*/)
{
    using namespace GloamConfirmationEvidence;

    FScopedWorld Scope;
    if (!TestNotNull(TEXT("live world created"), Scope.World))
    {
        return false;
    }
    URitualPlacementComponent* Placement = MakePlacementIn(Scope.World);
    if (!TestNotNull(TEXT("placement component created in the world"), Placement))
    {
        return false;
    }

    const FVector LanternLocation(1200.f, -340.f, 55.f);
    AActor* Lantern = MakeSubjectActorIn(Scope.World, LanternLocation);
    if (!TestNotNull(TEXT("an actor claims the lantern place-name"), Lantern))
    {
        return false;
    }

    // A real restored actor, so these tests exercise the CLEAN path and stay on their own failure class.
    // The null case has its own test below.
    AActor* RestoredActor = MakeRestoredActorIn(Scope.World);
    TestNotNull(TEXT("a restored actor stands in for the Blueprint spawn"), RestoredActor);

    UGloamsteadPCGSubsystem* PCG = MakeSeededPCG();
    const FString RequestId = MakeRequestId(TEXT("stableid"));

    TestTrue(TEXT("the confirmation restores point 1"),
        Placement->Test_CommitRestorationWithEvidence(PCG, 1, MakeEvidencePayload(1, RestoredActor), RequestId));
    TestTrue(TEXT("the evidence was published"), Placement->WasLastEvidencePublished());

    // 1. The id survives into the diagnostics unchanged — not re-minted, not decorated.
    TestEqual(TEXT("the HUD reports the id the confirmation minted"), Placement->GetLastEvidenceRequestId(), RequestId);

    // 2. The id decides where the artifact lives.
    const FString ExpectedPath = ArtifactPathFor(RequestId);
    TestEqual(TEXT("the report path is the path that id files under"),
        Placement->GetLastEvidenceReportPath(), ExpectedPath);
    TestTrue(TEXT("the artifact is on disk"), FPaths::FileExists(ExpectedPath));

    // 3. The id is inside the artifact, and the artifact describes the subject actor that was live.
    FString Json;
    if (TestTrue(TEXT("the artifact is readable"), FFileHelper::LoadFileToString(Json, *ExpectedPath)))
    {
        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (TestTrue(TEXT("the artifact is valid JSON"),
                FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
        {
            FString Field;
            TestTrue(TEXT("request_id present"), Root->TryGetStringField(TEXT("request_id"), Field));
            TestEqual(TEXT("the artifact is filed under the minted id"), Field, RequestId);

            TestTrue(TEXT("subject_id present"), Root->TryGetStringField(TEXT("subject_id"), Field));
            TestEqual(TEXT("the artifact names the subject the component surveys"), Field, FString(SubjectId));

            TestTrue(TEXT("status present"), Root->TryGetStringField(TEXT("status"), Field));
            TestEqual(TEXT("the subject resolved"), Field, GSSRequestStatusToken(EGSSRequestStatus::Resolved));

            // Actor identity must be the actor that was actually claiming the place-name, not a guess.
            TestTrue(TEXT("actor_object_path present"), Root->TryGetStringField(TEXT("actor_object_path"), Field));
            TestEqual(TEXT("the artifact names the real subject actor"), Field, Lantern->GetPathName());

            // ...and the coordinates are that same actor's, read live, not a remembered copy.
            const TSharedPtr<FJsonObject>* TransformObj = nullptr;
            if (TestTrue(TEXT("transform present"), Root->TryGetObjectField(TEXT("transform"), TransformObj)))
            {
                const TSharedPtr<FJsonObject>* LocationObj = nullptr;
                if (TestTrue(TEXT("transform.location present"),
                        (*TransformObj)->TryGetObjectField(TEXT("location"), LocationObj)))
                {
                    const FVector Actual = Lantern->GetActorTransform().GetLocation();
                    TestEqual(TEXT("artifact x is the subject actor's x"),
                        (*LocationObj)->GetNumberField(TEXT("x")), static_cast<double>(Actual.X), 0.01);
                    TestEqual(TEXT("artifact y is the subject actor's y"),
                        (*LocationObj)->GetNumberField(TEXT("y")), static_cast<double>(Actual.Y), 0.01);
                    TestEqual(TEXT("artifact z is the subject actor's z"),
                        (*LocationObj)->GetNumberField(TEXT("z")), static_cast<double>(Actual.Z), 0.01);
                    TestTrue(TEXT("the subject actor really is where the test placed it"),
                        Actual.Equals(LanternLocation, 0.01f));
                }
            }

            // Self-describing evidence: it says what wrote it and against which world.
            TestTrue(TEXT("schema present"), Root->TryGetStringField(TEXT("schema"), Field));
            TestEqual(TEXT("schema is the request schema"), Field, GSSRequestSchemaVersion());
            TestTrue(TEXT("producer_version present"), Root->TryGetStringField(TEXT("producer_version"), Field));
            TestEqual(TEXT("producer is this producer"), Field, GSSProducerVersion());
            TestTrue(TEXT("created_at present"), Root->TryGetStringField(TEXT("created_at"), Field));
            TestFalse(TEXT("created_at is stamped, not blank"), Field.IsEmpty());
            TestTrue(TEXT("map_name present"), Root->TryGetStringField(TEXT("map_name"), Field));
            TestFalse(TEXT("the artifact names the map it ran against"), Field.IsEmpty());
            TestTrue(TEXT("world_instance_id present"), Root->TryGetStringField(TEXT("world_instance_id"), Field));
            TestEqual(TEXT("the artifact names the exact world surveyed"), Field, Scope.World->GetPathName());
        }
    }

    // And the point index the HUD correlates on is the point the restoration mutated.
    TestEqual(TEXT("the HUD correlates the evidence with point 1"), Placement->GetLastEvidencePointIndex(), 1);
    TestTrue(TEXT("point 1 is the point that was restored"), PCG->IsPointRestored(1));
    TestFalse(TEXT("no other point was touched"), PCG->IsPointRestored(0));

    DeleteArtifactFor(RequestId);
    return true;
}

// ===== Failure class: a retry files a second artifact instead of re-publishing the same one =====
// The publisher's byte comparison is what makes the component's retry rule safe: re-emitting the SAME
// record is a no-op that reports success, while a REBUILT record under the same id is refused outright.
// If that ever softened into an overwrite, a bounded retry would silently rewrite evidence.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamEvidenceRetryIsIdempotentTest,
    "Gloamstead.ConfirmationEvidence.RetryRepublishesTheSameRecordOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEvidenceRetryIsIdempotentTest::RunTest(const FString& /*Parameters*/)
{
    using namespace GloamConfirmationEvidence;

    const FString RequestId = MakeRequestId(TEXT("retry"));
    const FString Dir = GloamsteadSurveySubjectReport::DefaultReportDir();
    const FGloamsteadSurveyRequest Record = MakeStandInRecord(RequestId);

    FString FirstPath;
    TArray<FString> FirstCodes;
    TestTrue(TEXT("the first publish lands"),
        GloamsteadSurveySubjectReport::WriteRequest(Record, Dir, FirstPath, FirstCodes));
    TestEqual(TEXT("a clean publish reports no codes"), FirstCodes.Num(), 0);

    FString FirstBytes;
    TestTrue(TEXT("the artifact is readable"), FFileHelper::LoadFileToString(FirstBytes, *FirstPath));

    // The retry the component actually performs: the SAME record, a second time.
    FString RetryPath;
    TArray<FString> RetryCodes;
    TestTrue(TEXT("re-publishing the identical record succeeds"),
        GloamsteadSurveySubjectReport::WriteRequest(Record, Dir, RetryPath, RetryCodes));
    TestEqual(TEXT("the retry files under the same path"), RetryPath, FirstPath);
    TestEqual(TEXT("the retry reports no codes"), RetryCodes.Num(), 0);

    FString RetryBytes;
    TestTrue(TEXT("the artifact is still readable"), FFileHelper::LoadFileToString(RetryBytes, *FirstPath));
    TestEqual(TEXT("the retry changed nothing on disk"), RetryBytes, FirstBytes);

    // The retry the component deliberately does NOT perform: rebuilding the record between attempts.
    // A rebuild re-stamps created_at, so the bytes differ and the id is defended rather than overwritten.
    AddExpectedErrorPlain(CollisionLog, EAutomationExpectedErrorFlags::Contains, 0);
    FGloamsteadSurveyRequest Rebuilt = Record;
    Rebuilt.CreatedAt = TEXT("2021-06-06T06:06:06Z");
    FString RebuiltPath;
    TArray<FString> RebuiltCodes;
    TestFalse(TEXT("a rebuilt record under the same id is refused"),
        GloamsteadSurveySubjectReport::WriteRequest(Rebuilt, Dir, RebuiltPath, RebuiltCodes));
    TestTrue(TEXT("the refusal is reported as a request-id collision"), RebuiltCodes.Contains(TEXT("GSS012")));

    FString AfterBytes;
    TestTrue(TEXT("the original artifact is still readable"), FFileHelper::LoadFileToString(AfterBytes, *FirstPath));
    TestEqual(TEXT("the refused rebuild wrote nothing"), AfterBytes, FirstBytes);

    DeleteArtifactFor(RequestId);
    return true;
}

// ===== Failure class: evidence is published under an id the confirmation never minted =====
// The id has to exist before the subject is resolved. If the publisher accepted an empty one, the
// registry would mint its own inside BuildRequest and file the artifact under an id that no log line and
// no getter from this confirmation ever saw — unattributable evidence. Refuse, loudly, and keep the
// restoration.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamEvidenceRequiresAMintedIdTest,
    "Gloamstead.ConfirmationEvidence.EvidenceRequiresARequestIdMintedUpFront",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEvidenceRequiresAMintedIdTest::RunTest(const FString& /*Parameters*/)
{
    using namespace GloamConfirmationEvidence;

    FScopedWorld Scope;
    if (!TestNotNull(TEXT("live world created"), Scope.World))
    {
        return false;
    }
    URitualPlacementComponent* Placement = MakePlacementIn(Scope.World);
    if (!TestNotNull(TEXT("placement component created in the world"), Placement))
    {
        return false;
    }
    TestNotNull(TEXT("an actor claims the lantern place-name"), MakeSubjectActorIn(Scope.World, FVector(800.f, 0.f, 0.f)));

    AddExpectedErrorPlain(TEXT("no request id was minted"), EAutomationExpectedErrorFlags::Contains, 0);

    // A real restored actor, so these tests exercise the CLEAN path and stay on their own failure class.
    // The null case has its own test below.
    AActor* RestoredActor = MakeRestoredActorIn(Scope.World);
    TestNotNull(TEXT("a restored actor stands in for the Blueprint spawn"), RestoredActor);

    UGloamsteadPCGSubsystem* PCG = MakeSeededPCG();
    const bool bRestored = Placement->Test_CommitRestorationWithEvidence(PCG, 2, MakeEvidencePayload(2, RestoredActor), FString());

    // Restoration is authoritative even when the evidence contract was violated.
    TestTrue(TEXT("the restoration still succeeded"), bRestored);
    TestTrue(TEXT("point 2 is restored"), PCG->IsPointRestored(2));
    TestEqual(TEXT("point 2 kept the light it was given"), PCG->GetLightLevel(2), 0.50f, KINDA_SMALL_NUMBER);

    // Nothing was published, and the HUD is told why rather than being shown a stale success.
    TestFalse(TEXT("no evidence was published"), Placement->WasLastEvidencePublished());
    TestTrue(TEXT("the report path is empty"), Placement->GetLastEvidenceReportPath().IsEmpty());
    TestTrue(TEXT("the identity failure is surfaced"),
        Placement->GetLastEvidenceFailureCodes().Contains(TEXT("GSS015")));
    TestEqual(TEXT("the diagnostics still name the restored point"), Placement->GetLastEvidencePointIndex(), 2);
    return true;
}

// ===== Failure class: a configured class is missing or fails to materialise an actor =====
// The point is spent, ApplyRestoration will refuse it for the rest of the session, and the player sees
// nothing there. The rule is proceed-and-mark: restoration is NOT refused and the artifact is NOT clean.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamEvidenceMissingActorIsMarkedTest,
    "Gloamstead.ConfirmationEvidence.RestorationWithNoActorIsMarked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEvidenceMissingActorIsMarkedTest::RunTest(const FString& /*Parameters*/)
{
    using namespace GloamConfirmationEvidence;

    FScopedWorld Scope;
    if (!TestNotNull(TEXT("live world created"), Scope.World))
    {
        return false;
    }
    URitualPlacementComponent* Placement = MakePlacementIn(Scope.World);
    if (!TestNotNull(TEXT("placement component created in the world"), Placement))
    {
        return false;
    }
    // The subject resolves cleanly, so GSS016 is the ONLY code the artifact can be carrying.
    TestNotNull(TEXT("an actor claims the lantern place-name"), MakeSubjectActorIn(Scope.World, FVector(900.f, 0.f, 0.f)));

    AddExpectedErrorPlain(MissingActorLog, EAutomationExpectedErrorFlags::Contains, 0);

    UGloamsteadPCGSubsystem* PCG = MakeSeededPCG();
    const FString RequestId = MakeRequestId(TEXT("noactor"));

    // The defect's exact shape: a confirmation whose spawn produced nothing.
    const bool bRestored =
        Placement->Test_CommitRestorationWithEvidence(PCG, 0, MakeEvidencePayload(0, nullptr), RequestId);

    // --- The rule is PROCEED, not refuse. A false return would contradict the authoritative point state.
    TestTrue(TEXT("the restoration proceeds rather than being refused"), bRestored);
    TestTrue(TEXT("the point is restored"), PCG->IsPointRestored(0));
    TestEqual(TEXT("the light was applied"), PCG->GetLightLevel(0), 0.30f, KINDA_SMALL_NUMBER);

    // ...which is precisely why silence would be unrecoverable: the point is now spent for good.
    TestFalse(TEXT("the spent point can never be restored again"),
        PCG->ApplyRestoration(0, MakeEvidencePayload(0, nullptr)));

    // --- And the confirmation is marked, not silent.
    TestTrue(TEXT("the missing actor is surfaced as its own answer"), Placement->WasLastRestoredActorMissing());
    TestTrue(TEXT("the degraded code reaches the HUD"),
        Placement->GetLastEvidenceFailureCodes().Contains(URitualPlacementComponent::GSSRestoredActorMissing));
    TestEqual(TEXT("the documented code is GSS016"),
        URitualPlacementComponent::GSSRestoredActorMissing, FString(TEXT("GSS016")));

    // The artifact must still be WRITTEN — withholding it would destroy the record of the degradation,
    // which is the opposite of what a marked failure is for.
    TestTrue(TEXT("the artifact is still published"), Placement->WasLastEvidencePublished());

    const FString Path = ArtifactPathFor(RequestId);
    FString Json;
    if (TestTrue(TEXT("the artifact is readable"), FFileHelper::LoadFileToString(Json, *Path)))
    {
        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (TestTrue(TEXT("the artifact is valid JSON"),
                FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
        {
            const TArray<TSharedPtr<FJsonValue>>* Codes = nullptr;
            if (TestTrue(TEXT("failure_codes present"), Root->TryGetArrayField(TEXT("failure_codes"), Codes))
                && TestEqual(TEXT("exactly one code — the degradation, nothing else"), Codes->Num(), 1))
            {
                TestEqual(TEXT("a reader can detect the missing actor from the artifact alone"),
                    (*Codes)[0]->AsString(), URitualPlacementComponent::GSSRestoredActorMissing);
            }

            // The subject resolution is untouched: the place-name really did resolve, and saying
            // otherwise to signal a spawn failure would corrupt the field the registry exists to answer.
            FString Status;
            TestTrue(TEXT("status present"), Root->TryGetStringField(TEXT("status"), Status));
            TestEqual(TEXT("the survey itself still reports resolved"),
                Status, GSSRequestStatusToken(EGSSRequestStatus::Resolved));
        }
    }

    DeleteArtifactFor(RequestId);
    return true;
}

// ===== Not covered here, and why =====
//
// * "ConfirmPlacement end to end, from placement mode to a filed artifact" and single-spawn retry
//   behavior are covered by Gloamstead.FirstNight.PlayableSlice.LanternConfirmationMaterializesOnce.
//
// * "the Blueprint notifications fire after the evidence is published" — OnPlacementConfirmed and
//   OnRestoredActorSpawned are BlueprintImplementableEvents; observing their ordering requires a
//   Blueprint child to observe from. The ordering is enforced at RitualPlacementComponent.cpp:135-141.
//
// * "a transient filesystem failure is retried and then succeeds" — the retry loop
//   (RitualPlacementComponent.cpp, PublishRestorationEvidence) re-invokes the publisher with the same
//   record; making the FIRST attempt fail and the second succeed needs the filesystem to change between
//   them, which is not something an automation test may arrange safely. The property that makes the
//   retry safe either way — same record in, same artifact out — is covered by
//   Gloamstead.ConfirmationEvidence.RetryRepublishesTheSameRecordOnly.

#endif // WITH_DEV_AUTOMATION_TESTS
