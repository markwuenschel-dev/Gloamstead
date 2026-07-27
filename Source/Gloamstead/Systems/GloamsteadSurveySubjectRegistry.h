#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/GloamsteadSurveySubjectTypes.h"
#include "GloamsteadSurveySubjectRegistry.generated.h"

/**
 * Gloamstead Survey Subject Registry (Wave G2).
 *
 * Answers exactly one question: "what real thing in this project does the place-name X refer to,
 * right now?" It maps a declared place-name to the transform and object path of an actor that is
 * actually placed in the world.
 *
 * Like the MeshForge adapter it takes its stance from, it NEVER takes authority: it only reads
 * placed-actor state. It does not spawn, move, restore, or otherwise touch gameplay, and it does not
 * decide which map is open — it reports on the world it is given.
 *
 * RESOLVE, NEVER GUESS. If a declared subject has no placed actor, or more than one candidate, the
 * result comes back unresolved with a failure code and no coordinates. There is deliberately no
 * fallback anchor and no nearest-match: fabricating one here would be the same semantic-authority
 * inversion the 2026-07-19 boundary forbids WorldForge from committing on the far side.
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadSurveySubjectRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Every place-name this project is willing to be asked about, placed or not. */
	static TArray<FGloamsteadSurveySubjectDeclaration> GetDeclarations();

	/** The declaration for an id, or nullptr if this project does not declare that place-name. */
	static const FGloamsteadSurveySubjectDeclaration* FindDeclaration(FName SubjectId);

	/**
	 * Resolve one subject against this subsystem's world.
	 * @return true only when a single placed actor was found. On false, Out carries the failure
	 *         codes and NO location — callers must not substitute a default.
	 */
	bool ResolveSubject(FName SubjectId, FGloamsteadSurveySubject& Out) const;

	/** Resolve every declared subject. Unresolved entries are included, honestly marked. */
	TArray<FGloamsteadSurveySubject> ResolveAll() const;

	/** Aggregate the current resolution state into an auditable report. */
	FGloamsteadSurveySubjectReport BuildReport() const;

	/** Build + write the JSON report under procedural/reports/gloamstead_survey_subjects. */
	bool EmitReport(FString& OutPrimaryPath) const;

	/** Test seam: resolve against an explicit world rather than the subsystem's own. */
	bool Test_ResolveSubjectIn(UWorld* World, FName SubjectId, FGloamsteadSurveySubject& Out) const;
	/** Test seam: full report against an explicit world. */
	FGloamsteadSurveySubjectReport Test_BuildReportFor(UWorld* World) const;

private:
	bool ResolveSubjectIn(UWorld* World, FName SubjectId, FGloamsteadSurveySubject& Out) const;
	FGloamsteadSurveySubjectReport BuildReportFor(UWorld* World) const;
};
