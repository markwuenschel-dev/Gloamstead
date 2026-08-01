#include "Systems/GloamsteadSurveySubjectRegistry.h"
#include "Components/GloamsteadSurveySubjectComponent.h"
#include "Systems/VeilHeart.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/GloamsteadForgeEvidence.h"
#include "PCG/GloamsteadSanctuaryBootstrap.h"
#include "Gloamstead.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"

// The declared place-name table.
//
// Hard-coded classes, deliberately NOT hard-coded coordinates: the class is the stable intent ("the
// Veil Heart"), while the location is whatever the level currently says it is. When the courtyard
// greybox lands (G4) this table gains named areas; nothing about the resolution contract changes.
TArray<FGloamsteadSurveySubjectDeclaration> UGloamsteadSurveySubjectRegistry::GetDeclarations()
{
	TArray<FGloamsteadSurveySubjectDeclaration> Out;

	{
		FGloamsteadSurveySubjectDeclaration D;
		D.SubjectId = TEXT("sanctuary.heart");
		D.ResolverKind = EGSSResolverKind::PlacedActorClass;
		D.ActorClass = AVeilHeart::StaticClass();
		D.Description = TEXT("The Veil Heart — the sanctuary's centre and the player's orientation anchor.");
		Out.Add(D);
	}
	{
		FGloamsteadSurveySubjectDeclaration D;
		D.SubjectId = TEXT("sanctuary.bootstrap");
		D.ResolverKind = EGSSResolverKind::PlacedActorClass;
		D.ActorClass = AGloamsteadSanctuaryBootstrap::StaticClass();
		D.Description = TEXT("The sanctuary bootstrap volume that seeds ritual points for this map.");
		Out.Add(D);
	}
	{
		FGloamsteadSurveySubjectDeclaration D;
		D.SubjectId = TEXT("sanctuary.night_director");
		D.ResolverKind = EGSSResolverKind::PlacedActorClass;
		// NOTE: this one is placed as a Blueprint subclass (BP_FirstNightDirector_C), not the native
		// class. GetAllActorsOfClass matches subclasses, so resolution works — but the concrete class
		// is reported separately in ResolvedClassName precisely so the difference stays visible.
		D.ActorClass = AGloamsteadFirstNightDirector::StaticClass();
		D.Description = TEXT("The first-night director that drives the opening night's sequence.");
		Out.Add(D);
	}
	{
		// Deliberately NO ActorClass. There is no "first lantern" class to search for, and inventing
		// one — or matching on an actor label — would be exactly the guess this whole system refuses.
		// The authored courtyard actor carries a UGloamsteadSurveySubjectComponent and names itself;
		// until it does, this place-name is declared and honestly unresolved.
		FGloamsteadSurveySubjectDeclaration D;
		D.SubjectId = TEXT("courtyard.lantern.first");
		D.ResolverKind = EGSSResolverKind::RegisteredComponent;
		D.Description = TEXT("The first lantern lit in the courtyard — identified by the map actor that claims it.");
		Out.Add(D);
	}

	return Out;
}

const FGloamsteadSurveySubjectDeclaration* UGloamsteadSurveySubjectRegistry::FindDeclaration(FName SubjectId)
{
	static const TArray<FGloamsteadSurveySubjectDeclaration> Declarations = GetDeclarations();
	return Declarations.FindByPredicate(
		[SubjectId](const FGloamsteadSurveySubjectDeclaration& D) { return D.SubjectId == SubjectId; });
}

// ===== Component registration =====

