#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "GloamsteadSurveySubjectTypes.generated.h"

class AActor;

/**
 * Gloamstead survey-subject vocabulary (Wave G2).
 *
 * Gloamstead owns SEMANTIC INTENT: which named place in this project a survey is about. These types
 * carry that intent and nothing else — they never describe how a survey is executed, captured, or
 * verified, which is WorldForge's half of the boundary ratified 2026-07-19.
 *
 * The governing discipline is RESOLVE, NEVER GUESS. A subject that cannot be resolved to a real
 * placed actor comes back unresolved, carrying a failure code and NO coordinates. There is no
 * fallback anchor, no nearest-match, and no default map — inventing any of those on this side is the
 * same authority inversion the boundary forbids on the far side.
 */

/** How a subject's location was established. There is deliberately no "Inferred" or "Default" kind. */
UENUM(BlueprintType)
enum class EGSSResolverKind : uint8
{
	/** Not resolved. Carries no location. */
	None                UMETA(DisplayName = "None"),
	/** Resolved by finding the single placed actor of a declared class in the world. */
	PlacedActorClass    UMETA(DisplayName = "Placed Actor Class"),
	/**
	 * Resolved from an explicit UGloamsteadSurveySubjectComponent placed on a map actor. The actor
	 * NAMES ITSELF as the subject; the registry does not search, match, or rank anything. This is the
	 * only kind that lets an authored map introduce a place-name without a code change, and it is
	 * still "resolve, never guess" — an unregistered id resolves to nothing at all.
	 */
	RegisteredComponent UMETA(DisplayName = "Registered Component"),
};

/** Lifecycle of one request-bound survey. A request that never resolved says so; it never guesses. */
UENUM(BlueprintType)
enum class EGSSRequestStatus : uint8
{
	/** Built but not yet run against a world. */
	Pending     UMETA(DisplayName = "Pending"),
	/** The named subject resolved to exactly one live actor. */
	Resolved    UMETA(DisplayName = "Resolved"),
	/** The named subject did not resolve. Carries failure codes and NO location. */
	Unresolved  UMETA(DisplayName = "Unresolved"),
};

/** How a resolved subject should be expressed as a WorldForge request anchor. Mirrors ANCHOR_MODES. */
UENUM(BlueprintType)
enum class EGSSAnchorMode : uint8
{
	/** No anchor — an unresolved subject must not produce one. */
	None                UMETA(DisplayName = "None"),
	/** Anchor by object path. Survives the greybox moving; the preferred mode. */
	ActorObjectPath     UMETA(DisplayName = "Actor Object Path"),
	/** Anchor by literal transform. Correct only at the instant it is read. */
	ExplicitTransform   UMETA(DisplayName = "Explicit Transform"),
};

/**
 * A declared subject: a stable place-name this project is willing to be asked about, plus the rule
 * for resolving it. Declaring a subject says nothing about whether it is currently placed.
 */
USTRUCT(BlueprintType)
struct FGloamsteadSurveySubjectDeclaration
{
	GENERATED_BODY()

	/** Stable dotted place-name, e.g. "sanctuary.heart". The id a request names. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") FName SubjectId;
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") EGSSResolverKind ResolverKind = EGSSResolverKind::None;
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") TSubclassOf<AActor> ActorClass;
	/** Human-readable intent — what this place IS, for the audit trail. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") FString Description;
};

/**
 * A resolution attempt's honest result. An unresolved subject is a first-class outcome, not an error
 * to be papered over: bLocationResolved is false, Transform stays identity, and the codes say why.
 */
USTRUCT(BlueprintType)
struct FGloamsteadSurveySubject
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") FName SubjectId;
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") EGSSResolverKind ResolverKind = EGSSResolverKind::None;
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") EGSSAnchorMode AnchorMode = EGSSAnchorMode::None;
	/** /Game/Map.Map:PersistentLevel.Actor_Name — empty unless resolved. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") FString ActorObjectPath;
	/** The CONCRETE class found, which may be a Blueprint subclass of the declared native class. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") FString ResolvedClassName;
	/** Identity unless bLocationResolved. Never a guess, never a fallback. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") FTransform Transform;
	/** True only when a real placed actor was found unambiguously. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") bool bLocationResolved = false;
	/** How many candidates the resolver saw. >1 is ambiguity, and ambiguity does not resolve. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") int32 CandidateCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "SurveySubject") TArray<FString> FailureCodes;
};

/**
 * One request-bound survey: "request R asked where subject S is, in world W, and here is the answer".
 *
 * A request is the unit of evidence. It is self-describing — it carries its own schema and producer
 * version so a reader holding only this record can tell what produced it and what shape to expect —
 * and it is self-identifying, so a second, unrelated request can never be mistaken for a re-run of
 * this one (see GSS012).
 */
USTRUCT(BlueprintType)
struct FGloamsteadSurveyRequest
{
	GENERATED_BODY()

