// Hostile tests for the first-lantern tutorial loop (Lane 7 — independent critic).
//
// These attack the loop from the outside: each one asks "what does the player see when this step is
// missing, duplicated, dead, or unwritable?" and asserts that the failing step yields NOTHING rather
// than something plausible-looking. ONE failure class per test.
//
// Two shapes, matching the house patterns:
//   * worldless  — NewObject'd subsystems + the documented Test_ seams (see RestorationLifecycleTests.cpp)
//   * LIVE world — a real UWorld so world subsystems exist and dynamic multicasts actually dispatch
//                  (see SurveySubjectRegistryTests.cpp / PlayableCycleTests.cpp)
//
// Deliberately NOT covered here, with reasons, at the bottom of the file.
#include "Misc/AutomationTest.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Components/GloamsteadSurveySubjectComponent.h"
#include "Systems/GloamsteadSurveySubjectRegistry.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/VeilHeart.h"
#include "Data/GloamsteadSurveySubjectTypes.h"
#include "Data/RitualTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

// A NAMED namespace, not an anonymous one. Anonymous namespaces do not keep helper names apart once
// files land in the same unity translation unit — the same C2264 hazard documented at
// GloamsteadSurveySubjectComponent.cpp:13-15. Every helper below is reachable only as
// GloamLoopHostile::X, so it cannot collide with the identically-shaped helpers in
// RestorationLifecycleTests.cpp:17 or PCGSubsystemTests.cpp:7.
namespace GloamLoopHostile
{
	/**
	 * A real game world, torn down on scope exit.
	 *
	 * Scoped rather than hand-unwound because several tests below assert-and-bail; a manual teardown at
	 * the bottom of the function is skipped on every early return, and a leaked UWorld poisons whatever
	 * automation test runs next.
	 */
	struct FScopedGameWorld
	{
		UWorld* World = nullptr;

		FScopedGameWorld()
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

		~FScopedGameWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
		}

		FScopedGameWorld(const FScopedGameWorld&) = delete;
		FScopedGameWorld& operator=(const FScopedGameWorld&) = delete;
	};

	/** Clean point states; index 0 is the usual target. */
	inline TArray<FRitualPointState> MakePoints(int32 Count)
	{
		TArray<FRitualPointState> States;
		States.SetNum(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			States[i].LightLevel = 0.10f;
			States[i].CorruptionLevel = 0.60f;
		}
		return States;
	}

	inline FRestorationEventPayload MakePayload(int32 PointIndex, float LightDelta, float CorruptionCleared)
	{
		FRestorationEventPayload P;
		P.PointIndex = PointIndex;
		P.RitualType = ERitualType::LanternPost;
		P.LightDelta = LightDelta;
		P.CorruptionCleared = CorruptionCleared;
		return P;
	}

	/**
	 * Attach a survey-subject component to Owner WITHOUT registering it as an engine component.
	 *
	 * NewObject with the actor as outer is enough for the registry's purposes: UActorComponent's owner
	 * is derived from its outer in PostInitProperties, so GetOwner() is valid immediately and
	 * RegisterSubjectComponent (GloamsteadSurveySubjectRegistry.cpp:95-101) accepts it. Skipping
	 * RegisterComponent() is deliberate — it keeps BeginPlay/EndPlay out of the picture so the tests
	 * below drive registration explicitly and can model a claim that outlives its hooks.
	 */
	inline UGloamsteadSurveySubjectComponent* MakeClaimant(AActor* Owner, const TCHAR* SubjectId)
	{
		UGloamsteadSurveySubjectComponent* Comp = NewObject<UGloamsteadSurveySubjectComponent>(Owner);
		Comp->SubjectId = FName(SubjectId);
		Comp->bRegisterOnBeginPlay = false;
		return Comp;
	}

	/**
	 * A request record that passes GSSValidateRequest cleanly, so a test can isolate the WRITE layer.
	 * Unresolved + no coordinates: GSS001 is an honest outcome and is filtered out of the blocking set
	 * (GloamsteadSurveySubjectTypes.cpp:466-471), so this record is publishable.
	 */
	inline FGloamsteadSurveyRequest MakeWritableRequest(const FString& RequestId, const TCHAR* SubjectId)
	{
		FGloamsteadSurveyRequest R;
		R.RequestId = RequestId;
		R.SubjectId = FName(SubjectId);
		R.SchemaVersion = GSSRequestSchemaVersion();
		R.ProducerVersion = GSSProducerVersion();
		R.CreatedAt = TEXT("2026-07-30T00:00:00Z");
		R.GitSha = TEXT("0000000000000000000000000000000000000000");
		R.MapName = TEXT("GloamLoopHostileMap");
		R.MapPackageName = TEXT("/Game/Maps/GloamLoopHostileMap");
		R.WorldInstanceId = TEXT("/Game/Maps/GloamLoopHostileMap.GloamLoopHostileMap");
		R.Status = EGSSRequestStatus::Unresolved;
		R.FailureCodes.Add(TEXT("GSS001"));
		return R;
	}

	/** A scratch directory under Saved/, emptied on construction and on scope exit. */
	struct FScopedScratchDir
	{
		FString Path;

		explicit FScopedScratchDir(const TCHAR* Leaf)
		{
			Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GloamLoopHostile"), Leaf);
			Wipe();
		}

		~FScopedScratchDir() { Wipe(); }

		void Wipe() const
		{
			IFileManager& FM = IFileManager::Get();
			if (FM.DirectoryExists(*Path))
			{
				FM.DeleteDirectory(*Path, /*RequireExists*/ false, /*Tree*/ true);
			}
			// A blocking file may occupy the same name (see the unwritable-directory test).
			FM.Delete(*Path, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
		}

		FScopedScratchDir(const FScopedScratchDir&) = delete;
		FScopedScratchDir& operator=(const FScopedScratchDir&) = delete;
	};

	// Error substrings the artifact writer logs. An undeclared Error fails an automation test, so every
	// test that deliberately trips one of these must declare it first.
	static const TCHAR* const LogUnwritableDir = TEXT("[GSS013] Survey evidence NOT written: cannot create output directory");
	static const TCHAR* const LogRequestIdCollision = TEXT("[GSS012] Survey request id");
	static const TCHAR* const LogInternallyInvalid = TEXT("is internally invalid and was NOT written");
}

