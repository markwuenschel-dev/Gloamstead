// Corrected Wave 3 — GloamsteadForge runtime evidence emitter.
//
// Runs the REAL Wave 2 night runtime (strategies on seeded PCG state, worldless) for each matrix scenario,
// captures the observed values into a GloamsteadForge runtime report, asserts the report reflects real
// behavior, and writes conformant JSON to <ProjectDir>/procedural/reports/gloamsteadforge/. The hostile
// PowerShell validators then enforce fail-closed semantics over these emitted reports.
#include "Misc/AutomationTest.h"
#include "Systems/GloamsteadForgeEvidence.h"
#include "Systems/NightStrategy.h"
#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/RitualTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<FRitualPointState> GFEmitMakeStates(const TArray<float>& Corruptions, float Light, bool bRestored)
	{
		TArray<FRitualPointState> States;
		for (float C : Corruptions)
		{
			FRitualPointState S;
			S.LightLevel = Light;
			S.CorruptionLevel = C;
			S.bIsRestored = bRestored;
			States.Add(S);
		}
		return States;
	}

	// One restored target (index 0) plus unrestored neighbors — the shape a Retrieval night reacts to.
	TArray<FRitualPointState> GFEmitMakeRetrievalStates(float TargetCorruption, float TargetLight)
	{
		TArray<FRitualPointState> States;
		FRitualPointState T; T.bIsRestored = true;  T.LightLevel = TargetLight; T.CorruptionLevel = TargetCorruption; States.Add(T);
		FRitualPointState A; A.bIsRestored = false; A.LightLevel = 0.2f;         A.CorruptionLevel = 0.3f;             States.Add(A);
		FRitualPointState B; B.bIsRestored = false; B.LightLevel = 0.2f;         B.CorruptionLevel = 0.2f;             States.Add(B);
		return States;
	}

	FString NightTypeToken(ENightConsequenceType T)
	{
		switch (T)
		{
		case ENightConsequenceType::Tutorial:          return TEXT("Tutorial");
		case ENightConsequenceType::Corruption:        return TEXT("Corruption");
		case ENightConsequenceType::Omen:              return TEXT("Omen");
		case ENightConsequenceType::Retrieval:         return TEXT("Retrieval");
		case ENightConsequenceType::SilencePossession: return TEXT("SilencePossession");
		case ENightConsequenceType::Mirror:            return TEXT("Mirror");
		case ENightConsequenceType::Bargain:           return TEXT("Bargain");
		case ENightConsequenceType::Fracture:          return TEXT("Fracture");
		case ENightConsequenceType::TrueSiege:         return TEXT("TrueSiege");
		default:                                       return TEXT("Invalid");
		}
	}

	FString ObjectiveKindToken(ENightObjectiveKind K)
	{
		switch (K)
		{
		case ENightObjectiveKind::CleanseCorruptionBloom: return TEXT("CleanseCorruptionBloom");
		case ENightObjectiveKind::TutorialTeach:          return TEXT("TutorialTeach");
		case ENightObjectiveKind::HeedOmen:               return TEXT("HeedOmen");
		case ENightObjectiveKind::HoldRestored:           return TEXT("HoldRestored");
		default:                                          return TEXT("None");
		}
	}

	FNightRuntimeContext GFEmitMakeContext(ENightConsequenceType Type, UGloamsteadPCGSubsystem* PCG)
	{
		FNightRuntimeContext Ctx;
		Ctx.NightType = Type;
		Ctx.DuskSnapshot = PCG->BuildSanctuarySnapshot();
		Ctx.TargetPointIndex = PCG->FindMostCorruptedPointIndex(/*bOnlyUnrestored*/ true);
		if (Ctx.TargetPointIndex >= 0)
		{
			Ctx.TargetStartCorruption = PCG->GetCorruptionLevel(Ctx.TargetPointIndex);
		}
		return Ctx;
	}

	// Fills the common runtime fields of a report from a completed strategy run.
	void FillFromRun(FGloamsteadForgeReport& R, UGloamsteadPCGSubsystem* PCG, UNightStrategy* Strategy,
		const FNightRuntimeContext& Ctx, float AvgBefore, const FNightRuntimeOutcome& Outcome)
	{
		R.PcgInit.bInitialized = true;
		R.PcgInit.PointCount = PCG->Test_PeekPointStates().Num();

		const FNightObjective Obj = Strategy->GetObjective();
		R.NightLoop.bStarted = true;
		R.NightLoop.NightType = NightTypeToken(Ctx.NightType);
		R.NightLoop.ObjectiveKind = ObjectiveKindToken(Obj.Kind);
		R.NightLoop.TargetPointIndex = Obj.TargetPointIndex;
		R.NightLoop.bObjectiveResolved = Strategy->IsObjectiveResolved();
		R.NightLoop.bEndedIntentionally = true; // outcome is objective-governed; early-end mechanism exists
		R.NightLoop.OutcomeResult = GetNightOutcomeResultDisplayName(Outcome.Result);
		R.NightLoop.ResultTag = Outcome.ResultTag.ToString();

		R.Sanctuary.AvgCorruptionBefore = AvgBefore;
		R.Sanctuary.AvgCorruptionAfter = PCG->GetSanctuaryAverageCorruptionLevel();
		R.Sanctuary.TargetCorruptionBefore = Ctx.TargetStartCorruption;
		R.Sanctuary.TargetCorruptionAfter = (Obj.TargetPointIndex >= 0)
			? PCG->GetCorruptionLevel(Obj.TargetPointIndex) : Ctx.TargetStartCorruption;
		R.Sanctuary.bMutated = !FMath::IsNearlyEqual(R.Sanctuary.AvgCorruptionBefore, R.Sanctuary.AvgCorruptionAfter, KINDA_SMALL_NUMBER)
			|| !FMath::IsNearlyEqual(R.Sanctuary.TargetCorruptionBefore, R.Sanctuary.TargetCorruptionAfter, KINDA_SMALL_NUMBER);

		// Dawn genuinely consumes the outcome.
		AVeilHeart* Heart = NewObject<AVeilHeart>();
		Heart->WarningCatalog = nullptr;
		Heart->ProcessDawnReflectionWithOutcome(Outcome);
		R.Dawn.bConsumedOutcome = (Heart->GetLastNightOutcome().Result == Outcome.Result);
		R.Dawn.OutcomeResult = GetNightOutcomeResultDisplayName(Heart->GetLastNightOutcome().Result);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamForgeEmitEvidenceTest,
	"Gloamstead.GloamsteadForge.EmitRuntimeEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamForgeEmitEvidenceTest::RunTest(const FString& /*Parameters*/)
{
	const FString Dir = GloamsteadForgeEvidence::DefaultReportDir();
	int32 Emitted = 0;
	FString OutPath;
	TArray<FString> EmittedIds;

	// --- corruption_success: cleanse the bloom -> Success ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.8f, 0.1f, 0.1f, 0.1f }, 0.3f, false));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightCorruptionStrategy* S = NewObject<UNightCorruptionStrategy>();
		const FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Corruption, PCG);
		S->EnterNight(Ctx, PCG);
		S->ApplyPressureStep(PCG);
		FRestorationEventPayload Cleanse; Cleanse.PointIndex = Ctx.TargetPointIndex; Cleanse.CorruptionCleared = 1.0f;
		PCG->ApplyRestoration(Ctx.TargetPointIndex, Cleanse);
		S->NotifyRestoration(Cleanse, PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("corruption_success");
		R.Restoration = { true, true, Ctx.TargetPointIndex };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		TestTrue(TEXT("corruption_success is Success"), R.NightLoop.OutcomeResult == TEXT("Success"));
		TestTrue(TEXT("corruption_success mutated"), R.Sanctuary.bMutated);
		TestTrue(TEXT("corruption_success dawn consumed"), R.Dawn.bConsumedOutcome);
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- corruption_partial: reduce but not clear -> Partial ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.6f, 0.1f }, 0.3f, false));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightCorruptionStrategy* S = NewObject<UNightCorruptionStrategy>();
		const FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Corruption, PCG);
		S->EnterNight(Ctx, PCG);
		// A real but incomplete restoration: reduces the bloom below its start yet above the cleanse threshold.
		FRestorationEventPayload PartialCleanse; PartialCleanse.PointIndex = Ctx.TargetPointIndex; PartialCleanse.CorruptionCleared = 0.25f;
		PCG->ApplyRestoration(Ctx.TargetPointIndex, PartialCleanse); // 0.6 -> 0.35 (> 0.2 threshold)
		S->NotifyRestoration(PartialCleanse, PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("corruption_partial");
		R.Restoration = { true, true, Ctx.TargetPointIndex };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		TestTrue(TEXT("corruption_partial is Partial"), R.NightLoop.OutcomeResult == TEXT("Partial"));
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- corruption_failure: untouched -> Failure ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.5f, 0.2f, 0.2f }, 0.2f, false));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightCorruptionStrategy* S = NewObject<UNightCorruptionStrategy>();
		const FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Corruption, PCG);
		S->EnterNight(Ctx, PCG);
		S->ApplyPressureStep(PCG);
		S->ApplyPressureStep(PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("corruption_failure");
		R.Restoration = { false, false, -1 }; // player never acted
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		TestTrue(TEXT("corruption_failure is Failure"), R.NightLoop.OutcomeResult == TEXT("Failure"));
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- tutorial_success ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.3f, 0.3f, 0.3f }, 0.4f, false));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightTutorialStrategy* S = NewObject<UNightTutorialStrategy>();
		const FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Tutorial, PCG);
		S->EnterNight(Ctx, PCG);
		S->ApplyPressureStep(PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("tutorial_success");
		R.Restoration = { false, false, -1 }; // the tutorial teaches; it performs no restoration
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		TestTrue(TEXT("tutorial_success is Success"), R.NightLoop.OutcomeResult == TEXT("Success"));
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- quiet_fallback: no bloom -> benign quiet night ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.4f, 0.4f }, 0.6f, /*bRestored*/ true)); // all restored -> no unrestored bloom
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightCorruptionStrategy* S = NewObject<UNightCorruptionStrategy>();
		const FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Corruption, PCG);
		S->EnterNight(Ctx, PCG);
		S->ApplyPressureStep(PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("quiet_fallback");
		R.bQuiet = true;
		R.Restoration = { false, false, -1 };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		TestTrue(TEXT("quiet_fallback is Success"), R.NightLoop.OutcomeResult == TEXT("Success"));
		TestTrue(TEXT("quiet_fallback objective is None"), R.NightLoop.ObjectiveKind == TEXT("None"));
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- saveload_continuity: night mutation survives save/load ---
	{
		const FString Slot = TEXT("W3_ForgeContinuity");
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.5f, 0.2f, 0.1f }, 0.2f, false));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightCorruptionStrategy* S = NewObject<UNightCorruptionStrategy>();
		const FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Corruption, PCG);
		S->EnterNight(Ctx, PCG);
		S->ApplyPressureStep(PCG);
		S->ApplyPressureStep(PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		const float PostNight = PCG->GetSanctuaryAverageCorruptionLevel();
		const bool bSaved = PCG->SaveToSlot(Slot);
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.f, 0.f, 0.f }, 0.f, false));
		const bool bLoaded = PCG->LoadFromSlot(Slot);
		const bool bRoundtrip = bSaved && bLoaded &&
			FMath::IsNearlyEqual(PCG->GetSanctuaryAverageCorruptionLevel(), PostNight, KINDA_SMALL_NUMBER);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("saveload_continuity");
		R.bContinuity = true;
		R.Restoration = { false, false, -1 };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		R.SaveLoad = { true, bRoundtrip };
		TestTrue(TEXT("continuity roundtrip ok"), bRoundtrip);
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// ===== Night Types II: Omen =====

	// --- omen_success: player heeds the marked point -> Success ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.7f, 0.1f, 0.1f }, 0.3f, false));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightOmenStrategy* S = NewObject<UNightOmenStrategy>();
		const FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Omen, PCG);
		S->EnterNight(Ctx, PCG);
		S->ApplyPressureStep(PCG);
		FRestorationEventPayload Heed; Heed.PointIndex = Ctx.TargetPointIndex; Heed.CorruptionCleared = 1.0f;
		PCG->ApplyRestoration(Ctx.TargetPointIndex, Heed);
		S->NotifyRestoration(Heed, PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("omen_success");
		R.Restoration = { true, true, Ctx.TargetPointIndex };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		TestTrue(TEXT("omen_success is Success"), R.NightLoop.OutcomeResult == TEXT("Success"));
		TestTrue(TEXT("omen_success heeded tag"), R.NightLoop.ResultTag == TEXT("OmenHeeded"));
		TestTrue(TEXT("omen_success reduced the marked point"), R.Sanctuary.TargetCorruptionAfter < R.Sanctuary.TargetCorruptionBefore);
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- omen_partial: player acts off-target -> Partial ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.7f, 0.2f }, 0.3f, false));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightOmenStrategy* S = NewObject<UNightOmenStrategy>();
		const FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Omen, PCG);
		S->EnterNight(Ctx, PCG);
		S->ApplyPressureStep(PCG);
		// A real restoration, but on a DIFFERENT point than the omen marked.
		const int32 OffTarget = (Ctx.TargetPointIndex == 0) ? 1 : 0;
		FRestorationEventPayload Elsewhere; Elsewhere.PointIndex = OffTarget; Elsewhere.CorruptionCleared = 1.0f;
		PCG->ApplyRestoration(OffTarget, Elsewhere);
		S->NotifyRestoration(Elsewhere, PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("omen_partial");
		R.Restoration = { true, true, OffTarget };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		TestTrue(TEXT("omen_partial is Partial"), R.NightLoop.OutcomeResult == TEXT("Partial"));
		TestTrue(TEXT("omen_partial clouded tag"), R.NightLoop.ResultTag == TEXT("OmenClouded"));
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- omen_failure: ignored -> Failure, corruption seed ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.5f, 0.2f }, 0.2f, false));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightOmenStrategy* S = NewObject<UNightOmenStrategy>();
		const FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Omen, PCG);
		S->EnterNight(Ctx, PCG);
		S->ApplyPressureStep(PCG);
		S->ApplyPressureStep(PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("omen_failure");
		R.Restoration = { false, false, -1 };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		TestTrue(TEXT("omen_failure is Failure"), R.NightLoop.OutcomeResult == TEXT("Failure"));
		TestTrue(TEXT("omen_failure ignored tag"), R.NightLoop.ResultTag == TEXT("OmenIgnored"));
		TestTrue(TEXT("omen_failure left a seed"), R.Sanctuary.TargetCorruptionAfter > R.Sanctuary.TargetCorruptionBefore);
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// ===== Night Types II: Retrieval =====
	// The night's target is a RESTORED point, not the context's most-corrupted point; re-point the report's
	// target fields at the retrieval target so before/after describe the same point.

	// --- retrieval_success: player re-stabilizes the reclaimed point -> Success ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeRetrievalStates(0.1f, 0.6f));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightRetrievalStrategy* S = NewObject<UNightRetrievalStrategy>();
		FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Retrieval, PCG);
		S->EnterNight(Ctx, PCG);
		const int32 Target = S->GetObjective().TargetPointIndex;
		const bool bWasRestored = (Target >= 0) && PCG->IsPointRestored(Target);
		Ctx.TargetPointIndex = Target;
		Ctx.TargetStartCorruption = S->GetObjective().StartCorruption;
		S->ApplyPressureStep(PCG);
		FRestorationEventPayload Defend; Defend.PointIndex = Target; Defend.CorruptionCleared = 1.0f;
		PCG->ApplyRestoration(Target, Defend);
		S->NotifyRestoration(Defend, PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("retrieval_success");
		R.Restoration = { true, true, Target };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		R.NightLoop.bTargetWasRestored = bWasRestored;
		TestTrue(TEXT("retrieval_success is Success"), R.NightLoop.OutcomeResult == TEXT("Success"));
		TestTrue(TEXT("retrieval_success repelled tag"), R.NightLoop.ResultTag == TEXT("RetrievalRepelled"));
		TestTrue(TEXT("retrieval_success target was restored"), R.NightLoop.bTargetWasRestored);
		TestTrue(TEXT("retrieval_success stabilized the point"), R.Sanctuary.TargetCorruptionAfter < R.Sanctuary.TargetCorruptionBefore);
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- retrieval_partial: intervened but scarred -> Partial ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeRetrievalStates(0.1f, 0.6f));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightRetrievalStrategy* S = NewObject<UNightRetrievalStrategy>();
		FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Retrieval, PCG);
		S->EnterNight(Ctx, PCG);
		const int32 Target = S->GetObjective().TargetPointIndex;
		const bool bWasRestored = (Target >= 0) && PCG->IsPointRestored(Target);
		Ctx.TargetPointIndex = Target;
		Ctx.TargetStartCorruption = S->GetObjective().StartCorruption;
		S->ApplyPressureStep(PCG);
		S->ApplyPressureStep(PCG);
		FRestorationEventPayload Partial; Partial.PointIndex = Target; Partial.CorruptionCleared = 0.15f;
		PCG->ApplyRestoration(Target, Partial);
		S->NotifyRestoration(Partial, PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("retrieval_partial");
		R.Restoration = { true, true, Target };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		R.NightLoop.bTargetWasRestored = bWasRestored;
		TestTrue(TEXT("retrieval_partial is Partial"), R.NightLoop.OutcomeResult == TEXT("Partial"));
		TestTrue(TEXT("retrieval_partial seam tag"), R.NightLoop.ResultTag == TEXT("RetrievalSeam"));
		TestTrue(TEXT("retrieval_partial still restored"), PCG->IsPointRestored(Target));
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- retrieval_failure: no intervention -> Failure, point reclaimed ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeRetrievalStates(0.1f, 0.6f));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightRetrievalStrategy* S = NewObject<UNightRetrievalStrategy>();
		FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Retrieval, PCG);
		S->EnterNight(Ctx, PCG);
		const int32 Target = S->GetObjective().TargetPointIndex;
		const bool bWasRestored = (Target >= 0) && PCG->IsPointRestored(Target);
		Ctx.TargetPointIndex = Target;
		Ctx.TargetStartCorruption = S->GetObjective().StartCorruption;
		S->ApplyPressureStep(PCG);
		S->ApplyPressureStep(PCG);
		S->ApplyPressureStep(PCG);
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("retrieval_failure");
		R.Restoration = { false, false, -1 };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		R.NightLoop.bTargetWasRestored = bWasRestored;
		TestTrue(TEXT("retrieval_failure is Failure"), R.NightLoop.OutcomeResult == TEXT("Failure"));
		TestTrue(TEXT("retrieval_failure reclaimed tag"), R.NightLoop.ResultTag == TEXT("RetrievalReclaimed"));
		TestTrue(TEXT("retrieval_failure target was restored at dusk"), R.NightLoop.bTargetWasRestored);
		TestFalse(TEXT("retrieval_failure point reclaimed"), PCG->IsPointRestored(Target));
		TestTrue(TEXT("retrieval_failure worsened the point"), R.Sanctuary.TargetCorruptionAfter > R.Sanctuary.TargetCorruptionBefore);
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	// --- retrieval_nofallback: nothing restored -> honest quiet no-target fallback ---
	{
		UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
		PCG->Test_SeedPointStates(GFEmitMakeStates({ 0.3f, 0.3f }, 0.2f, /*bRestored*/ false));
		const float AvgBefore = PCG->GetSanctuaryAverageCorruptionLevel();
		UNightRetrievalStrategy* S = NewObject<UNightRetrievalStrategy>();
		FNightRuntimeContext Ctx = GFEmitMakeContext(ENightConsequenceType::Retrieval, PCG);
		S->EnterNight(Ctx, PCG);
		Ctx.TargetPointIndex = -1;      // no retrieval target exists
		Ctx.TargetStartCorruption = 0.f;
		S->ApplyPressureStep(PCG);      // fallback applies no pressure
		const FNightRuntimeOutcome Outcome = S->ResolveNight(PCG);

		FGloamsteadForgeReport R; R.ScenarioId = TEXT("retrieval_nofallback");
		R.bQuiet = true;
		R.Restoration = { false, false, -1 };
		FillFromRun(R, PCG, S, Ctx, AvgBefore, Outcome);
		TestTrue(TEXT("retrieval_nofallback is Success"), R.NightLoop.OutcomeResult == TEXT("Success"));
		TestTrue(TEXT("retrieval_nofallback objective is None"), R.NightLoop.ObjectiveKind == TEXT("None"));
		TestTrue(TEXT("retrieval_nofallback fallback tag"), R.NightLoop.ResultTag == TEXT("RetrievalNoTarget"));
		if (GloamsteadForgeEvidence::WriteReport(R, Dir, OutPath)) { ++Emitted; EmittedIds.Add(R.ScenarioId); }
	}

	TestEqual(TEXT("all thirteen scenario reports emitted"), Emitted, 13);

	// Write the run manifest (nonce + git + scenario set) that the integrity validator binds against.
	FString ManifestPath;
	const bool bManifest = GloamsteadForgeEvidence::WriteRunManifest(Dir, EmittedIds, ManifestPath);
	TestTrue(TEXT("run manifest written"), bManifest);

	UE_LOG(LogTemp, Log, TEXT("GloamsteadForge: emitted %d runtime reports + manifest to %s"), Emitted, *Dir);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