	/** Caller-supplied or generated. The identity a stored artifact is filed under; never reused silently. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString RequestId;
	/** The place-name this request asked about. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FName SubjectId;
	/** World->GetMapName() — the map as the engine reports it (may carry a PIE prefix). */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString MapName;
	/** The world's package name, e.g. /Game/Maps/Courtyard. Read from the world, never defaulted. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString MapPackageName;
	/** Full object path of the specific UWorld instance surveyed — distinguishes PIE/editor/duplicated worlds. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString WorldInstanceId;
	/** Empty unless Status == Resolved. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString ActorObjectPath;
	/** Identity unless Status == Resolved. Never a guess, never a fallback. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FTransform Transform;
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") EGSSRequestStatus Status = EGSSRequestStatus::Pending;
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") EGSSResolverKind ResolverKind = EGSSResolverKind::None;
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") EGSSAnchorMode AnchorMode = EGSSAnchorMode::None;
	/** The concrete class actually found. Empty unless resolved. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString ResolvedClassName;
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") TArray<FString> FailureCodes;
	/** Stamped at build time from GSSRequestSchemaVersion(). */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString SchemaVersion;
	/** Stamped at build time from GSSProducerVersion(). Identifies the code that produced the record. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString ProducerVersion;
	/** ISO-8601 UTC, stamped once when the request was built — NOT re-stamped at write time. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString CreatedAt;
	/** Repo git SHA at build time, for provenance. */
	UPROPERTY(BlueprintReadOnly, Category = "SurveyRequest") FString GitSha;
};

/** Aggregate resolution report — the auditable artifact this wave emits. */
USTRUCT(BlueprintType)
struct FGloamsteadSurveySubjectReport
{
	GENERATED_BODY()