bool UGloamsteadSurveySubjectRegistry::RegisterSubjectComponent(
	UGloamsteadSurveySubjectComponent* Component, TArray<FString>& OutFailureCodes)
{
	OutFailureCodes.Reset();

	if (!IsValid(Component))
	{
		OutFailureCodes.AddUnique(TEXT("GSS009"));
		return false;
	}

	const FName SubjectId = Component->SubjectId;
	if (SubjectId.IsNone() || SubjectId.ToString().IsEmpty())
	{
		// An anonymous claim is not a claim.
		OutFailureCodes.AddUnique(TEXT("GSS002"));
		OutFailureCodes.AddUnique(TEXT("GSS009"));
		return false;
	}

	AActor* Owner = Component->GetOwner();
	if (!IsValid(Owner))
	{
		// The registration exists to name an ACTOR. A component with no owner has nothing to point at.
		OutFailureCodes.AddUnique(TEXT("GSS009"));
		return false;
	}

	// Drop any claim this same component holds under a DIFFERENT id (SubjectId edited at runtime).
	// Stale claims by OTHER components are deliberately left in place so they stay reportable as
	// GSS008 rather than silently vanishing.
	for (auto It = ComponentRegistrations.CreateIterator(); It; ++It)
	{
		if (It.Key() != SubjectId && It.Value().Component.Get() == Component)
		{
			It.RemoveCurrent();
		}
	}

	if (FGSSComponentRegistration* Existing = ComponentRegistrations.Find(SubjectId))
	{
		if (Existing->Component.Get() == Component)
		{
			// Idempotent: same component, same id. Not an error; refresh the actor reference in case
			// the component was re-attached to a different owner.
			Existing->Actor = Owner;
			return true;
		}
		if (Existing->Component.IsValid() && Existing->Actor.IsValid())
		{
			// Two live actors claiming one place-name is ambiguity, and ambiguity never resolves.
			// The FIRST claim stands; the second is refused rather than silently overriding it.
			OutFailureCodes.AddUnique(TEXT("GSS007"));
			UE_LOG(LogGloamstead, Warning,
				TEXT("[GSS007] Survey subject '%s' is already claimed by '%s'; refusing the duplicate claim from '%s'."),
				*SubjectId.ToString(), *GetNameSafe(Existing->Actor.Get()), *GetNameSafe(Owner));
			return false;
		}
		// The incumbent claim is dead. A dead claim is not a competing claim, so this one takes it.
	}

	FGSSComponentRegistration Reg;
	Reg.Component = Component;
	Reg.Actor = Owner;
	ComponentRegistrations.Add(SubjectId, Reg);
	return true;
}

void UGloamsteadSurveySubjectRegistry::UnregisterSubjectComponent(UGloamsteadSurveySubjectComponent* Component)
{
	if (!Component)
	{
		return;
	}
	for (auto It = ComponentRegistrations.CreateIterator(); It; ++It)
	{
		if (It.Value().Component.Get() == Component)
		{
			It.RemoveCurrent();
		}
	}
}

const UGloamsteadSurveySubjectComponent* UGloamsteadSurveySubjectRegistry::GetRegisteredComponent(FName SubjectId) const
{
	const FGSSComponentRegistration* Reg = ComponentRegistrations.Find(SubjectId);
	// .Get() on the weak pointer is the whole point: a destroyed component reads back as nullptr
	// instead of a dangling address.
	return Reg ? Reg->Component.Get() : nullptr;
}

bool UGloamsteadSurveySubjectRegistry::IsSubjectRegistered(FName SubjectId) const
{
	const FGSSComponentRegistration* Reg = ComponentRegistrations.Find(SubjectId);
	return Reg && Reg->Component.IsValid() && Reg->Actor.IsValid();
}

TArray<FName> UGloamsteadSurveySubjectRegistry::GetRegisteredSubjectIds() const
{
	TArray<FName> Ids;
	Ids.Reserve(ComponentRegistrations.Num());
	for (const TPair<FName, FGSSComponentRegistration>& Pair : ComponentRegistrations)
	{
		Ids.Add(Pair.Key);
	}
	// TMap iteration order is not stable across runs; evidence listings must be.
	Ids.Sort(FNameLexicalLess());
	return Ids;
}

// ===== Resolution =====

