#include "Data/GloamsteadSurveySubjectTypes.h"
#include "Systems/GloamsteadForgeEvidence.h" // reuse ReadGitCommit for provenance
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"

// ===== Tokens =====

FString GSSResolverKindToken(EGSSResolverKind Kind)
{
	switch (Kind)
	{
	case EGSSResolverKind::PlacedActorClass: return TEXT("placed_actor_class");
	case EGSSResolverKind::None:             return TEXT("none");
	default:                                 return TEXT("none");
	}
}

FString GSSAnchorModeToken(EGSSAnchorMode Mode)
{
	switch (Mode)
	{
	case EGSSAnchorMode::ActorObjectPath:   return TEXT("actor_object_path");
	case EGSSAnchorMode::ExplicitTransform: return TEXT("explicit_transform");
	case EGSSAnchorMode::None:              return TEXT("none");
	default:                                return TEXT("none");
	}
}

// ===== Validation =====

TArray<FString> GSSValidateSubject(const FGloamsteadSurveySubject& S)
{
	TArray<FString> Codes;

	if (S.SubjectId.IsNone() || S.SubjectId.ToString().IsEmpty())
	{
		Codes.Add(TEXT("GSS002"));
	}

	// Ambiguity is not resolution. A subject picked out of a set is an inferred subject.
	if (S.CandidateCount > 1)
	{
		Codes.AddUnique(TEXT("GSS007"));
		if (S.bLocationResolved) { Codes.AddUnique(TEXT("GSS007")); }
	}

	if (!S.bLocationResolved)
	{
		Codes.AddUnique(TEXT("GSS001"));

		// The never-guess rail: an unresolved subject must carry no location whatsoever.
		if (!S.Transform.Equals(FTransform::Identity)) { Codes.AddUnique(TEXT("GSS004")); }
		if (!S.ActorObjectPath.IsEmpty())              { Codes.AddUnique(TEXT("GSS004")); }
		if (S.AnchorMode != EGSSAnchorMode::None)      { Codes.AddUnique(TEXT("GSS005")); }
	}
	else
	{
		// Resolution has to be backed by evidence a reader can independently check.
		if (S.ResolverKind == EGSSResolverKind::None) { Codes.AddUnique(TEXT("GSS006")); }
		if (S.AnchorMode == EGSSAnchorMode::None)     { Codes.AddUnique(TEXT("GSS005")); }
		if (S.AnchorMode == EGSSAnchorMode::ActorObjectPath && S.ActorObjectPath.IsEmpty())
		{
			Codes.AddUnique(TEXT("GSS003"));
		}
		if (S.ResolvedClassName.IsEmpty()) { Codes.AddUnique(TEXT("GSS003")); }
	}

	return Codes;
}

TArray<FString> GSSValidateReport(const FGloamsteadSurveySubjectReport& R)
{
	TArray<FString> Codes;

	if (R.DeclaredCount <= 0) { Codes.Add(TEXT("GSS010")); }

	// Counts must reconcile with the list they claim to summarise.
	int32 Resolved = 0;
	for (const FGloamsteadSurveySubject& S : R.Subjects)
	{
		if (S.bLocationResolved) { ++Resolved; }
	}
	const int32 Unresolved = R.Subjects.Num() - Resolved;
	if (R.ResolvedCount != Resolved)                       { Codes.AddUnique(TEXT("GSS011")); }
	if (R.UnresolvedCount != Unresolved)                   { Codes.AddUnique(TEXT("GSS011")); }
	if (R.DeclaredCount != R.Subjects.Num())               { Codes.AddUnique(TEXT("GSS011")); }

	// Per-subject dishonesty rolls up. Note GSS001 does NOT invalidate a report: an honestly
	// reported unresolved subject is a correct outcome, and the report must be able to say so.
	for (const FGloamsteadSurveySubject& S : R.Subjects)
	{
		for (const FString& C : GSSValidateSubject(S))
		{
			if (C != TEXT("GSS001")) { Codes.AddUnique(C); }
		}
	}

	return Codes;
}

// ===== JSON report =====

// Named, not anonymous: these helper names (R4/VectorJson/WriteJson) are also used by
// GloamsteadMeshForgeTypes.cpp, and an anonymous namespace does NOT keep them apart once both files
// land in the same unity translation unit -- it collides with C2264. Local builds can hide this,
// because UBT's adaptive non-unity build excludes files that `git status` reports as in the working
// set, so uncommitted files compile standalone and only collide after they are committed.
namespace GloamsteadSurveySubjectJson
{
	double R4(double V) { return FMath::RoundToDouble(V * 10000.0) / 10000.0; }

