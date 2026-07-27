#include "Systems/GloamsteadSurveySubjectRegistry.h"
#include "Systems/VeilHeart.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "PCG/GloamsteadSanctuaryBootstrap.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"

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

	return Out;
}

const FGloamsteadSurveySubjectDeclaration* UGloamsteadSurveySubjectRegistry::FindDeclaration(FName SubjectId)
{
	static const TArray<FGloamsteadSurveySubjectDeclaration> Declarations = GetDeclarations();
	return Declarations.FindByPredicate(
		[SubjectId](const FGloamsteadSurveySubjectDeclaration& D) { return D.SubjectId == SubjectId; });
}

bool UGloamsteadSurveySubjectRegistry::ResolveSubjectIn(UWorld* World, FName SubjectId, FGloamsteadSurveySubject& Out) const
{
	Out = FGloamsteadSurveySubject();
	Out.SubjectId = SubjectId;

	const FGloamsteadSurveySubjectDeclaration* Decl = FindDeclaration(SubjectId);
	if (!Decl || !Decl->ActorClass)
	{
		// An undeclared place-name is unresolved, not an invitation to search for something similar.
		Out.FailureCodes.AddUnique(TEXT("GSS001"));
		return false;
	}

	if (!World)
	{
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

FGloamsteadSurveySubjectReport UGloamsteadSurveySubjectRegistry::BuildReportFor(UWorld* World) const
{
	FGloamsteadSurveySubjectReport R;
	R.ReportId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	R.CreatedAt = FDateTime::UtcNow().ToIso8601();
	// The map is read from the world we were handed. It is never defaulted — a report that cannot say
	// which map it describes is not evidence.
	R.MapName = World ? World->GetMapName() : FString();

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

FGloamsteadSurveySubjectReport UGloamsteadSurveySubjectRegistry::BuildReport() const
{
	return BuildReportFor(GetWorld());
}

FGloamsteadSurveySubjectReport UGloamsteadSurveySubjectRegistry::Test_BuildReportFor(UWorld* World) const
{
	return BuildReportFor(World);
}

bool UGloamsteadSurveySubjectRegistry::EmitReport(FString& OutPrimaryPath) const
{
	const FGloamsteadSurveySubjectReport Report = BuildReport();
	return GloamsteadSurveySubjectReport::WriteReport(
		Report, GloamsteadSurveySubjectReport::DefaultReportDir(), OutPrimaryPath);
}