bool UGloamsteadSurveySubjectRegistry::ResolveSubjectIn(UWorld* World, FName SubjectId, FGloamsteadSurveySubject& Out) const
{
	Out = FGloamsteadSurveySubject();
	Out.SubjectId = SubjectId;

	if (SubjectId.IsNone() || SubjectId.ToString().IsEmpty())
	{
		Out.FailureCodes.AddUnique(TEXT("GSS002"));
		Out.FailureCodes.AddUnique(TEXT("GSS001"));
		return false;
	}

	if (!World)
	{
		Out.FailureCodes.AddUnique(TEXT("GSS001"));
		return false;
	}

	// --- Path 1: an actor that named itself, via UGloamsteadSurveySubjectComponent. ---
	//
	// Registrations live on the registry of the world being resolved, not necessarily on `this` — the
	// test seam can hand us a foreign world, and answering it from another world's registrations
	// would report an actor that is not in the surveyed map.
	if (const UGloamsteadSurveySubjectRegistry* WorldRegistry = World->GetSubsystem<UGloamsteadSurveySubjectRegistry>())
	{
		if (const FGSSComponentRegistration* Reg = WorldRegistry->ComponentRegistrations.Find(SubjectId))
		{
			// Exactly one claim can exist per id (RegisterSubjectComponent refuses a second live one),
			// so a registration is unambiguous by construction.
			Out.CandidateCount = 1;

			// Resolve the weak pointer ONCE and read every field off that single pointer, so the
			// object path and the transform cannot come from two different objects — or from an
			// object that died between the two reads.
			AActor* Actor = Reg->Actor.Get();
			if (!Actor || !Reg->Component.IsValid())
			{
				// The claim outlived its actor. That is a reported failure, never a fallback: we do
				// NOT drop through to the class table, because dropping through would answer a
				// question about THIS lantern with a different actor.
				Out.CandidateCount = 0;
				Out.FailureCodes.AddUnique(TEXT("GSS008"));
				Out.FailureCodes.AddUnique(TEXT("GSS001"));
				return false;
			}

			Out.ResolverKind = EGSSResolverKind::RegisteredComponent;
			Out.AnchorMode = EGSSAnchorMode::ActorObjectPath;
			Out.ActorObjectPath = Actor->GetPathName();
			Out.ResolvedClassName = Actor->GetClass()->GetName();
			Out.Transform = Actor->GetActorTransform();
			Out.bLocationResolved = true;
			return true;
		}
	}

	// --- Path 2: the hard-coded declared-class table. ---
	const FGloamsteadSurveySubjectDeclaration* Decl = FindDeclaration(SubjectId);
	if (!Decl)
	{
		// An undeclared place-name is unresolved, not an invitation to search for something similar.
		Out.FailureCodes.AddUnique(TEXT("GSS001"));
		return false;
	}

	if (Decl->ResolverKind == EGSSResolverKind::RegisteredComponent || !Decl->ActorClass)
	{
		// Declared, but its only resolver is a map actor that has not registered. Honestly unresolved.
		Out.FailureCodes.AddUnique(TEXT("GSS001"));
		return false;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, Decl->ActorClass, Found);
	Out.CandidateCount = Found.Num();

	if (Found.Num() == 0)
	{
		Out.FailureCodes.AddUnique(TEXT("GSS001"));
		return false;
	}
	if (Found.Num() > 1)
	{
		// Ambiguity does not resolve. Picking Found[0] here would be an inferred subject — fine for a
		// debug visualiser, fatal for evidence that has to prove WHICH thing was surveyed.
		Out.FailureCodes.AddUnique(TEXT("GSS007"));
		Out.FailureCodes.AddUnique(TEXT("GSS001"));
		return false;
	}

	const AActor* Actor = Found[0];
	if (!Actor)
	{
		Out.FailureCodes.AddUnique(TEXT("GSS001"));
		return false;
	}

	Out.ResolverKind = Decl->ResolverKind;
	Out.AnchorMode = EGSSAnchorMode::ActorObjectPath;
	Out.ActorObjectPath = Actor->GetPathName();
	Out.ResolvedClassName = Actor->GetClass()->GetName();
	Out.Transform = Actor->GetActorTransform();
	Out.bLocationResolved = true;
	return true;
}

