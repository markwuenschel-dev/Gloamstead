#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/GloamsteadSurveySubjectTypes.h"
#include "GloamsteadSurveySubjectRegistry.generated.h"

class UGloamsteadSurveySubjectComponent;

/**
 * One live claim on a place-name, held by a UGloamsteadSurveySubjectComponent.
 *
 * Weak on BOTH ends, deliberately. A destroyed actor can therefore never be handed back as a
 * resolution: the weak pointer reads as null and the registry reports a stale registration (GSS008)
 * instead of dereferencing a dangling address or, worse, answering with coordinates from an object
 * that no longer exists. Not a USTRUCT — reflecting a non-owning reference would suggest the
 * registry has some stake in the actor's lifetime, and it must not.
 */
struct FGSSComponentRegistration
{
	TWeakObjectPtr<UGloamsteadSurveySubjectComponent> Component;
	TWeakObjectPtr<AActor> Actor;
};

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
 *
 * Two declaration paths, consulted in this order:
 *   1. COMPONENT REGISTRATION — an actor in the map carries a UGloamsteadSurveySubjectComponent and
 *      names itself. Lets an authored map introduce a place-name with no code change.
 *   2. DECLARED CLASS TABLE — the hard-coded GetDeclarations() list, resolved by finding the single
 *      placed actor of that class.
 * Both are declarations. Neither is a search: an id with no registration and no declaration comes
 * back unresolved, exactly as before.
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

	// ===== Component registration (path 1) =====

	/**
	 * Claim Component->SubjectId for Component in THIS subsystem's world.
	 *
	 * Idempotent: the same component re-registering the same id succeeds and changes nothing but the
	 * cached owner reference. A different LIVE component claiming a taken id is refused with GSS007.
	 * A taken id whose registered component/actor has since died is reclaimable — a dead claim is not
	 * a competing claim.
	 *
	 * @param OutFailureCodes reset then filled on refusal (GSS002 empty id, GSS007 duplicate,
	 *        GSS009 unregisterable component).
	 * @return true when Component owns the id afterwards.
	 */
	bool RegisterSubjectComponent(UGloamsteadSurveySubjectComponent* Component, TArray<FString>& OutFailureCodes);

	/** Withdraw every claim held by Component. Safe on an unregistered or already-withdrawn component. */
	void UnregisterSubjectComponent(UGloamsteadSurveySubjectComponent* Component);

	/**
	 * The component currently claiming SubjectId, or nullptr.
	 * Returns nullptr for a stale claim too — a claim whose component has been destroyed is not a
	 * live registration, and this must never hand back a dangling pointer.
	 */
	const UGloamsteadSurveySubjectComponent* GetRegisteredComponent(FName SubjectId) const;

	/** True when SubjectId has a live component registration in this world. */
	bool IsSubjectRegistered(FName SubjectId) const;

	/** Every subject id with a claim in this world, live or stale. Sorted for stable evidence. */
	TArray<FName> GetRegisteredSubjectIds() const;

	// ===== Resolution =====

	/**
	 * Resolve one subject against this subsystem's world.
	 * @return true only when a single placed actor was found. On false, Out carries the failure
	 *         codes and NO location — callers must not substitute a default.
	 */
	bool ResolveSubject(FName SubjectId, FGloamsteadSurveySubject& Out) const;

	/** Resolve every declared subject. Unresolved entries are included, honestly marked. */
	TArray<FGloamsteadSurveySubject> ResolveAll() const;

	// ===== Request-bound evidence =====

	/**
	 * Build one request-bound record for SubjectId against this subsystem's world.
	 * Pass an empty RequestId to have a fresh GUID minted. The record is stamped with schema,
	 * producer version, world identity, and a build-time timestamp — all of which are then
	 * SERIALIZED AS STAMPED, never recomputed at write time.
	 */
	FGloamsteadSurveyRequest BuildRequest(FName SubjectId, const FString& RequestId) const;

	/**
	 * Build + atomically publish one request-bound artifact under
	 * procedural/reports/gloamstead_survey_subjects/requests/<request_id>.json.
	 *
	 * Reporting is downstream of gameplay and never rolls it back: this reads world state only, so a
	 * failed emission leaves everything that already happened intact. It is surfaced loudly instead —
	 * an Error log plus GSS012/GSS013/GSS014/GSS015 in OutFailureCodes.
	 *
	 * @return true only when the artifact is on disk and validated.
	 */
	bool EmitRequest(const FGloamsteadSurveyRequest& Request, FString& OutPath, TArray<FString>& OutFailureCodes) const;

	/** Convenience: build a request for SubjectId and publish it in one call. */
	bool SurveyAndEmit(FName SubjectId, const FString& RequestId, FString& OutPath, TArray<FString>& OutFailureCodes) const;

	// ===== Aggregate report =====

	/** Aggregate the current resolution state into an auditable report bound to RequestId. */
	FGloamsteadSurveySubjectReport BuildReport(const FString& RequestId) const;

	/** Build + write the JSON report under procedural/reports/gloamstead_survey_subjects. */
	bool EmitReport(const FString& RequestId, FString& OutPrimaryPath) const;

	// ===== Test seams =====

	/** Test seam: resolve against an explicit world rather than the subsystem's own. */
	bool Test_ResolveSubjectIn(UWorld* World, FName SubjectId, FGloamsteadSurveySubject& Out) const;
	/** Test seam: full report against an explicit world. */
	FGloamsteadSurveySubjectReport Test_BuildReportFor(UWorld* World, const FString& RequestId = FString()) const;
	/** Test seam: request-bound record against an explicit world. */
	FGloamsteadSurveyRequest Test_BuildRequestFor(UWorld* World, FName SubjectId, const FString& RequestId) const;

private:
	/**
	 * Not a UPROPERTY by design: these are deliberately weak, non-owning references. Reflecting them
	 * would neither keep the actors alive (weak pointers never do) nor add anything the registry
	 * needs, and a registry that kept surveyed actors alive would be taking authority over gameplay.
	 * Scope is per-world, because the subsystem is per-world.
	 */
	TMap<FName, FGSSComponentRegistration> ComponentRegistrations;

	bool ResolveSubjectIn(UWorld* World, FName SubjectId, FGloamsteadSurveySubject& Out) const;
	FGloamsteadSurveySubjectReport BuildReportFor(UWorld* World, const FString& RequestId) const;
	FGloamsteadSurveyRequest BuildRequestIn(UWorld* World, FName SubjectId, const FString& RequestId) const;

	/** Fill a request's world/map identity fields from World. */
	static void StampWorldIdentity(const UWorld* World, FString& OutMapName, FString& OutMapPackageName, FString& OutWorldInstanceId);
};
