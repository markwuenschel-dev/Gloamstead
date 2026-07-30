#include "Data/GloamsteadSurveySubjectTypes.h"
#include "Gloamstead.h"                        // LogGloamstead
#include "Systems/GloamsteadForgeEvidence.h"   // reuse ReadGitCommit for provenance
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "Templates/UniquePtr.h"

// ===== Tokens =====

FString GSSResolverKindToken(EGSSResolverKind Kind)
{
	switch (Kind)
	{
	case EGSSResolverKind::PlacedActorClass:    return TEXT("placed_actor_class");
	case EGSSResolverKind::RegisteredComponent: return TEXT("registered_component");
	case EGSSResolverKind::None:                return TEXT("none");
	default:                                    return TEXT("none");
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

FString GSSRequestStatusToken(EGSSRequestStatus Status)
{
	switch (Status)
	{
	case EGSSRequestStatus::Resolved:   return TEXT("resolved");
	case EGSSRequestStatus::Unresolved: return TEXT("unresolved");
	case EGSSRequestStatus::Pending:    return TEXT("pending");
	default:                            return TEXT("pending");
	}
}

// ===== Versioning =====

FString GSSReportSchemaVersion()  { return TEXT("GloamsteadSurveySubjectReport/v2"); }
FString GSSRequestSchemaVersion() { return TEXT("GloamsteadSurveyRequest/v1"); }
FString GSSProducerVersion()      { return TEXT("Gloamstead.SurveySubject/2.0.0"); }

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

TArray<FString> GSSValidateRequest(const FGloamsteadSurveyRequest& R)
{
	TArray<FString> Codes;

	// Identity: an artifact that cannot say who asked, about what, in which world, is not evidence.
	if (R.RequestId.IsEmpty())      { Codes.AddUnique(TEXT("GSS015")); }
	if (R.SchemaVersion.IsEmpty())  { Codes.AddUnique(TEXT("GSS015")); }
	if (R.ProducerVersion.IsEmpty()){ Codes.AddUnique(TEXT("GSS015")); }
	if (R.CreatedAt.IsEmpty())      { Codes.AddUnique(TEXT("GSS015")); }
	if (R.WorldInstanceId.IsEmpty() && R.MapPackageName.IsEmpty() && R.MapName.IsEmpty())
	{
		Codes.AddUnique(TEXT("GSS015"));
	}
	if (R.SubjectId.IsNone() || R.SubjectId.ToString().IsEmpty())
	{
		Codes.AddUnique(TEXT("GSS002"));
	}

	if (R.Status == EGSSRequestStatus::Resolved)
	{
		if (R.ResolverKind == EGSSResolverKind::None) { Codes.AddUnique(TEXT("GSS006")); }
		if (R.AnchorMode == EGSSAnchorMode::None)     { Codes.AddUnique(TEXT("GSS005")); }
		if (R.AnchorMode == EGSSAnchorMode::ActorObjectPath && R.ActorObjectPath.IsEmpty())
		{
			Codes.AddUnique(TEXT("GSS003"));
		}
		if (R.ResolvedClassName.IsEmpty()) { Codes.AddUnique(TEXT("GSS003")); }
	}
	else
	{
		// Same never-guess rail as a subject: no status other than Resolved may carry a location.
		if (R.Status == EGSSRequestStatus::Unresolved) { Codes.AddUnique(TEXT("GSS001")); }
		if (!R.Transform.Equals(FTransform::Identity)) { Codes.AddUnique(TEXT("GSS004")); }
		if (!R.ActorObjectPath.IsEmpty())              { Codes.AddUnique(TEXT("GSS004")); }
		if (R.AnchorMode != EGSSAnchorMode::None)      { Codes.AddUnique(TEXT("GSS005")); }
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

	void SetStringOrNull(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FString& Value)
	{
		// Null rather than empty-string when there is nothing to report: a reader must not be able to
		// mistake "no anchor" for "an anchor at the origin".
		if (Value.IsEmpty()) { O->SetField(Field, MakeShared<FJsonValueNull>()); }
		else                 { O->SetStringField(Field, Value); }
	}

	void SetCodes(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const TArray<FString>& Codes)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& C : Codes) { Arr.Add(MakeShared<FJsonValueString>(C)); }
		O->SetArrayField(Field, Arr);
	}

	TSharedPtr<FJsonObject> SubjectJson(const FGloamsteadSurveySubject& S)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("subject_id"), S.SubjectId.ToString());
		O->SetStringField(TEXT("resolver_kind"), GSSResolverKindToken(S.ResolverKind));
		O->SetStringField(TEXT("anchor_mode"), GSSAnchorModeToken(S.AnchorMode));
		O->SetBoolField(TEXT("location_resolved"), S.bLocationResolved);
		O->SetNumberField(TEXT("candidate_count"), S.CandidateCount);

		SetStringOrNull(O, TEXT("actor_object_path"), S.ActorObjectPath);
		SetStringOrNull(O, TEXT("resolved_class"), S.ResolvedClassName);
		if (S.bLocationResolved) { O->SetObjectField(TEXT("transform"), TransformJson(S.Transform)); }
		else { O->SetField(TEXT("transform"), MakeShared<FJsonValueNull>()); }

		SetCodes(O, TEXT("failure_codes"), S.FailureCodes);
		return O;
	}

	TSharedPtr<FJsonObject> RequestJson(const FGloamsteadSurveyRequest& R)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		// Schema and producer are stamped from the record, not recomputed, so an archived artifact
		// keeps saying what actually produced it even after this code moves on.
		O->SetStringField(TEXT("schema"), R.SchemaVersion);
		O->SetStringField(TEXT("producer_version"), R.ProducerVersion);
		O->SetStringField(TEXT("request_id"), R.RequestId);
		O->SetStringField(TEXT("subject_id"), R.SubjectId.ToString());
		O->SetStringField(TEXT("created_at"), R.CreatedAt);
		O->SetStringField(TEXT("git_sha"), R.GitSha);
		O->SetStringField(TEXT("map_name"), R.MapName);
		SetStringOrNull(O, TEXT("map_package_name"), R.MapPackageName);
		SetStringOrNull(O, TEXT("world_instance_id"), R.WorldInstanceId);
		O->SetStringField(TEXT("status"), GSSRequestStatusToken(R.Status));
		O->SetStringField(TEXT("resolver_kind"), GSSResolverKindToken(R.ResolverKind));
		O->SetStringField(TEXT("anchor_mode"), GSSAnchorModeToken(R.AnchorMode));
		SetStringOrNull(O, TEXT("actor_object_path"), R.ActorObjectPath);
		SetStringOrNull(O, TEXT("resolved_class"), R.ResolvedClassName);
		if (R.Status == EGSSRequestStatus::Resolved)
		{
			O->SetObjectField(TEXT("transform"), TransformJson(R.Transform));
		}
		else
		{
			O->SetField(TEXT("transform"), MakeShared<FJsonValueNull>());
		}
		SetCodes(O, TEXT("failure_codes"), R.FailureCodes);
		return O;
	}

	FString SerializeJson(const TSharedPtr<FJsonObject>& Root)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return Out;
	}

	/** Parse text and confirm its "schema" field is exactly ExpectedSchema. */
	bool ParsesWithSchema(const FString& Text, const FString& ExpectedSchema)
	{
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			return false;
		}
		FString Schema;
		return Parsed->TryGetStringField(TEXT("schema"), Schema) && Schema == ExpectedSchema;
	}

	/**
	 * Atomic publish: directory -> temp file -> flush -> close -> re-read/parse/schema-check -> rename.
	 *
	 * A reader of FinalPath therefore only ever observes a whole, parseable, correctly-tagged
	 * artifact: on every failure path the temp file is removed and FinalPath is left exactly as it
	 * was. Nothing here touches gameplay state, so a failed write cannot roll anything back — it is
	 * reported loudly (GSS013/GSS014 + an Error log) and the caller decides.
	 */
	bool WriteStringAtomic(const FString& Contents, const FString& FinalPath,
		const FString& ExpectedSchema, TArray<FString>& OutCodes)
	{
		IFileManager& FM = IFileManager::Get();

		// 1. Create the output directory.
		const FString Dir = FPaths::GetPath(FinalPath);
		if (!Dir.IsEmpty() && !FM.DirectoryExists(*Dir))
		{
			FM.MakeDirectory(*Dir, /*Tree*/ true);
			if (!FM.DirectoryExists(*Dir))
			{
				OutCodes.AddUnique(TEXT("GSS013"));
				UE_LOG(LogGloamstead, Error,
					TEXT("[GSS013] Survey evidence NOT written: cannot create output directory '%s'."), *Dir);
				return false;
			}
		}

		// 2. Write to a temporary file. The GUID suffix keeps concurrent writers off each other.
		const FString TempPath = FinalPath + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
		{
			TUniquePtr<FArchive> Ar(FM.CreateFileWriter(*TempPath, FILEWRITE_EvenIfReadOnly));
			if (!Ar)
			{
				OutCodes.AddUnique(TEXT("GSS013"));
				UE_LOG(LogGloamstead, Error,
					TEXT("[GSS013] Survey evidence NOT written: cannot open temp file '%s'."), *TempPath);
				return false;
			}

			const FTCHARToUTF8 Utf8(*Contents);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			if (Bytes.Num() > 0)
			{
				Ar->Serialize(Bytes.GetData(), Bytes.Num());
			}

			// 3. Flush and close, and treat a failed close as a failed write — a buffered artifact
			//    that never reached the disk must not be renamed into place as if it had.
			Ar->Flush();
			const bool bClosed = Ar->Close();
			Ar.Reset();
			if (!bClosed)
			{
				FM.Delete(*TempPath, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
				OutCodes.AddUnique(TEXT("GSS013"));
				UE_LOG(LogGloamstead, Error,
					TEXT("[GSS013] Survey evidence NOT written: failed to flush/close temp file '%s'."), *TempPath);
				return false;
			}
		}

		// 4. Validate what actually landed on disk — not what we intended to write.
		{
			FString RoundTrip;
			if (!FFileHelper::LoadFileToString(RoundTrip, *TempPath)
				|| RoundTrip != Contents
				|| !ParsesWithSchema(RoundTrip, ExpectedSchema))
			{
				FM.Delete(*TempPath, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
				OutCodes.AddUnique(TEXT("GSS014"));
				UE_LOG(LogGloamstead, Error,
					TEXT("[GSS014] Survey evidence NOT published: temp artifact '%s' failed post-write validation ")
					TEXT("(expected schema '%s'). The previous artifact at '%s' is untouched."),
					*TempPath, *ExpectedSchema, *FinalPath);
				return false;
			}
		}

		// 5. Rename into the final path. Replace=true so a validated re-emit supersedes cleanly; the
		//    caller is responsible for refusing to overwrite an UNRELATED artifact (see GSS012).
		if (!FM.Move(*FinalPath, *TempPath, /*Replace*/ true, /*EvenIfReadOnly*/ true))
		{
			FM.Delete(*TempPath, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
			OutCodes.AddUnique(TEXT("GSS013"));
			UE_LOG(LogGloamstead, Error,
				TEXT("[GSS013] Survey evidence NOT published: cannot rename '%s' -> '%s'."), *TempPath, *FinalPath);
			return false;
		}

		return true;
	}

	/** Filesystem-safe form of a request id. Distinct ids may collapse here; that is caught by GSS012. */
	FString SanitizeForFilename(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len());
		for (const TCHAR C : In)
		{
			const bool bSafe =
				(C >= TEXT('a') && C <= TEXT('z')) ||
				(C >= TEXT('A') && C <= TEXT('Z')) ||
				(C >= TEXT('0') && C <= TEXT('9')) ||
				C == TEXT('.') || C == TEXT('-') || C == TEXT('_');
			Out.AppendChar(bSafe ? C : TEXT('_'));
		}
		return Out;
	}
}

FString GloamsteadSurveySubjectReport::DefaultReportDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("procedural"), TEXT("reports"), TEXT("gloamstead_survey_subjects"));
}

