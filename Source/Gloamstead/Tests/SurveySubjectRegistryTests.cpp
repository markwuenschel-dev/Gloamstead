// Gloamstead Survey Subject Registry (Wave G2) — source-level proofs.
//
//  1. Contract (headless): the GSS validators fail closed. An unresolved subject that carries a
//     location, or a resolved subject with no evidence behind it, is rejected.
//  2. LIVE world: a placed actor resolves to its real object path and transform; an undeclared name
//     and an unplaced subject both come back unresolved WITH NO COORDINATES; two candidates is
//     ambiguity and does not resolve. The registry mutates nothing.
//  3. Report: counts reconcile, an honestly-unresolved subject does not invalidate the report, and
//     the JSON artifact is written.
#include "Misc/AutomationTest.h"
#include "Data/GloamsteadSurveySubjectTypes.h"
#include "Systems/GloamsteadSurveySubjectRegistry.h"
#include "Systems/VeilHeart.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGloamsteadSurveySubject MakeResolvedSubject()
	{
		FGloamsteadSurveySubject S;
		S.SubjectId = TEXT("sanctuary.heart");
		S.ResolverKind = EGSSResolverKind::PlacedActorClass;
		S.AnchorMode = EGSSAnchorMode::ActorObjectPath;
		S.ActorObjectPath = TEXT("/Game/Map.Map:PersistentLevel.VeilHeart_0");
		S.ResolvedClassName = TEXT("VeilHeart");
		S.Transform = FTransform(FVector(100.f, 200.f, 300.f));
		S.bLocationResolved = true;
		S.CandidateCount = 1;
		return S;
	}
}

// 1. Validators fail closed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSurveySubjectContractTest,
	"Gloamstead.SurveySubject.ContractsFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSurveySubjectContractTest::RunTest(const FString& /*Parameters*/)
{
	// A clean resolved subject validates.
	const FGloamsteadSurveySubject Clean = MakeResolvedSubject();
	TestEqual(TEXT("clean resolved subject is valid"), GSSValidateSubject(Clean).Num(), 0);

	// The never-guess rail: an unresolved subject must carry no location at all.
	FGloamsteadSurveySubject Ghost = MakeResolvedSubject();
	Ghost.bLocationResolved = false;
	TestTrue(TEXT("unresolved -> GSS001"), GSSValidateSubject(Ghost).Contains(TEXT("GSS001")));
	TestTrue(TEXT("unresolved but carrying a transform/path -> GSS004"), GSSValidateSubject(Ghost).Contains(TEXT("GSS004")));

	FGloamsteadSurveySubject BareUnresolved;
	BareUnresolved.SubjectId = TEXT("sanctuary.heart");
	TestTrue(TEXT("honest unresolved still reports GSS001"), GSSValidateSubject(BareUnresolved).Contains(TEXT("GSS001")));
	TestFalse(TEXT("honest unresolved does NOT trip the guess rail"), GSSValidateSubject(BareUnresolved).Contains(TEXT("GSS004")));

	// Resolution has to be backed by evidence.
	FGloamsteadSurveySubject NoPath = MakeResolvedSubject(); NoPath.ActorObjectPath = FString();
	TestTrue(TEXT("resolved with no object path -> GSS003"), GSSValidateSubject(NoPath).Contains(TEXT("GSS003")));
	FGloamsteadSurveySubject NoClass = MakeResolvedSubject(); NoClass.ResolvedClassName = FString();
	TestTrue(TEXT("resolved with no concrete class -> GSS003"), GSSValidateSubject(NoClass).Contains(TEXT("GSS003")));
	FGloamsteadSurveySubject NoKind = MakeResolvedSubject(); NoKind.ResolverKind = EGSSResolverKind::None;
	TestTrue(TEXT("resolved by no resolver -> GSS006"), GSSValidateSubject(NoKind).Contains(TEXT("GSS006")));
	FGloamsteadSurveySubject NoMode = MakeResolvedSubject(); NoMode.AnchorMode = EGSSAnchorMode::None;
	TestTrue(TEXT("resolved with no anchor mode -> GSS005"), GSSValidateSubject(NoMode).Contains(TEXT("GSS005")));
	FGloamsteadSurveySubject NoId = MakeResolvedSubject(); NoId.SubjectId = NAME_None;
	TestTrue(TEXT("empty subject id -> GSS002"), GSSValidateSubject(NoId).Contains(TEXT("GSS002")));

	// Ambiguity is never resolution.
	FGloamsteadSurveySubject Ambiguous = MakeResolvedSubject(); Ambiguous.CandidateCount = 2;
	TestTrue(TEXT("more than one candidate -> GSS007"), GSSValidateSubject(Ambiguous).Contains(TEXT("GSS007")));

	// Tokens are stable wire values, not display strings.
	TestEqual(TEXT("anchor mode token"), GSSAnchorModeToken(EGSSAnchorMode::ActorObjectPath), FString(TEXT("actor_object_path")));
	TestEqual(TEXT("resolver kind token"), GSSResolverKindToken(EGSSResolverKind::PlacedActorClass), FString(TEXT("placed_actor_class")));
	return true;
}