// ===== Failure class: the sanctuary was never initialized from PCG, and placement is attempted anyway =====
// InitializeFromPCGComponent is what fills PointStates (GloamsteadPCGSubsystem.cpp:94-101). If PCG has
// not generated — or generated nothing — every index is out of range and ApplyRestoration's first guard
// (GloamsteadPCGSubsystem.cpp:276) refuses. The player-visible failure this pins is the WRONG outcome:
// a lantern that reports "placed" against a sanctuary that has no points to place it in.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopUninitializedSanctuaryTest,
	"Gloamstead.Loop.UninitializedSanctuaryAcceptsNoRestoration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopUninitializedSanctuaryTest::RunTest(const FString& /*Parameters*/)
{
	// Never initialized: no PCG component was ever handed in, so there are no points at all.
	UGloamsteadPCGSubsystem* Sub = NewObject<UGloamsteadPCGSubsystem>();

	TestEqual(TEXT("an uninitialized sanctuary has no ritual points"), Sub->GetRitualPointCount(), 0);

	// The query the placement preview runs every tick must find nothing rather than index into nothing.
	// The radius is left at the production default: FindNearestUnrestoredPointIndex walks a cube of grid
	// cells of side (2*ceil(Radius/CellSize)+3) (GloamsteadPCGSubsystem.cpp:158-166), so an inflated
	// "search everywhere" radius here would cost hundreds of millions of map lookups and hang the run.
	TestEqual(TEXT("no lantern point is offered anywhere"),
		Sub->FindNearestUnrestoredPointIndex(FVector::ZeroVector, ERitualType::LanternPost, 1600.f), -1);
	FPCGPoint Unused;
	TestFalse(TEXT("point 0 cannot be read"), Sub->GetPointByIndex(0, Unused));

	// A well-formed payload aimed at point 0 is still refused: there is no point 0.
	TestFalse(TEXT("restoration is refused before initialization"),
		Sub->ApplyRestoration(0, GloamLoopHostile::MakePayload(0, 0.35f, 0.20f)));

	// And the refusal mutated nothing into existence.
	TestEqual(TEXT("still no ritual points"), Sub->GetRitualPointCount(), 0);
	TestEqual(TEXT("still no point states"), Sub->Test_PeekPointStates().Num(), 0);
	TestEqual(TEXT("nothing is counted as restored"), Sub->GetRestoredPointCount(), 0);
	TestEqual(TEXT("the restored set is empty"), Sub->GetRestoredPointIndices().Num(), 0);
	TestFalse(TEXT("point 0 does not read as restored"), Sub->IsPointRestored(0));
	TestEqual(TEXT("light is the safe default, not a phantom value"), Sub->GetLightLevel(0), 0.f, KINDA_SMALL_NUMBER);
	return true;
}

// ===== Failure class: the lantern subject is declared but no map actor has claimed it =====
// courtyard.lantern.first is declared with ResolverKind::RegisteredComponent and NO ActorClass
// (GloamsteadSurveySubjectRegistry.cpp:52-61), so the only thing that can resolve it is a live actor
// naming itself. With nothing registered it must come back unresolved and CARRYING NO COORDINATES —
// not the Heart's, not the world origin's. A guessed origin would send the tutorial's "go here" marker
// to (0,0,0) and read as a placed lantern to anything downstream.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopUnregisteredLanternTest,
	"Gloamstead.Loop.UnregisteredLanternSubjectNeverResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopUnregisteredLanternTest::RunTest(const FString& /*Parameters*/)
{
	GloamLoopHostile::FScopedGameWorld Scoped;
	if (!TestNotNull(TEXT("live world created"), Scoped.World))
	{
		return false;
	}

	UGloamsteadSurveySubjectRegistry* Registry = Scoped.World->GetSubsystem<UGloamsteadSurveySubjectRegistry>();
	if (!TestNotNull(TEXT("registry subsystem present"), Registry))
	{
		return false;
	}

	// A populated world: something IS placed and resolvable, so an unresolved lantern below cannot be
	// dismissed as "resolution is broken in this world". This is the anti-vacuous-pass control.
	AVeilHeart* Heart = Scoped.World->SpawnActor<AVeilHeart>(FVector(700.f, -300.f, 40.f), FRotator::ZeroRotator);
	TestNotNull(TEXT("a Heart is placed in the world"), Heart);

	FGloamsteadSurveySubject Control;
	TestTrue(TEXT("control: the placed Heart does resolve"),
		Registry->Test_ResolveSubjectIn(Scoped.World, TEXT("sanctuary.heart"), Control));

	// The declaration exists...
	TestNotNull(TEXT("the first lantern is a declared place-name"),
		UGloamsteadSurveySubjectRegistry::FindDeclaration(TEXT("courtyard.lantern.first")));
	// ...but nothing has claimed it.
	TestFalse(TEXT("no component claims the first lantern"),
		Registry->IsSubjectRegistered(TEXT("courtyard.lantern.first")));
	TestNull(TEXT("no component is handed back for the first lantern"),
		Registry->GetRegisteredComponent(TEXT("courtyard.lantern.first")));

	FGloamsteadSurveySubject Lantern;
	TestFalse(TEXT("the unclaimed lantern does not resolve"),
		Registry->Test_ResolveSubjectIn(Scoped.World, TEXT("courtyard.lantern.first"), Lantern));
	TestFalse(TEXT("the unclaimed lantern is not marked resolved"), Lantern.bLocationResolved);
	TestTrue(TEXT("the unclaimed lantern reports GSS001"), Lantern.FailureCodes.Contains(TEXT("GSS001")));
	TestEqual(TEXT("the unclaimed lantern saw no candidates"), Lantern.CandidateCount, 0);

	// The never-guess rail: no coordinates of any kind came back.
	TestTrue(TEXT("the unclaimed lantern returns no object path"), Lantern.ActorObjectPath.IsEmpty());
	TestTrue(TEXT("the unclaimed lantern returns no class name"), Lantern.ResolvedClassName.IsEmpty());
	TestTrue(TEXT("the unclaimed lantern returns identity, not the world origin as a value"),
		Lantern.Transform.Equals(FTransform::Identity));
	TestTrue(TEXT("the unclaimed lantern claims no anchor mode"), Lantern.AnchorMode == EGSSAnchorMode::None);
	TestTrue(TEXT("the unclaimed lantern claims no resolver"), Lantern.ResolverKind == EGSSResolverKind::None);

	// And crucially it did not borrow the one thing that IS placed.
	if (Heart)
	{
		TestNotEqual(TEXT("the lantern did not answer with the Heart's path"),
			Lantern.ActorObjectPath, Heart->GetPathName());
	}
	return true;
}