FString GloamsteadSurveySubjectReport::RequestDir(const FString& OutDir)
{
	return FPaths::Combine(OutDir, TEXT("requests"));
}

FString GloamsteadSurveySubjectReport::RequestPath(const FString& OutDir, const FString& RequestId)
{
	const FString Safe = GloamsteadSurveySubjectJson::SanitizeForFilename(RequestId);
	if (Safe.IsEmpty())
	{
		return FString();
	}
	return FPaths::Combine(RequestDir(OutDir), Safe + TEXT(".json"));
}

bool GloamsteadSurveySubjectReport::WriteReport(const FGloamsteadSurveySubjectReport& Report,
	const FString& OutDir, FString& OutPrimaryPath)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), GSSReportSchemaVersion());
	Root->SetStringField(TEXT("producer_version"),
		Report.ProducerVersion.IsEmpty() ? GSSProducerVersion() : Report.ProducerVersion);
	Root->SetStringField(TEXT("report_id"), Report.ReportId);
	// Request binding: which request this emission answers. Serialized as json null rather than ""
	// so an unattributed legacy report is visibly unattributed instead of quietly blank.
	GloamsteadSurveySubjectJson::SetStringOrNull(Root, TEXT("request_id"), Report.RequestId);
	// created_at / git_sha are the values the report was BUILT with. v1 recomputed both here, which
	// meant the artifact timestamped the moment of serialization and could attribute a report to a
	// commit that was checked out after the resolution ran.
	Root->SetStringField(TEXT("created_at"), Report.CreatedAt);
	Root->SetStringField(TEXT("git_sha"), Report.GitSha);
	Root->SetStringField(TEXT("map_name"), Report.MapName);
	GloamsteadSurveySubjectJson::SetStringOrNull(Root, TEXT("map_package_name"), Report.MapPackageName);
	GloamsteadSurveySubjectJson::SetStringOrNull(Root, TEXT("world_instance_id"), Report.WorldInstanceId);
	Root->SetNumberField(TEXT("declared_count"), Report.DeclaredCount);
	Root->SetNumberField(TEXT("resolved_count"), Report.ResolvedCount);
	Root->SetNumberField(TEXT("unresolved_count"), Report.UnresolvedCount);
	GloamsteadSurveySubjectJson::SetCodes(Root, TEXT("failure_codes"), Report.FailureCodes);
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FGloamsteadSurveySubject& S : Report.Subjects)
		{
			Arr.Add(MakeShared<FJsonValueObject>(GloamsteadSurveySubjectJson::SubjectJson(S)));
		}
		Root->SetArrayField(TEXT("subjects"), Arr);
	}

	OutPrimaryPath = FPaths::Combine(OutDir, TEXT("survey_subject_report.json"));

	const FString Contents = GloamsteadSurveySubjectJson::SerializeJson(Root);
	TArray<FString> Codes;
	return GloamsteadSurveySubjectJson::WriteStringAtomic(
		Contents, OutPrimaryPath, GSSReportSchemaVersion(), Codes);
}