// 2 + 3. LIVE world resolution and reporting.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSurveySubjectLiveWorldTest,
	"Gloamstead.SurveySubject.ResolvesPlacedActorsAndRefusesToGuess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSurveySubjectLiveWorldTest::RunTest(const FString& /*Parameters*/)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("live world created"), World))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	UGloamsteadSurveySubjectRegistry* Registry = World->GetSubsystem<UGloamsteadSurveySubjectRegistry>();
	if (TestNotNull(TEXT("registry subsystem present"), Registry))
	{
		// The project declares place-names whether or not anything is placed.
		TestTrue(TEXT("subjects are declared"), UGloamsteadSurveySubjectRegistry::GetDeclarations().Num() >= 3);
		TestNotNull(TEXT("sanctuary.heart is declared"), UGloamsteadSurveySubjectRegistry::FindDeclaration(TEXT("sanctuary.heart")));

		// --- Nothing placed yet: unresolved, and crucially carrying NO location. ---
		FGloamsteadSurveySubject Before;
		TestFalse(TEXT("heart does not resolve before it is placed"),
			Registry->Test_ResolveSubjectIn(World, TEXT("sanctuary.heart"), Before));
		TestFalse(TEXT("unplaced subject reports no resolution"), Before.bLocationResolved);
		TestTrue(TEXT("unplaced subject returns no object path"), Before.ActorObjectPath.IsEmpty());
		TestTrue(TEXT("unplaced subject returns identity transform, not a guess"),
			Before.Transform.Equals(FTransform::Identity));
		TestTrue(TEXT("unplaced subject reports GSS001"), Before.FailureCodes.Contains(TEXT("GSS001")));

		// --- An undeclared name is unresolved, never fuzzy-matched. ---
		FGloamsteadSurveySubject Unknown;
		TestFalse(TEXT("an undeclared place-name does not resolve"),
			Registry->Test_ResolveSubjectIn(World, TEXT("courtyard.lantern.first"), Unknown));
		TestTrue(TEXT("undeclared name returns no object path"), Unknown.ActorObjectPath.IsEmpty());
		TestTrue(TEXT("undeclared name returns identity transform"), Unknown.Transform.Equals(FTransform::Identity));

		// --- Place the Heart: now it resolves to the real actor. ---
		const FVector HeartLocation(1234.f, -567.f, 89.f);
		AVeilHeart* Heart = World->SpawnActor<AVeilHeart>(HeartLocation, FRotator::ZeroRotator);
		TestNotNull(TEXT("Heart spawned"), Heart);

		FGloamsteadSurveySubject Resolved;
		const bool bOk = Registry->Test_ResolveSubjectIn(World, TEXT("sanctuary.heart"), Resolved);
		TestTrue(TEXT("heart resolves once placed"), bOk);
		TestTrue(TEXT("resolution is marked resolved"), Resolved.bLocationResolved);
		TestEqual(TEXT("resolution is unambiguous"), Resolved.CandidateCount, 1);
		TestEqual(TEXT("no failure codes on a clean resolution"), GSSValidateSubject(Resolved).Num(), 0);
		TestTrue(TEXT("anchor mode is actor_object_path"), Resolved.AnchorMode == EGSSAnchorMode::ActorObjectPath);
		if (Heart)
		{
			// The evidence must name the actual actor, and the transform must BE the actor's — not a
			// remembered or rounded copy of it.
			TestEqual(TEXT("object path is the real actor's path"), Resolved.ActorObjectPath, Heart->GetPathName());
			TestTrue(TEXT("transform is the actor's own transform"),
				Resolved.Transform.GetLocation().Equals(HeartLocation, 0.01f));
			TestEqual(TEXT("concrete class is reported"), Resolved.ResolvedClassName, Heart->GetClass()->GetName());
		}

		// --- Two candidates: ambiguity must NOT resolve to whichever came first. ---
		AVeilHeart* SecondHeart = World->SpawnActor<AVeilHeart>(FVector(4000.f, 0.f, 0.f), FRotator::ZeroRotator);
		TestNotNull(TEXT("second Heart spawned"), SecondHeart);
		FGloamsteadSurveySubject Ambiguous;
		TestFalse(TEXT("two candidates do not resolve"),
			Registry->Test_ResolveSubjectIn(World, TEXT("sanctuary.heart"), Ambiguous));
		TestTrue(TEXT("ambiguity is reported as GSS007"), Ambiguous.FailureCodes.Contains(TEXT("GSS007")));
		TestTrue(TEXT("ambiguous subject returns no object path"), Ambiguous.ActorObjectPath.IsEmpty());
		TestTrue(TEXT("ambiguous subject returns identity transform"), Ambiguous.Transform.Equals(FTransform::Identity));
		if (SecondHeart)
		{
			SecondHeart->Destroy();
		}

		// --- The report reconciles, and tolerates honestly-unresolved subjects. ---
		const FGloamsteadSurveySubjectReport Report = Registry->Test_BuildReportFor(World);
		TestEqual(TEXT("report declares every declared subject"),
			Report.DeclaredCount, UGloamsteadSurveySubjectRegistry::GetDeclarations().Num());
		TestEqual(TEXT("report lists one entry per declaration"), Report.Subjects.Num(), Report.DeclaredCount);
		TestEqual(TEXT("counts reconcile"), Report.ResolvedCount + Report.UnresolvedCount, Report.DeclaredCount);
		TestTrue(TEXT("the placed Heart is counted as resolved"), Report.ResolvedCount >= 1);
		TestTrue(TEXT("the unplaced subjects are counted as unresolved"), Report.UnresolvedCount >= 1);
		TestEqual(TEXT("an honest report with unresolved subjects is still valid"),
			GSSValidateReport(Report).Num(), 0);
		TestFalse(TEXT("report names the map it ran against"), Report.MapName.IsEmpty());

		// A report whose counts lie about its own list is rejected.
		FGloamsteadSurveySubjectReport Lying = Report; Lying.ResolvedCount += 1;
		TestTrue(TEXT("miscounted report -> GSS011"), GSSValidateReport(Lying).Contains(TEXT("GSS011")));
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// 3b. The JSON artifact is actually written and is non-trivial.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSurveySubjectReportEmitTest,
	"Gloamstead.SurveySubject.ReportArtifactIsWritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSurveySubjectReportEmitTest::RunTest(const FString& /*Parameters*/)
{
	FGloamsteadSurveySubjectReport R;
	R.ReportId = TEXT("test-report");
	R.MapName = TEXT("TestMap");
	R.DeclaredCount = 1;
	R.UnresolvedCount = 1;
	FGloamsteadSurveySubject S;
	S.SubjectId = TEXT("sanctuary.heart");
	S.FailureCodes.Add(TEXT("GSS001"));
	R.Subjects.Add(S);

	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GloamsteadSurveySubjectTests"));
	FString Path;
	TestTrue(TEXT("report written"), GloamsteadSurveySubjectReport::WriteReport(R, Dir, Path));

	FString Json;
	TestTrue(TEXT("report readable"), FFileHelper::LoadFileToString(Json, *Path));

	// Parse rather than substring-match: the property under test is that the field IS json null, not
	// that the serialiser happened to emit it without a space.
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	const bool bParsed = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
	if (!TestTrue(TEXT("report is valid JSON"), bParsed))
	{
		return false;
	}

	FString Schema;
	TestTrue(TEXT("schema is stamped"), Root->TryGetStringField(TEXT("schema"), Schema));
	TestEqual(TEXT("schema value"), Schema, FString(TEXT("GloamsteadSurveySubjectReport/v1")));

	const TArray<TSharedPtr<FJsonValue>>* Subjects = nullptr;
	if (TestTrue(TEXT("subjects array present"), Root->TryGetArrayField(TEXT("subjects"), Subjects))
		&& TestEqual(TEXT("one subject serialised"), Subjects->Num(), 1))
	{
		const TSharedPtr<FJsonObject> S0 = (*Subjects)[0]->AsObject();
		if (TestTrue(TEXT("subject is an object"), S0.IsValid()))
		{
			FString Id;
			TestTrue(TEXT("subject id round-trips"), S0->TryGetStringField(TEXT("subject_id"), Id));
			TestEqual(TEXT("subject id value"), Id, FString(TEXT("sanctuary.heart")));

			// An unresolved subject must serialise as null — never as a zero transform a reader could
			// mistake for "surveyed at the world origin".
			const TSharedPtr<FJsonValue> TransformField = S0->TryGetField(TEXT("transform"));
			TestTrue(TEXT("unresolved transform serialises as json null"),
				TransformField.IsValid() && TransformField->Type == EJson::Null);
			const TSharedPtr<FJsonValue> PathField = S0->TryGetField(TEXT("actor_object_path"));
			TestTrue(TEXT("unresolved path serialises as json null"),
				PathField.IsValid() && PathField->Type == EJson::Null);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