bool UGloamsteadSurveySubjectRegistry::ResolveSubject(FName SubjectId, FGloamsteadSurveySubject& Out) const
{
	return ResolveSubjectIn(GetWorld(), SubjectId, Out);
}

bool UGloamsteadSurveySubjectRegistry::Test_ResolveSubjectIn(UWorld* World, FName SubjectId, FGloamsteadSurveySubject& Out) const
{
	return ResolveSubjectIn(World, SubjectId, Out);
}

TArray<FGloamsteadSurveySubject> UGloamsteadSurveySubjectRegistry::ResolveAll() const
{
	TArray<FGloamsteadSurveySubject> Out;
	for (const FGloamsteadSurveySubjectDeclaration& D : GetDeclarations())
	{
		FGloamsteadSurveySubject S;
		ResolveSubjectIn(GetWorld(), D.SubjectId, S);
		Out.Add(S);
	}
	return Out;
}

// ===== Request-bound evidence =====

void UGloamsteadSurveySubjectRegistry::StampWorldIdentity(const UWorld* World,
	FString& OutMapName, FString& OutMapPackageName, FString& OutWorldInstanceId)
{
	OutMapName.Reset();
	OutMapPackageName.Reset();
	OutWorldInstanceId.Reset();
	if (!World)
	{
		return;
	}
	// All three are read from the world we were handed and never defaulted. The instance id is the
	// world's full object path, which is what distinguishes a PIE duplicate from the editor original
	// — without it, two artifacts from the same map in different sessions look interchangeable.
	OutMapName = World->GetMapName();
	if (const UPackage* Package = World->GetOutermost())
	{
		OutMapPackageName = Package->GetName();
	}
	OutWorldInstanceId = World->GetPathName();
}

FGloamsteadSurveyRequest UGloamsteadSurveySubjectRegistry::BuildRequestIn(
	UWorld* World, FName SubjectId, const FString& RequestId) const
{
	FGloamsteadSurveyRequest R;
	R.RequestId = RequestId.IsEmpty()
		? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens)
		: RequestId;
	R.SubjectId = SubjectId;
	R.SchemaVersion = GSSRequestSchemaVersion();
	R.ProducerVersion = GSSProducerVersion();
	// Stamped once, here. WriteRequest serializes these values rather than recomputing them, so an
	// archived artifact keeps saying when it was produced and against which commit.
	R.CreatedAt = FDateTime::UtcNow().ToIso8601();
	R.GitSha = GloamsteadForgeEvidence::ReadGitCommit();
	StampWorldIdentity(World, R.MapName, R.MapPackageName, R.WorldInstanceId);

	FGloamsteadSurveySubject S;
	const bool bResolved = ResolveSubjectIn(World, SubjectId, S);

	R.ResolverKind = S.ResolverKind;
	R.AnchorMode = S.AnchorMode;
	R.FailureCodes = S.FailureCodes;
	if (bResolved)
	{
		R.Status = EGSSRequestStatus::Resolved;
		R.ActorObjectPath = S.ActorObjectPath;
		R.ResolvedClassName = S.ResolvedClassName;
		R.Transform = S.Transform;
	}
	else
	{
		// The never-guess rail, restated at the request layer: an unresolved request carries no
		// location at all, so nothing downstream can mistake a default for a survey result.
		R.Status = EGSSRequestStatus::Unresolved;
		R.ActorObjectPath.Reset();
		R.ResolvedClassName.Reset();
		R.Transform = FTransform::Identity;
		R.AnchorMode = EGSSAnchorMode::None;
		R.ResolverKind = EGSSResolverKind::None;
		R.FailureCodes.AddUnique(TEXT("GSS001"));
	}
	return R;
}

FGloamsteadSurveyRequest UGloamsteadSurveySubjectRegistry::BuildRequest(FName SubjectId, const FString& RequestId) const
{
	return BuildRequestIn(GetWorld(), SubjectId, RequestId);
}