bool GloamsteadSurveySubjectReport::WriteRequest(const FGloamsteadSurveyRequest& Request,
	const FString& OutDir, FString& OutPath, TArray<FString>& OutFailureCodes)
{
	OutFailureCodes.Reset();
	OutPath.Reset();

	// A record that fails its own contract is never published — a malformed artifact on disk is worse
	// than no artifact, because it looks like evidence.
	for (const FString& C : GSSValidateRequest(Request))
	{
		// GSS001 is an honest outcome (the subject did not resolve), not a reason to withhold the
		// record. Everything else is internal dishonesty and blocks the write.
		if (C != TEXT("GSS001")) { OutFailureCodes.AddUnique(C); }
	}
	if (OutFailureCodes.Num() > 0)
	{
		UE_LOG(LogGloamstead, Error,
			TEXT("[GSS] Survey request '%s' (subject '%s') is internally invalid and was NOT written: %s"),
			*Request.RequestId, *Request.SubjectId.ToString(), *FString::Join(OutFailureCodes, TEXT(",")));
		return false;
	}

	OutPath = RequestPath(OutDir, Request.RequestId);
	if (OutPath.IsEmpty())
	{
		OutFailureCodes.AddUnique(TEXT("GSS015"));
		UE_LOG(LogGloamstead, Error,
			TEXT("[GSS015] Survey request id '%s' cannot be filed under any artifact name."), *Request.RequestId);
		return false;
	}

	const FString Contents = GloamsteadSurveySubjectJson::SerializeJson(
		GloamsteadSurveySubjectJson::RequestJson(Request));

	// Collision guard. Re-emitting the identical record is idempotent; anything else filed under this
	// request id is a DIFFERENT request, and overwriting it would silently destroy evidence.
	if (FPaths::FileExists(OutPath))
	{
		FString Existing;
		const bool bRead = FFileHelper::LoadFileToString(Existing, *OutPath);
		if (bRead && Existing == Contents)
		{
			return true; // already published, byte-for-byte
		}
		OutFailureCodes.AddUnique(TEXT("GSS012"));
		UE_LOG(LogGloamstead, Error,
			TEXT("[GSS012] Survey request id '%s' is already filed at '%s' with different content. ")
			TEXT("Refusing to overwrite; nothing was written."), *Request.RequestId, *OutPath);
		return false;
	}

	if (!GloamsteadSurveySubjectJson::WriteStringAtomic(
			Contents, OutPath, Request.SchemaVersion, OutFailureCodes))
	{
		return false;
	}
	return true;
}