// ===== Failure class: a place-name this project never declared is asked about =====
// A distinct branch from the test above: that one is declared-but-unclaimed
// (GloamsteadSurveySubjectRegistry.cpp:252-257); this one never appears in the declaration table at all
// (GloamsteadSurveySubjectRegistry.cpp:244-250). A typo'd or renamed id must resolve to nothing rather
// than fuzzy-match a neighbour — the tutorial would otherwise point the player at the wrong object.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopUndeclaredNameTest,
	"Gloamstead.Loop.UndeclaredLanternNameNeverResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopUndeclaredNameTest::RunTest(const FString& /*Parameters*/)
{
	GloamLoopHostile::FScopedGameWorld Scoped;
	if (!TestNotNull(TEXT("live world created"), Scoped.World))
	{
		return false;
	}

	UGloamsteadSurveySubjectRegistry* Registry = Scoped.World->GetSubsystem<UGloamsteadSurveySubjectRegistry>();
	if (!TestNotNull(TEXT("registry subsystem present"), Registry))
	{
		return false;
	}

	// A near-miss of a real declared id: same prefix, same shape, one word different.
	const TCHAR* Typo = TEXT("courtyard.lantern.second");
	TestNull(TEXT("the near-miss id is genuinely undeclared"),
		UGloamsteadSurveySubjectRegistry::FindDeclaration(Typo));

	// Place a real actor that a fuzzy resolver would happily reach for.
	TestNotNull(TEXT("a Heart is placed in the world"),
		Scoped.World->SpawnActor<AVeilHeart>(FVector(1500.f, 250.f, 0.f), FRotator::ZeroRotator));

	FGloamsteadSurveySubject Out;
	TestFalse(TEXT("an undeclared place-name does not resolve"),
		Registry->Test_ResolveSubjectIn(Scoped.World, Typo, Out));
	TestFalse(TEXT("an undeclared place-name is not marked resolved"), Out.bLocationResolved);
	TestTrue(TEXT("an undeclared place-name reports GSS001"), Out.FailureCodes.Contains(TEXT("GSS001")));
	TestTrue(TEXT("an undeclared place-name returns no object path"), Out.ActorObjectPath.IsEmpty());
	TestTrue(TEXT("an undeclared place-name returns identity"), Out.Transform.Equals(FTransform::Identity));
	TestEqual(TEXT("an undeclared place-name saw no candidates"), Out.CandidateCount, 0);
	// The subject id is echoed back so the caller can tell which question was refused.
	TestEqual(TEXT("the refusal names the id that was asked about"), Out.SubjectId.ToString(), FString(Typo));
	return true;
}

// ===== Failure class: two live map actors claim the same place-name =====
// Ambiguity never resolves. The FIRST claim stands and the second is refused with GSS007
// (GloamsteadSurveySubjectRegistry.cpp:123-132) — the hostile half is that the refusal must not
// half-apply: the incumbent must still own the id and must still be the actor resolution answers with.
// A silent override would make "the first lantern" mean a different object depending on load order.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopDuplicateLanternClaimTest,
	"Gloamstead.Loop.SecondLiveClaimOnTheLanternIsRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopDuplicateLanternClaimTest::RunTest(const FString& /*Parameters*/)
{
	GloamLoopHostile::FScopedGameWorld Scoped;
	if (!TestNotNull(TEXT("live world created"), Scoped.World))
	{
		return false;
	}

	UGloamsteadSurveySubjectRegistry* Registry = Scoped.World->GetSubsystem<UGloamsteadSurveySubjectRegistry>();
	if (!TestNotNull(TEXT("registry subsystem present"), Registry))
	{
		return false;
	}

	const FVector FirstLocation(400.f, 0.f, 0.f);
	const FVector SecondLocation(-900.f, 120.f, 30.f);

	// AVeilHeart rather than a bare AActor: a plain AActor has no root component, so its transform
	// reads back as identity and a "resolution returned the right actor's transform" assertion would be
	// vacuous. The Heart carries a root (VeilHeart.cpp:16-17). Two of them make sanctuary.heart
	// ambiguous, which is irrelevant here — component registration is path 1 and never consults the
	// declared-class table (GloamsteadSurveySubjectRegistry.cpp:205-241).
	AActor* FirstActor = Scoped.World->SpawnActor<AVeilHeart>(FirstLocation, FRotator::ZeroRotator);
	AActor* SecondActor = Scoped.World->SpawnActor<AVeilHeart>(SecondLocation, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("first claimant spawned"), FirstActor) ||
		!TestNotNull(TEXT("second claimant spawned"), SecondActor))
	{
		return false;
	}

	UGloamsteadSurveySubjectComponent* First =
		GloamLoopHostile::MakeClaimant(FirstActor, TEXT("courtyard.lantern.first"));
	UGloamsteadSurveySubjectComponent* Second =
		GloamLoopHostile::MakeClaimant(SecondActor, TEXT("courtyard.lantern.first"));

	TArray<FString> FirstCodes;
	TestTrue(TEXT("the first claim is accepted"), Registry->RegisterSubjectComponent(First, FirstCodes));
	TestEqual(TEXT("a clean claim reports no codes"), FirstCodes.Num(), 0);

	// The second, equally live, claim on the same id.
	TArray<FString> SecondCodes;
	TestFalse(TEXT("a second live claim on the same place-name is refused"),
		Registry->RegisterSubjectComponent(Second, SecondCodes));
	TestTrue(TEXT("the refusal is reported as GSS007"), SecondCodes.Contains(TEXT("GSS007")));

	// The refusal did not disturb the incumbent.
	TestTrue(TEXT("the id is still registered"), Registry->IsSubjectRegistered(TEXT("courtyard.lantern.first")));
	TestTrue(TEXT("the FIRST component still owns the id"),
		Registry->GetRegisteredComponent(TEXT("courtyard.lantern.first")) == First);
	TestFalse(TEXT("the refused component owns nothing"),
		Registry->GetRegisteredComponent(TEXT("courtyard.lantern.first")) == Second);

	const TArray<FName> RegisteredIds = Registry->GetRegisteredSubjectIds();
	int32 ClaimsOnTheLantern = 0;
	for (const FName& Id : RegisteredIds)
	{
		if (Id == FName(TEXT("courtyard.lantern.first"))) { ++ClaimsOnTheLantern; }
	}
	TestEqual(TEXT("exactly one claim exists for the id"), ClaimsOnTheLantern, 1);

	// And resolution still answers with the incumbent's actor, not the interloper's.
	FGloamsteadSurveySubject Resolved;
	TestTrue(TEXT("the lantern still resolves"),
		Registry->Test_ResolveSubjectIn(Scoped.World, TEXT("courtyard.lantern.first"), Resolved));
	TestEqual(TEXT("resolution names the FIRST actor"), Resolved.ActorObjectPath, FirstActor->GetPathName());
	TestNotEqual(TEXT("resolution does not name the refused actor"), Resolved.ActorObjectPath, SecondActor->GetPathName());
	TestTrue(TEXT("resolution returns the FIRST actor's transform"),
		Resolved.Transform.GetLocation().Equals(FirstLocation, 0.01f));
	TestEqual(TEXT("resolution stays unambiguous"), Resolved.CandidateCount, 1);
	return true;
}

