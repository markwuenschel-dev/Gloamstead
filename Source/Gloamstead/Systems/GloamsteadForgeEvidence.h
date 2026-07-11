#pragma once

#include "CoreMinimal.h"

/**
 * GloamsteadForge runtime evidence (Corrected Wave 3).
 *
 * Plain C++ report structs + a JSON serializer. These are populated ONLY from a real Gloamstead loop run
 * (see GloamsteadForgeEvidenceTests) and serialized to conformant JSON matching
 * specs/gloamsteadforge/contracts/GloamsteadForgeRuntimeReport.schema.json. The hostile PowerShell
 * validators then enforce fail-closed semantics over the emitted reports.
 *
 * Not UStructs: this is evidence emission, not gameplay reflection.
 */

struct FGFPcgInit
{
	bool bInitialized = false;
	int32 PointCount = 0;
};

struct FGFRestoration
{
	bool bAttempted = false;
	bool bApplied = false;
	int32 PointIndex = -1;
};

struct FGFNightLoop
{
	bool bStarted = false;
	FString NightType = TEXT("Invalid");
	FString ObjectiveKind = TEXT("None");
	int32 TargetPointIndex = -1;
	bool bObjectiveResolved = false;
	bool bEndedIntentionally = false;
	FString OutcomeResult = TEXT("None");
	FString ResultTag;
};

struct FGFSanctuaryState
{
	float AvgCorruptionBefore = 0.f;
	float AvgCorruptionAfter = 0.f;
	float TargetCorruptionBefore = 0.f;
	float TargetCorruptionAfter = 0.f;
	bool bMutated = false;
};

struct FGFDawn
{
	bool bConsumedOutcome = false;
	FString OutcomeResult = TEXT("None");
};

struct FGFSaveLoad
{
	bool bChecked = false;
	bool bRoundtripOk = false;
};

struct FGloamsteadForgeReport
{
	FString ScenarioId;
	FString Engine = TEXT("UE5.8");
	bool bQuiet = false;
	bool bContinuity = false;
	bool bHumanPlaytest = false;
	FGFPcgInit PcgInit;
	FGFRestoration Restoration;
	FGFNightLoop NightLoop;
	FGFSanctuaryState Sanctuary;
	FGFDawn Dawn;
	FGFSaveLoad SaveLoad;
	TArray<FString> FailureCodes;
};

namespace GloamsteadForgeEvidence
{
	/** Serialize a report to schema-conformant JSON (stamps schema tag, UTC time, and repo git commit/branch). */
	GLOAMSTEAD_API FString ToJson(const FGloamsteadForgeReport& Report);

	/** Write a report as <OutDir>/<scenario_id>.json. Returns true on success; OutPath = written path. */
	GLOAMSTEAD_API bool WriteReport(const FGloamsteadForgeReport& Report, const FString& OutDir, FString& OutPath);

	/** Default emit dir: <ProjectDir>/procedural/reports/gloamsteadforge. */
	GLOAMSTEAD_API FString DefaultReportDir();

	/** Write the per-run manifest (_run_manifest.json): run nonce, git commit, and the emitted scenario set.
	 *  The nonce comes from the GLOAMSTEAD_FORGE_NONCE env var set by gate.ps1, so a hand-authored report
	 *  (which cannot know the fresh nonce) is rejected by the integrity validator. */
	GLOAMSTEAD_API bool WriteRunManifest(const FString& OutDir, const TArray<FString>& ScenarioIds, FString& OutPath);

	/** Best-effort short git commit SHA read from <ProjectDir>/.git (loose or packed ref). Empty if unavailable. */
	GLOAMSTEAD_API FString ReadGitCommit();

	/** Best-effort current git branch from <ProjectDir>/.git/HEAD. Empty if detached/unavailable. */
	GLOAMSTEAD_API FString ReadGitBranch();
}