	TSharedPtr<FJsonObject> VectorJson(const FVector& V)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), R4(V.X));
		O->SetNumberField(TEXT("y"), R4(V.Y));
		O->SetNumberField(TEXT("z"), R4(V.Z));
		return O;
	}

	TSharedPtr<FJsonObject> TransformJson(const FTransform& T)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetObjectField(TEXT("location"), VectorJson(T.GetLocation()));
		const FRotator Rot = T.Rotator();
		TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
		R->SetNumberField(TEXT("pitch"), R4(Rot.Pitch));
		R->SetNumberField(TEXT("yaw"),   R4(Rot.Yaw));
		R->SetNumberField(TEXT("roll"),  R4(Rot.Roll));
		O->SetObjectField(TEXT("rotation"), R);
		O->SetObjectField(TEXT("scale"), VectorJson(T.GetScale3D()));
		return O;
	}

	TSharedPtr<FJsonObject> SubjectJson(const FGloamsteadSurveySubject& S)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("subject_id"), S.SubjectId.ToString());
		O->SetStringField(TEXT("resolver_kind"), GSSResolverKindToken(S.ResolverKind));
		O->SetStringField(TEXT("anchor_mode"), GSSAnchorModeToken(S.AnchorMode));
		O->SetBoolField(TEXT("location_resolved"), S.bLocationResolved);
		O->SetNumberField(TEXT("candidate_count"), S.CandidateCount);

		// Null rather than empty-string when there is nothing to report: a reader must not be able to
		// mistake "no anchor" for "an anchor at the origin".
		if (S.ActorObjectPath.IsEmpty()) { O->SetField(TEXT("actor_object_path"), MakeShared<FJsonValueNull>()); }
		else { O->SetStringField(TEXT("actor_object_path"), S.ActorObjectPath); }
		if (S.ResolvedClassName.IsEmpty()) { O->SetField(TEXT("resolved_class"), MakeShared<FJsonValueNull>()); }
		else { O->SetStringField(TEXT("resolved_class"), S.ResolvedClassName); }
		if (S.bLocationResolved) { O->SetObjectField(TEXT("transform"), TransformJson(S.Transform)); }
		else { O->SetField(TEXT("transform"), MakeShared<FJsonValueNull>()); }

		TArray<TSharedPtr<FJsonValue>> Fc;
		for (const FString& C : S.FailureCodes) { Fc.Add(MakeShared<FJsonValueString>(C)); }
		O->SetArrayField(TEXT("failure_codes"), Fc);
		return O;
	}

	bool WriteJson(const TSharedPtr<FJsonObject>& Root, const FString& Path)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return FFileHelper::SaveStringToFile(Out, *Path);
	}
}

FString GloamsteadSurveySubjectReport::DefaultReportDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("procedural"), TEXT("reports"), TEXT("gloamstead_survey_subjects"));
}

bool GloamsteadSurveySubjectReport::WriteReport(const FGloamsteadSurveySubjectReport& Report,
	const FString& OutDir, FString& OutPrimaryPath)
{
	IFileManager::Get().MakeDirectory(*OutDir, /*Tree*/ true);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("GloamsteadSurveySubjectReport/v1"));
	Root->SetStringField(TEXT("report_id"), Report.ReportId);
	Root->SetStringField(TEXT("created_at"), FDateTime::UtcNow().ToIso8601());
	Root->SetStringField(TEXT("git_sha"), GloamsteadForgeEvidence::ReadGitCommit());
	Root->SetStringField(TEXT("map_name"), Report.MapName);
	Root->SetNumberField(TEXT("declared_count"), Report.DeclaredCount);
	Root->SetNumberField(TEXT("resolved_count"), Report.ResolvedCount);
	Root->SetNumberField(TEXT("unresolved_count"), Report.UnresolvedCount);
	{
		TArray<TSharedPtr<FJsonValue>> Fc;
		for (const FString& C : Report.FailureCodes) { Fc.Add(MakeShared<FJsonValueString>(C)); }
		Root->SetArrayField(TEXT("failure_codes"), Fc);
	}
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FGloamsteadSurveySubject& S : Report.Subjects) { Arr.Add(MakeShared<FJsonValueObject>(GloamsteadSurveySubjectJson::SubjectJson(S))); }
		Root->SetArrayField(TEXT("subjects"), Arr);
	}

	OutPrimaryPath = FPaths::Combine(OutDir, TEXT("survey_subject_report.json"));
	return GloamsteadSurveySubjectJson::WriteJson(Root, OutPrimaryPath);
}