// ===== Failure class: the claimed object dies and the claim is left behind =====
// The registry's whole reason for holding WEAK references (GloamsteadSurveySubjectRegistry.h:19-23) is
// that a claim can outlive its object. When it does, the answer must be "gone" — never the coordinates
// the caller saw a moment ago, and never a DIFFERENT actor found by the class table
// (GloamsteadSurveySubjectRegistry.cpp:224-230 refuses exactly that fallback).
//
// This is the strongest available shape: the dead claimant holds "sanctuary.heart", and a real, live
// AVeilHeart is placed in the same world — so the class-table fallback has something to find. If that
// fallback ran, the answer would be a confident statement about the WRONG object (or a GSS007
// ambiguity), never the bare GSS008 the contract requires. Both are detectable, and both are asserted
// against below.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopStaleClaimTest,
	"Gloamstead.Loop.StaleClaimNeverFallsBackToAnotherActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopStaleClaimTest::RunTest(const FString& /*Parameters*/)
{
	GloamLoopHostile::FScopedGameWorld Scoped;
	if (!TestNotNull(TEXT("live world created"), Scoped.World))
	{
		return false;
	}

	UGloamsteadSurveySubjectRegistry* Registry = Scoped.World->GetSubsystem<UGloamsteadSurveySubjectRegistry>();
	if (!TestNotNull(TEXT("registry subsystem present"), Registry))
	{
		return false;
	}

	// The real Heart the class table WOULD find if the fallback ever ran.
	const FVector HeartLocation(2500.f, 2500.f, 0.f);
	AVeilHeart* Heart = Scoped.World->SpawnActor<AVeilHeart>(HeartLocation, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("a real Heart is placed"), Heart))
	{
		return false;
	}

	// A different actor claims the id explicitly. A component registration outranks the class table
	// (GloamsteadSurveySubjectRegistry.cpp:205-241), so this is the object the id now means.
	// Spawned as an AVeilHeart because a bare AActor has no root component and would report an identity
	// transform, making the "it carried the claimant's transform" baseline below vacuous.
	const FVector ClaimantLocation(-1000.f, 50.f, 10.f);
	AActor* Claimant = Scoped.World->SpawnActor<AVeilHeart>(ClaimantLocation, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("claimant spawned"), Claimant))
	{
		return false;
	}
	UGloamsteadSurveySubjectComponent* Comp = GloamLoopHostile::MakeClaimant(Claimant, TEXT("sanctuary.heart"));

	TArray<FString> Codes;
	if (!TestTrue(TEXT("the claim is accepted"), Registry->RegisterSubjectComponent(Comp, Codes)))
	{
		return false;
	}

	// Baseline: while it is alive, the id resolves to the CLAIMANT, not the Heart.
	FGloamsteadSurveySubject Live;
	TestTrue(TEXT("the live claim resolves"),
		Registry->Test_ResolveSubjectIn(Scoped.World, TEXT("sanctuary.heart"), Live));
	TestEqual(TEXT("it resolves to the claimant"), Live.ActorObjectPath, Claimant->GetPathName());
	TestTrue(TEXT("it carries the claimant's transform"), Live.Transform.GetLocation().Equals(ClaimantLocation, 0.01f));
	TestTrue(TEXT("it resolved via the registered component"), Live.ResolverKind == EGSSResolverKind::RegisteredComponent);

	// Now the claimed thing goes away WITHOUT the component's EndPlay/OnComponentDestroyed hooks running
	// — the exact case the header at GloamsteadSurveySubjectComponent.h:19-21 promises to survive.
	//
	// Deliberately NOT Destroy(): AActor::Destroy runs EndPlay on its components, which calls
	// UnregisterSubjectComponent and ERASES the claim. That is the clean-teardown path, and with the
	// claim gone the registry legitimately falls through to the declared-class table — which, because
	// this claimant is itself an AVeilHeart, resolves the OTHER Heart. That would make this test assert
	// the opposite of what its name says. MarkAsGarbage invalidates the weak pointers synchronously
	// while leaving the registration in place, which is the actual stale-claim state.
	Claimant->MarkAsGarbage();
	Comp->MarkAsGarbage();

	FGloamsteadSurveySubject Stale;
	TestFalse(TEXT("a dead claim does not resolve"),
		Registry->Test_ResolveSubjectIn(Scoped.World, TEXT("sanctuary.heart"), Stale));
	TestFalse(TEXT("a dead claim is not marked resolved"), Stale.bLocationResolved);
	TestTrue(TEXT("a dead claim is reported as GSS008"), Stale.FailureCodes.Contains(TEXT("GSS008")));
	TestTrue(TEXT("a dead claim is also reported as unresolved (GSS001)"), Stale.FailureCodes.Contains(TEXT("GSS001")));
	TestEqual(TEXT("a dead claim counts no candidates"), Stale.CandidateCount, 0);

	// It did not hand back what it said a moment ago.
	TestTrue(TEXT("a dead claim returns no object path"), Stale.ActorObjectPath.IsEmpty());
	TestTrue(TEXT("a dead claim returns identity, not the remembered transform"),
		Stale.Transform.Equals(FTransform::Identity));
	TestFalse(TEXT("a dead claim does not re-report the claimant's location"),
		Stale.Transform.GetLocation().Equals(ClaimantLocation, 0.01f));

	// And — the point of the test — it did NOT quietly answer with the live Heart instead.
	TestNotEqual(TEXT("a dead claim does not fall back to the placed Heart"),
		Stale.ActorObjectPath, Heart->GetPathName());
	TestFalse(TEXT("a dead claim does not return the Heart's location"),
		Stale.Transform.GetLocation().Equals(HeartLocation, 0.01f));
	// The class table was never consulted at all: reaching it would have produced GSS007 (two Hearts) or
	// a resolution (one Heart), never the bare GSS008 that path 1 returns at
	// GloamsteadSurveySubjectRegistry.cpp:227-230. This assertion holds regardless of when the engine
	// gets around to reaping the destroyed actor, which is what keeps the test timing-independent.
	TestFalse(TEXT("the declared-class fallback was never reached"), Stale.FailureCodes.Contains(TEXT("GSS007")));

	// The claim is dead but still auditable: it reads as unregistered, while remaining listed so the
	// stale entry is reportable rather than silently vanishing (GloamsteadSurveySubjectRegistry.h:90).
	TestFalse(TEXT("a dead claim does not read as registered"), Registry->IsSubjectRegistered(TEXT("sanctuary.heart")));
	TestNull(TEXT("a dead claim hands back no component pointer"),
		Registry->GetRegisteredComponent(TEXT("sanctuary.heart")));
	TestTrue(TEXT("the dead claim is still listed for reporting"),
		Registry->GetRegisteredSubjectIds().Contains(FName(TEXT("sanctuary.heart"))));
	return true;
}

