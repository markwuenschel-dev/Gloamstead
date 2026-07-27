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

/** Aggregate resolution report — the auditable artifact this wave emits. */
USTRUCT(BlueprintType)
struct FGloamsteadSurveySubjectReport
{
	GENERATED_BODY()

	UPROPERTY() FString ReportId;
	UPROPERTY() FString CreatedAt;
	UPROPERTY() FString GitSha;
	/** The map the resolution actually ran against. Read from the world, never defaulted. */
	UPROPERTY() FString MapName;
	UPROPERTY() int32 DeclaredCount = 0;
	UPROPERTY() int32 ResolvedCount = 0;
	UPROPERTY() int32 UnresolvedCount = 0;
	UPROPERTY() TArray<FString> FailureCodes;
	UPROPERTY() TArray<FGloamsteadSurveySubject> Subjects;
};

// ===== Token helpers =====
GLOAMSTEAD_API FString GSSResolverKindToken(EGSSResolverKind Kind);
GLOAMSTEAD_API FString GSSAnchorModeToken(EGSSAnchorMode Mode);

// ===== Fail-closed validation (returns GSS codes; empty = valid) =====
//
//  GSS001  subject unresolved                       (near-side mirror of WF1106_SUBJECT_UNRESOLVED)
//  GSS002  subject id empty
//  GSS003  claims resolved but carries no evidence   (no object path under ActorObjectPath mode)
//  GSS004  carries a transform while unresolved      (the never-guess rail)
//  GSS005  anchor mode disagrees with the evidence
//  GSS006  claims resolved with resolver kind None
//  GSS007  ambiguous — more than one candidate; a survey subject must not be picked from a set
//  GSS010  report declares no subjects at all
//  GSS011  report counts do not reconcile with its subject list

/** Validate one resolution result's internal honesty. */
GLOAMSTEAD_API TArray<FString> GSSValidateSubject(const FGloamsteadSurveySubject& Subject);
/** Validate an aggregate report (counts reconcile + per-subject honesty rolls up). */
GLOAMSTEAD_API TArray<FString> GSSValidateReport(const FGloamsteadSurveySubjectReport& Report);

namespace GloamsteadSurveySubjectReport
{
	/** Default report dir: <ProjectDir>/procedural/reports/gloamstead_survey_subjects. */
	GLOAMSTEAD_API FString DefaultReportDir();
	/** Serialize the resolution report to JSON under OutDir. */
	GLOAMSTEAD_API bool WriteReport(const FGloamsteadSurveySubjectReport& Report,
		const FString& OutDir, FString& OutPrimaryPath);
}