FGloamsteadSurveyRequest UGloamsteadSurveySubjectRegistry::Test_BuildRequestFor(
	UWorld* World, FName SubjectId, const FString& RequestId) const
{
	return BuildRequestIn(World, SubjectId, RequestId);
}

bool UGloamsteadSurveySubjectRegistry::EmitRequest(const FGloamsteadSurveyRequest& Request,
	FString& OutPath, TArray<FString>& OutFailureCodes) const
{
	// Read-only by construction: nothing above this line mutated world state, so a failed publish
	// cannot roll back anything that already happened in the session. It is surfaced, not swallowed.
	const bool bOk = GloamsteadSurveySubjectReport::WriteRequest(
		Request, GloamsteadSurveySubjectReport::DefaultReportDir(), OutPath, OutFailureCodes);
	if (!bOk)
	{
		UE_LOG(LogGloamstead, Error,
			TEXT("[GSS] Survey request '%s' for subject '%s' was NOT published (%s). Gameplay is unaffected; ")
			TEXT("the evidence trail is incomplete."),
			*Request.RequestId, *Request.SubjectId.ToString(), *FString::Join(OutFailureCodes, TEXT(",")));
	}
	return bOk;
}

bool UGloamsteadSurveySubjectRegistry::SurveyAndEmit(FName SubjectId, const FString& RequestId,
	FString& OutPath, TArray<FString>& OutFailureCodes) const
{
	return EmitRequest(BuildRequest(SubjectId, RequestId), OutPath, OutFailureCodes);
}

// ===== Aggregate report =====

FGloamsteadSurveySubjectReport UGloamsteadSurveySubjectRegistry::BuildReportFor(UWorld* World, const FString& RequestId) const
{
	FGloamsteadSurveySubjectReport R;
	R.ReportId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	R.RequestId = RequestId.IsEmpty()
		? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens)
		: RequestId;
	R.CreatedAt = FDateTime::UtcNow().ToIso8601();
	// Stamped here rather than at write time, so the artifact names the commit the resolution
	// actually ran against.
	R.GitSha = GloamsteadForgeEvidence::ReadGitCommit();
	R.ProducerVersion = GSSProducerVersion();
	// The map is read from the world we were handed. It is never defaulted — a report that cannot say
	// which map it describes is not evidence.
	StampWorldIdentity(World, R.MapName, R.MapPackageName, R.WorldInstanceId);

	const TArray<FGloamsteadSurveySubjectDeclaration> Declarations = GetDeclarations();
	R.DeclaredCount = Declarations.Num();

	for (const FGloamsteadSurveySubjectDeclaration& D : Declarations)
	{
		FGloamsteadSurveySubject S;
		const bool bResolved = ResolveSubjectIn(World, D.SubjectId, S);
		if (bResolved) { ++R.ResolvedCount; } else { ++R.UnresolvedCount; }
		R.Subjects.Add(S);
	}

	R.FailureCodes = GSSValidateReport(R);
	return R;
}

FGloamsteadSurveySubjectReport UGloamsteadSurveySubjectRegistry::BuildReport(const FString& RequestId) const
{
	return BuildReportFor(GetWorld(), RequestId);
}

FGloamsteadSurveySubjectReport UGloamsteadSurveySubjectRegistry::Test_BuildReportFor(UWorld* World, const FString& RequestId) const
{
	return BuildReportFor(World, RequestId);
}

bool UGloamsteadSurveySubjectRegistry::EmitReport(const FString& RequestId, FString& OutPrimaryPath) const
{
	const FGloamsteadSurveySubjectReport Report = BuildReport(RequestId);
	const bool bOk = GloamsteadSurveySubjectReport::WriteReport(
		Report, GloamsteadSurveySubjectReport::DefaultReportDir(), OutPrimaryPath);
	if (!bOk)
	{
		UE_LOG(LogGloamstead, Error,
			TEXT("[GSS013] Survey subject report for request '%s' was NOT published to '%s'. ")
			TEXT("Gameplay is unaffected; the evidence trail is incomplete."),
			*Report.RequestId, *OutPrimaryPath);
	}
	return bOk;
}