// ===== Failure class: restoring a point silently advances the day/night cycle =====
// The first day belongs to the scripted first-night director, which owns Day->Dusk and gates it on the
// lantern (GloamsteadFirstNightDirector.cpp:275-287). The DAY/NIGHT SUBSYSTEM must not develop its own
// opinion: CanRestNow reads only phase and NightCount (GloamsteadDayNightSubsystem.cpp:40-54), and none
// of the five live OnStructureRestored subscribers may move the cycle on their own.
//
// This runs in a LIVE world precisely so ApplyRestoration's Broadcast
// (GloamsteadPCGSubsystem.cpp:311) reaches real subscribers — a worldless run would prove nothing,
// because dynamic multicasts do not dispatch there. No director is placed, so nothing legitimate owns
// the transition; if the phase moves, something illegitimate did it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopRestorationDoesNotAdvanceCycleTest,
	"Gloamstead.Loop.RestorationAloneNeverAdvancesTheCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopRestorationDoesNotAdvanceCycleTest::RunTest(const FString& /*Parameters*/)
{
	GloamLoopHostile::FScopedGameWorld Scoped;
	if (!TestNotNull(TEXT("live world created"), Scoped.World))
	{
		return false;
	}

	UGloamsteadDayNightSubsystem* DayNight = Scoped.World->GetSubsystem<UGloamsteadDayNightSubsystem>();
	UGloamsteadPCGSubsystem* PCG = Scoped.World->GetSubsystem<UGloamsteadPCGSubsystem>();
	if (!TestNotNull(TEXT("day/night subsystem present"), DayNight) ||
		!TestNotNull(TEXT("PCG subsystem present"), PCG))
	{
		return false;
	}

	PCG->Test_SeedPointStates(GloamLoopHostile::MakePoints(3));

	// Opening state: day one, no night has passed, and the tutorial gate is shut.
	TestTrue(TEXT("the cycle opens in Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
	TestEqual(TEXT("no night has been counted yet"), DayNight->GetNightCount(), 0);
	TestFalse(TEXT("rest is refused before the first night"), DayNight->CanRestNow());

	// The player lights the first lantern. This really broadcasts to every live subscriber.
	TestTrue(TEXT("the first lantern is restored"),
		PCG->ApplyRestoration(0, GloamLoopHostile::MakePayload(0, 0.35f, 0.20f)));
	TestTrue(TEXT("the point really did take"), PCG->IsPointRestored(0));
	TestEqual(TEXT("exactly one point is restored"), PCG->GetRestoredPointCount(), 1);

	// Nothing about the cycle moved on its own.
	TestTrue(TEXT("the phase is still Day"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
	TestEqual(TEXT("no night was counted by a restoration"), DayNight->GetNightCount(), 0);
	TestFalse(TEXT("restoring a point does not open the first-day rest gate"), DayNight->CanRestNow());
	TestFalse(TEXT("rest is still refused"), DayNight->RequestRest());
	TestTrue(TEXT("the refused rest left the phase alone"), DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Day);
	TestEqual(TEXT("the refused rest counted no night"), DayNight->GetNightCount(), 0);

	// The restoration itself is untouched by the refused rest.
	TestTrue(TEXT("the lantern is still lit"), PCG->IsPointRestored(0));
	TestEqual(TEXT("the restored count is unchanged"), PCG->GetRestoredPointCount(), 1);
	return true;
}

// ===== Failure class: the evidence directory cannot be created =====
// The artifact writer must fail CLOSED and say so: an unwritable output directory produces GSS013 and
// no file, never a silent success (GloamsteadSurveySubjectTypes.cpp:301-312). Silent success is the
// dangerous shape — a wave gate that greps for "did the artifact write return true" would pass while
// nothing reached disk.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopUnwritableEvidenceDirTest,
	"Gloamstead.Loop.UnwritableEvidenceDirectoryFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopUnwritableEvidenceDirTest::RunTest(const FString& /*Parameters*/)
{
	AddExpectedErrorPlain(GloamLoopHostile::LogUnwritableDir, EAutomationExpectedErrorFlags::Contains, 1);

	GloamLoopHostile::FScopedScratchDir Scratch(TEXT("UnwritableDir"));
	IFileManager& FM = IFileManager::Get();

	// Occupy the output directory's name with a regular FILE. MakeDirectory cannot then create it, and
	// the writer's DirectoryExists probe stays false — a deterministic, permission-free way to make the
	// destination genuinely unusable.
	FM.MakeDirectory(*FPaths::GetPath(Scratch.Path), /*Tree*/ true);
	if (!TestTrue(TEXT("the blocking file was created"),
		FFileHelper::SaveStringToFile(FString(TEXT("not a directory")), *Scratch.Path)))
	{
		return false;
	}
	TestFalse(TEXT("the output directory does not exist"), FM.DirectoryExists(*Scratch.Path));

	const FGloamsteadSurveyRequest Request =
		GloamLoopHostile::MakeWritableRequest(TEXT("gloam-loop-unwritable"), TEXT("courtyard.lantern.first"));

	FString OutPath;
	TArray<FString> Codes;
	TestFalse(TEXT("publishing into an unusable directory fails"),
		GloamsteadSurveySubjectReport::WriteRequest(Request, Scratch.Path, OutPath, Codes));
	TestTrue(TEXT("the failure is reported as GSS013"), Codes.Contains(TEXT("GSS013")));

	// The writer still names the path it intended to file under (assigned before the write is attempted,
	// GloamsteadSurveySubjectTypes.cpp:480), so a caller can report WHERE the evidence should have gone.
	TestFalse(TEXT("the writer still names the intended path"), OutPath.IsEmpty());

	// Nothing was left behind: no artifact, and no half-written temp file next to it. The filter is
	// scoped to THIS request id (the temp name is <final>.<guid>.tmp, GloamsteadSurveySubjectTypes.cpp:315)
	// so a sibling scratch test running in the same parent cannot make this pass or fail spuriously.
	TestFalse(TEXT("no artifact reached disk"), FPaths::FileExists(OutPath));
	TArray<FString> Leftovers;
	FM.FindFilesRecursive(Leftovers, *FPaths::GetPath(Scratch.Path), TEXT("gloam-loop-unwritable*.tmp"),
		/*Files*/ true, /*Directories*/ false);
	TestEqual(TEXT("no temp artifact was left behind"), Leftovers.Num(), 0);

	// The blocking file is untouched — a failed write never clobbers what is already there.
	TestTrue(TEXT("the pre-existing file is still there"), FPaths::FileExists(Scratch.Path));
	return true;
}

// ===== Failure class: a second, different record is filed under a request id already in use =====
// Overwriting would silently destroy the earlier evidence, so the writer refuses with GSS012 and
// leaves the original bytes alone (GloamsteadSurveySubjectTypes.cpp:492-507). Re-emitting the identical
// record stays idempotent, which is what makes the refusal safe to enforce.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopDuplicateRequestIdTest,
	"Gloamstead.Loop.DuplicateRequestIdNeverOverwritesEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopDuplicateRequestIdTest::RunTest(const FString& /*Parameters*/)
{
	AddExpectedErrorPlain(GloamLoopHostile::LogRequestIdCollision, EAutomationExpectedErrorFlags::Contains, 1);

	GloamLoopHostile::FScopedScratchDir Scratch(TEXT("DuplicateRequestId"));
	const FString RequestId = TEXT("gloam-loop-duplicate-id");

	const FGloamsteadSurveyRequest Original =
		GloamLoopHostile::MakeWritableRequest(RequestId, TEXT("courtyard.lantern.first"));

	FString FirstPath;
	TArray<FString> FirstCodes;
	if (!TestTrue(TEXT("the first record publishes"),
		GloamsteadSurveySubjectReport::WriteRequest(Original, Scratch.Path, FirstPath, FirstCodes)))
	{
		return false;
	}
	TestEqual(TEXT("a clean publish reports no codes"), FirstCodes.Num(), 0);
	TestTrue(TEXT("the artifact is on disk"), FPaths::FileExists(FirstPath));

	// Re-emitting the identical record is idempotent, not a collision.
	FString RepeatPath;
	TArray<FString> RepeatCodes;
	TestTrue(TEXT("re-emitting the identical record succeeds"),
		GloamsteadSurveySubjectReport::WriteRequest(Original, Scratch.Path, RepeatPath, RepeatCodes));
	TestEqual(TEXT("an idempotent re-emit reports no codes"), RepeatCodes.Num(), 0);
	TestEqual(TEXT("an idempotent re-emit files to the same path"), RepeatPath, FirstPath);

	// A DIFFERENT record under the SAME id. Same request id, different subject: this is a different
	// question, and filing it here would erase the answer to the first one.
	FGloamsteadSurveyRequest Impostor = Original;
	Impostor.SubjectId = FName(TEXT("sanctuary.heart"));

	FString ImpostorPath;
	TArray<FString> ImpostorCodes;
	TestFalse(TEXT("a different record under a taken id is refused"),
		GloamsteadSurveySubjectReport::WriteRequest(Impostor, Scratch.Path, ImpostorPath, ImpostorCodes));
	TestTrue(TEXT("the refusal is reported as GSS012"), ImpostorCodes.Contains(TEXT("GSS012")));

	// The original evidence survived, byte for byte. Two independent proofs:
	//  (a) the artifact still parses as the ORIGINAL subject, and
	//  (b) re-emitting the original still takes the idempotent path, which only compares equal if the
	//      on-disk bytes were never touched.
	FString Json;
	if (TestTrue(TEXT("the original artifact is still readable"), FFileHelper::LoadFileToString(Json, *FirstPath)))
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (TestTrue(TEXT("the surviving artifact is valid JSON"),
			FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
		{
			FString SubjectId;
			TestTrue(TEXT("the surviving artifact names a subject"), Root->TryGetStringField(TEXT("subject_id"), SubjectId));
			TestEqual(TEXT("the surviving artifact is still the ORIGINAL subject"),
				SubjectId, FString(TEXT("courtyard.lantern.first")));
		}
	}

	FString ProofPath;
	TArray<FString> ProofCodes;
	TestTrue(TEXT("the original record still re-emits idempotently (its bytes are unchanged)"),
		GloamsteadSurveySubjectReport::WriteRequest(Original, Scratch.Path, ProofPath, ProofCodes));
	TestEqual(TEXT("the idempotent proof reports no codes"), ProofCodes.Num(), 0);
	return true;
}

// ===== Failure class: a record claims a resolution it cannot evidence =====
// This is the "the report says success while the thing never happened" shape, at the only layer where
// it is currently expressible. GSSValidateRequest requires a Resolved record to carry an object path
// and a concrete class (GloamsteadSurveySubjectTypes.cpp:146-155), and WriteRequest refuses to publish
// anything that fails its own contract (:466-478) — a malformed artifact on disk is worse than none,
// because it LOOKS like evidence.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopOverclaimingRequestTest,
	"Gloamstead.Loop.RequestClaimingResolutionWithoutEvidenceIsRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopOverclaimingRequestTest::RunTest(const FString& /*Parameters*/)
{
	AddExpectedErrorPlain(GloamLoopHostile::LogInternallyInvalid, EAutomationExpectedErrorFlags::Contains, 1);

	GloamLoopHostile::FScopedScratchDir Scratch(TEXT("OverclaimingRequest"));
	const FString RequestId = TEXT("gloam-loop-overclaim");

	// "I found the first lantern" — with nothing whatsoever to show for it.
	FGloamsteadSurveyRequest Overclaim =
		GloamLoopHostile::MakeWritableRequest(RequestId, TEXT("courtyard.lantern.first"));
	Overclaim.Status = EGSSRequestStatus::Resolved;
	Overclaim.ResolverKind = EGSSResolverKind::RegisteredComponent;
	Overclaim.AnchorMode = EGSSAnchorMode::ActorObjectPath;
	Overclaim.ActorObjectPath = FString(); // the evidence that is missing
	Overclaim.ResolvedClassName = FString();
	Overclaim.FailureCodes.Reset();

	// The validator sees through it before anything reaches disk.
	const TArray<FString> Codes = GSSValidateRequest(Overclaim);
	TestTrue(TEXT("claiming resolution with no evidence trips GSS003"), Codes.Contains(TEXT("GSS003")));

	FString OutPath;
	TArray<FString> WriteCodes;
	TestFalse(TEXT("an overclaiming record is not published"),
		GloamsteadSurveySubjectReport::WriteRequest(Overclaim, Scratch.Path, OutPath, WriteCodes));
	TestTrue(TEXT("the refusal is reported as GSS003"), WriteCodes.Contains(TEXT("GSS003")));
	TestFalse(TEXT("GSS001 alone is not what blocked it"), WriteCodes.Contains(TEXT("GSS001")));

	// A refused write names no artifact and leaves none.
	TestTrue(TEXT("a refused write hands back no path"), OutPath.IsEmpty());
	const FString WouldBePath = GloamsteadSurveySubjectReport::RequestPath(Scratch.Path, RequestId);
	TestFalse(TEXT("the would-be path is computable"), WouldBePath.IsEmpty());
	TestFalse(TEXT("nothing was written at the would-be path"), FPaths::FileExists(WouldBePath));
	return true;
}

// ===== Failure class: a failed evidence write rolls back gameplay =====
// Reporting is downstream of gameplay and must never reach back into it. The registry states this
// contract twice (GloamsteadSurveySubjectRegistry.cpp:396-397, :403-404: "Gameplay is unaffected; the
// evidence trail is incomplete"), and it is the invariant most at risk the moment confirmation starts
// emitting evidence — the tempting fix for a failed write is to undo the restoration, which would take
// the player's lantern away because a disk write failed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamLoopEvidenceFailureNoRollbackTest,
	"Gloamstead.Loop.FailedEvidenceWriteNeverRollsBackRestoration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamLoopEvidenceFailureNoRollbackTest::RunTest(const FString& /*Parameters*/)
{
	AddExpectedErrorPlain(GloamLoopHostile::LogUnwritableDir, EAutomationExpectedErrorFlags::Contains, 1);

	GloamLoopHostile::FScopedGameWorld Scoped;
	if (!TestNotNull(TEXT("live world created"), Scoped.World))
	{
		return false;
	}

	UGloamsteadPCGSubsystem* PCG = Scoped.World->GetSubsystem<UGloamsteadPCGSubsystem>();
	UGloamsteadSurveySubjectRegistry* Registry = Scoped.World->GetSubsystem<UGloamsteadSurveySubjectRegistry>();
	if (!TestNotNull(TEXT("PCG subsystem present"), PCG) ||
		!TestNotNull(TEXT("registry subsystem present"), Registry))
	{
		return false;
	}

	PCG->Test_SeedPointStates(GloamLoopHostile::MakePoints(3));
	if (!TestTrue(TEXT("the lantern is restored"),
		PCG->ApplyRestoration(0, GloamLoopHostile::MakePayload(0, 0.35f, 0.20f))))
	{
		return false;
	}

	const float LightAfterRestore = PCG->GetLightLevel(0);
	const float CorruptionAfterRestore = PCG->GetCorruptionLevel(0);
	TestEqual(TEXT("the restoration added its light"), LightAfterRestore, 0.45f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("the restoration cleared its corruption"), CorruptionAfterRestore, 0.40f, KINDA_SMALL_NUMBER);

	// Now the evidence write fails, for a reason gameplay has no say in.
	GloamLoopHostile::FScopedScratchDir Scratch(TEXT("NoRollback"));
	IFileManager& FM = IFileManager::Get();
	FM.MakeDirectory(*FPaths::GetPath(Scratch.Path), /*Tree*/ true);
	if (!TestTrue(TEXT("the blocking file was created"),
		FFileHelper::SaveStringToFile(FString(TEXT("not a directory")), *Scratch.Path)))
	{
		return false;
	}

	const FGloamsteadSurveyRequest Request =
		Registry->Test_BuildRequestFor(Scoped.World, TEXT("courtyard.lantern.first"), TEXT("gloam-loop-no-rollback"));
	TestTrue(TEXT("the request carries this world's identity"), !Request.WorldInstanceId.IsEmpty());

	FString OutPath;
	TArray<FString> Codes;
	TestFalse(TEXT("the evidence write fails"),
		GloamsteadSurveySubjectReport::WriteRequest(Request, Scratch.Path, OutPath, Codes));
	TestTrue(TEXT("the write failure is reported as GSS013"), Codes.Contains(TEXT("GSS013")));

	// The player's lantern is exactly where they left it.
	TestTrue(TEXT("the point is still restored"), PCG->IsPointRestored(0));
	TestEqual(TEXT("its light was not taken back"), PCG->GetLightLevel(0), LightAfterRestore, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("its corruption was not restored"), PCG->GetCorruptionLevel(0), CorruptionAfterRestore, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("the restored count is unchanged"), PCG->GetRestoredPointCount(), 1);
	TestTrue(TEXT("the restored set still holds the index"), PCG->GetRestoredPointIndices().Contains(0));

	// And the two restored views still agree, so no partial unwind happened either.
	TestEqual(TEXT("restored flag and restored set still agree"),
		PCG->Test_PeekPointStates()[0].bIsRestored, PCG->GetRestoredPointIndices().Contains(0));
	return true;
}

// ===== Not covered here, and why =====
//
// * "missing ritual definition falls back rather than zeroing out" — already covered, and not worth a
//   second copy: Gloamstead.Restoration.MissingDefinitionFallsBackToTypeDefaults
//   (RestorationLifecycleTests.cpp:167-188) pins the non-zero type defaults that
//   BuildRestorationPayload seeds from. The UNCOVERED half is the opposite direction — a definition
//   asset that IS present overwrites those defaults unconditionally, with no validation, in
//   URitualPlacementComponent::BuildRestorationPayload (cited by symbol, not line: that file is being
//   edited by another lane this wave). So a DA_Ritual_* asset saved with DefaultLightContribution = 0
//   produces a restoration that consumes the point and adds no light.
//   Testing that needs the asset loaded through the editor, and RitualPlacementComponent is another
//   lane's file this wave. Recorded in the risk register instead.
//
// * "point already restored before placement" — covered twice already:
//   Gloamstead.Restoration.DoubleRestorationIsRejected and
//   Gloamstead.Restoration.BlueprintBypassOnRestoredPointIsRejected
//   (RestorationLifecycleTests.cpp:106-161) pin both the normal and the Blueprint-bypass entry.
//
// * "double confirm in one frame" — NOT REACHABLE without PIE, for a reason worth stating precisely:
//   URitualPlacementComponent::ConfirmPlacement bails unless IsCurrentPlacementValid(), which
//   needs a non-negative CurrentTargetPointIndex, which only ever comes from
//   FindNearestUnrestoredPointIndex. That query reads exclusively from SpatialGrid
//   (GloamsteadPCGSubsystem.cpp:157-185), and SpatialGrid is built only inside
//   InitializeFromPCGComponent (:111 -> BuildSpatialGrid at :622) — the test seam Test_SeedPoints
//   (:384-393) fills CachedPoints and does NOT build the grid. So no worldless or constructed-world
//   test can make placement produce a valid target. See the risk register: closing this needs a seam.
//
// * "cancel twice" / "cancel then confirm" mid-session — the never-entered branch is already covered by
//   Gloamstead.Restoration.CancelledPlacementLeavesNoPreviewState (RestorationLifecycleTests.cpp:348).
//   The mid-session branch IS reachable in a constructed live world (contrary to the note at
//   RestorationLifecycleTests.cpp:395-397) — see the risk register for the exact recipe — but it is not
//   written here: URitualPlacementComponent is owned by another lane this wave, and a test this lane
//   cannot rebuild against their edits is a test that fails the wave's gate for the wrong reason.
//
// * "report success with gameplay failure" — no such artifact exists to test. At the committed
//   baseline (2c3351b) the survey request pipeline made no gameplay claim at all and was unreachable
//   from a running game: UGloamsteadSurveySubjectRegistry::SurveyAndEmit and ::EmitReport had zero
//   callers anywhere in Source/. Another lane is wiring confirmation to evidence this wave, so the
//   direction that matters going forward is the inverse — that a failed report never unwinds a
//   successful restoration — which is pinned above by
//   Gloamstead.Loop.FailedEvidenceWriteNeverRollsBackRestoration. The nearest shape of the original
//   question, an artifact claiming a resolution it cannot evidence, is pinned by
//   Gloamstead.Loop.RequestClaimingResolutionWithoutEvidenceIsRefused.
//
// * "rest attempted before any restoration" / "repeated rest" — covered at the subsystem level by
//   Gloamstead.PlayableCycle.RestAdvancesRestingPhases (PlayableCycleTests.cpp:84-105) and in a live
//   world by Gloamstead.PlayableCycle.RestToDawnInLiveWorld. What was NOT covered is the coupling
//   question — whether restoring a point opens the first-day rest gate — which is why
//   Gloamstead.Loop.RestorationAloneNeverAdvancesTheCycle above runs in a live world with real
//   delegate dispatch rather than repeating the phase-walk.

#endif // WITH_DEV_AUTOMATION_TESTS