	UPROPERTY() FString ReportId;
	/** The request this whole emission answers. A report with no request id is unattributable evidence. */
	UPROPERTY() FString RequestId;
	UPROPERTY() FString CreatedAt;
	UPROPERTY() FString GitSha;
	/** Stamped at build time from GSSProducerVersion(). */
	UPROPERTY() FString ProducerVersion;
	/** The map the resolution actually ran against. Read from the world, never defaulted. */
	UPROPERTY() FString MapName;
	/** The surveyed world's package name, e.g. /Game/Maps/Courtyard. */
	UPROPERTY() FString MapPackageName;
	/** Full object path of the specific UWorld instance surveyed. */
	UPROPERTY() FString WorldInstanceId;
	UPROPERTY() int32 DeclaredCount = 0;
	UPROPERTY() int32 ResolvedCount = 0;
	UPROPERTY() int32 UnresolvedCount = 0;
	UPROPERTY() TArray<FString> FailureCodes;
	UPROPERTY() TArray<FGloamsteadSurveySubject> Subjects;
};

// ===== Token helpers =====
GLOAMSTEAD_API FString GSSResolverKindToken(EGSSResolverKind Kind);
GLOAMSTEAD_API FString GSSAnchorModeToken(EGSSAnchorMode Mode);
GLOAMSTEAD_API FString GSSRequestStatusToken(EGSSRequestStatus Status);

// ===== Versioning =====
//
// Both are stamped INTO the artifacts, not merely documented, so a reader never has to guess which
// producer wrote a file it is holding.
//
//  Report schema v2 (was v1) adds: request_id, producer_version, map_package_name, world_instance_id,
//  and fixes v1's bug of re-stamping created_at / git_sha at write time instead of serializing the
//  values the report was built with.

/** Wire schema tag stamped into the aggregate report artifact. */
GLOAMSTEAD_API FString GSSReportSchemaVersion();
/** Wire schema tag stamped into each request-bound artifact. */
GLOAMSTEAD_API FString GSSRequestSchemaVersion();
/** Identifies the code that produced an artifact. Bump on any change to emitted field semantics. */
GLOAMSTEAD_API FString GSSProducerVersion();

// ===== Fail-closed validation (returns GSS codes; empty = valid) =====
//
//  GSS001  subject unresolved                       (near-side mirror of WF1106_SUBJECT_UNRESOLVED)
//  GSS002  subject id empty
//  GSS003  claims resolved but carries no evidence   (no object path under ActorObjectPath mode)
//  GSS004  carries a transform while unresolved      (the never-guess rail)
//  GSS005  anchor mode disagrees with the evidence
//  GSS006  claims resolved with resolver kind None
//  GSS007  ambiguous — more than one candidate, or a second live component claiming a taken subject id
//  GSS008  stale registration — the registered component or its actor no longer exists
//  GSS009  registration rejected — component has no owning actor, or is otherwise unregisterable
//  GSS010  report declares no subjects at all
//  GSS011  report counts do not reconcile with its subject list
//  GSS012  request id collision — a different artifact is already filed under this request id
//  GSS013  atomic write failed — temp write, flush/close, or rename into place did not complete
//  GSS014  emitted artifact failed post-write validation (did not re-parse, or schema tag mismatched)
//  GSS015  request carries no request id, or no world/map identity to attribute it to

/** Validate one resolution result's internal honesty. */
GLOAMSTEAD_API TArray<FString> GSSValidateSubject(const FGloamsteadSurveySubject& Subject);
/** Validate an aggregate report (counts reconcile + per-subject honesty rolls up). */
GLOAMSTEAD_API TArray<FString> GSSValidateReport(const FGloamsteadSurveySubjectReport& Report);
/** Validate one request record: identity present, status agrees with the evidence, no phantom location. */
GLOAMSTEAD_API TArray<FString> GSSValidateRequest(const FGloamsteadSurveyRequest& Request);

namespace GloamsteadSurveySubjectReport
{
	/** Default report dir: <ProjectDir>/procedural/reports/gloamstead_survey_subjects. */
	GLOAMSTEAD_API FString DefaultReportDir();

	/** Per-request artifact dir: <OutDir>/requests. */
	GLOAMSTEAD_API FString RequestDir(const FString& OutDir);

	/** Final artifact path for a request id, or empty when the id cannot be filed. */
	GLOAMSTEAD_API FString RequestPath(const FString& OutDir, const FString& RequestId);

	/**
	 * Serialize the resolution report to <OutDir>/survey_subject_report.json.
	 * Write is atomic: temp file -> flush -> close -> re-parse and schema-check -> rename into place.
	 * A failed write leaves any pre-existing artifact untouched.
	 */
	GLOAMSTEAD_API bool WriteReport(const FGloamsteadSurveySubjectReport& Report,
		const FString& OutDir, FString& OutPrimaryPath);

	/**
	 * Serialize one request-bound record to <OutDir>/requests/<request_id>.json, atomically.
	 *
	 * Refuses to clobber: if an artifact already exists under this request id and its bytes differ
	 * from what this call would write, nothing is written and GSS012 is returned. Re-emitting the
	 * byte-identical record is idempotent and succeeds.
	 *
	 * @return true only when the artifact is on disk and validated. OutFailureCodes carries the why.
	 */
	GLOAMSTEAD_API bool WriteRequest(const FGloamsteadSurveyRequest& Request,
		const FString& OutDir, FString& OutPath, TArray<FString>& OutFailureCodes);
}
