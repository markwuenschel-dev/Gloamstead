#include "Systems/GloamsteadForgeEvidence.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"

namespace
{
	constexpr int64 MaxMetadataFileBytes = 4096;
	constexpr int64 MaxPackedRefsBytes = 16 * 1024 * 1024;

	struct FResolvedGitMetadata
	{
		FString ProjectRoot;
		FString DotGitEntry;
		FString WorktreeGitDir;
		FString CommonGitDir;
	};

	enum class ERefReadResult : uint8
	{
		NotFound,
		Found,
		Invalid
	};

	bool CanonicalizePath(const FString& Input, FString& OutPath, const bool bDirectory)
	{
		// FString paths are length-delimited; an empty value is the only representable null path.
		if (Input.IsEmpty())
		{
			return false;
		}

		OutPath = FPaths::ConvertRelativePathToFull(Input);
		if (bDirectory)
		{
			FPaths::NormalizeDirectoryName(OutPath);
		}
		else
		{
			FPaths::NormalizeFilename(OutPath);
		}
		if (!FPaths::CollapseRelativeDirectories(OutPath) || FPaths::IsRelative(OutPath))
		{
			OutPath.Reset();
			return false;
		}
		return true;
	}

	bool IsSamePath(const FString& A, const FString& B)
	{
		return A.Equals(B, ESearchCase::IgnoreCase);
	}

	bool IsPathWithin(const FString& Candidate, const FString& Root)
	{
		if (IsSamePath(Candidate, Root))
		{
			return true;
		}
		FString RootWithSlash = Root;
		if (!RootWithSlash.EndsWith(TEXT("/")))
		{
			RootWithSlash += TEXT("/");
		}
		return Candidate.StartsWith(RootWithSlash, ESearchCase::IgnoreCase);
	}

	bool ResolveAgainst(const FString& BaseDirectory, const FString& PathText, FString& OutPath, const bool bDirectory)
	{
		if (PathText.IsEmpty())
		{
			return false;
		}
		const FString Candidate = FPaths::IsRelative(PathText)
			? FPaths::Combine(BaseDirectory, PathText)
			: PathText;
		return CanonicalizePath(Candidate, OutPath, bDirectory);
	}

	bool ReadSingleMetadataLine(const FString& Path, FString& OutLine)
	{
		OutLine.Reset();
		const int64 Size = IFileManager::Get().FileSize(*Path);
		if (Size <= 0 || Size > MaxMetadataFileBytes || !FFileHelper::LoadFileToString(OutLine, *Path))
		{
			OutLine.Reset();
			return false;
		}

		// Git metadata is one line terminated by LF or CRLF. Remove one terminator and reject
		// embedded lines, bare CR, or edge whitespace instead of trying to repair it.
		if (OutLine.EndsWith(TEXT("\n")))
		{
			OutLine.LeftChopInline(1, EAllowShrinking::No);
			if (OutLine.EndsWith(TEXT("\r")))
			{
				OutLine.LeftChopInline(1, EAllowShrinking::No);
			}
		}
		if (OutLine.IsEmpty()
			|| OutLine.Contains(TEXT("\n"))
			|| OutLine.Contains(TEXT("\r"))
			|| !OutLine.Equals(OutLine.TrimStartAndEnd(), ESearchCase::CaseSensitive))
		{
			OutLine.Reset();
			return false;
		}
		return true;
	}

	bool ResolveGitMetadata(const FString& ProjectRootInput, FResolvedGitMetadata& OutMetadata)
	{
		OutMetadata = FResolvedGitMetadata();
		if (!CanonicalizePath(ProjectRootInput, OutMetadata.ProjectRoot, true)
			|| !IFileManager::Get().DirectoryExists(*OutMetadata.ProjectRoot))
		{
			return false;
		}

		if (!CanonicalizePath(FPaths::Combine(OutMetadata.ProjectRoot, TEXT(".git")), OutMetadata.DotGitEntry, false)
			|| !IsPathWithin(OutMetadata.DotGitEntry, OutMetadata.ProjectRoot))
		{
			return false;
		}

		if (IFileManager::Get().DirectoryExists(*OutMetadata.DotGitEntry))
		{
			OutMetadata.WorktreeGitDir = OutMetadata.DotGitEntry;
			OutMetadata.CommonGitDir = OutMetadata.DotGitEntry;
			return true;
		}

		if (!IFileManager::Get().FileExists(*OutMetadata.DotGitEntry))
		{
			return false;
		}

		FString GitDirLine;
		if (!ReadSingleMetadataLine(OutMetadata.DotGitEntry, GitDirLine))
		{
			return false;
		}
		const FString GitDirPrefix = TEXT("gitdir: ");
		if (!GitDirLine.StartsWith(GitDirPrefix, ESearchCase::CaseSensitive)
			|| !ResolveAgainst(
				OutMetadata.ProjectRoot,
				GitDirLine.RightChop(GitDirPrefix.Len()),
				OutMetadata.WorktreeGitDir,
				true)
			|| !IFileManager::Get().DirectoryExists(*OutMetadata.WorktreeGitDir))
		{
			return false;
		}

		FString CommonDirLine;
		if (!ReadSingleMetadataLine(FPaths::Combine(OutMetadata.WorktreeGitDir, TEXT("commondir")), CommonDirLine))
		{
			return false;
		}
		if (!ResolveAgainst(OutMetadata.WorktreeGitDir, CommonDirLine, OutMetadata.CommonGitDir, true))
		{
			return false;
		}
		if (!IFileManager::Get().DirectoryExists(*OutMetadata.CommonGitDir)
			|| IsSamePath(OutMetadata.WorktreeGitDir, OutMetadata.CommonGitDir)
			|| !FPaths::GetCleanFilename(OutMetadata.CommonGitDir).Equals(TEXT(".git"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		// A linked-worktree administrative directory has exactly this ancestry. This prevents an
		// arbitrary .git file from granting reads across unrelated directories.
		FString ExpectedAdminParent;
		if (!CanonicalizePath(FPaths::Combine(OutMetadata.CommonGitDir, TEXT("worktrees")), ExpectedAdminParent, true)
			|| !IsSamePath(FPaths::GetPath(OutMetadata.WorktreeGitDir), ExpectedAdminParent)
			|| FPaths::GetCleanFilename(OutMetadata.WorktreeGitDir).IsEmpty())
		{
			return false;
		}

		FString BackPointerLine;
		FString BackPointer;
		if (!ReadSingleMetadataLine(FPaths::Combine(OutMetadata.WorktreeGitDir, TEXT("gitdir")), BackPointerLine)
			|| !ResolveAgainst(OutMetadata.WorktreeGitDir, BackPointerLine, BackPointer, false)
			|| !IsSamePath(BackPointer, OutMetadata.DotGitEntry))
		{
			return false;
		}

		return true;
	}

	bool IsHexCommit(const FString& Value)
	{
		if (Value.Len() != 40 && Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsHexDigit(Character))
			{
				return false;
			}
		}
		return true;
	}

	bool IsSafeHeadRef(const FString& RefName)
	{
		if (!RefName.StartsWith(TEXT("refs/heads/"), ESearchCase::CaseSensitive)
			|| RefName.Len() <= FCString::Strlen(TEXT("refs/heads/"))
			|| RefName.Len() > 1024
			|| RefName.EndsWith(TEXT("/"))
			|| RefName.EndsWith(TEXT("."))
			|| RefName.Contains(TEXT("//"))
			|| RefName.Contains(TEXT(".."))
			|| RefName.Contains(TEXT("@{"))
			|| RefName.Contains(TEXT("\\")))
		{
			return false;
		}

		for (const TCHAR Character : RefName)
		{
			if (FChar::IsWhitespace(Character)
				|| FChar::IsControl(Character)
				|| Character == TEXT('~')
				|| Character == TEXT('^')
				|| Character == TEXT(':')
				|| Character == TEXT('?')
				|| Character == TEXT('*')
				|| Character == TEXT('['))
			{
				return false;
			}
		}

		TArray<FString> Components;
		RefName.ParseIntoArray(Components, TEXT("/"), false);
		for (const FString& Component : Components)
		{
			if (Component.IsEmpty()
				|| Component == TEXT(".")
				|| Component == TEXT("..")
				|| Component == TEXT("@")
				|| Component.StartsWith(TEXT("."))
				|| Component.EndsWith(TEXT("."))
				|| Component.EndsWith(TEXT(".lock"), ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
		return true;
	}

	bool SafeMetadataPath(const FString& Root, const FString& RelativePath, FString& OutPath)
	{
		return CanonicalizePath(FPaths::Combine(Root, RelativePath), OutPath, false)
			&& IsPathWithin(OutPath, Root);
	}

	ERefReadResult ReadLooseRef(const FString& GitRoot, const FString& RefName, FString& OutCommit)
	{
		FString RefPath;
		if (!SafeMetadataPath(GitRoot, RefName, RefPath))
		{
			return ERefReadResult::Invalid;
		}
		if (!IFileManager::Get().FileExists(*RefPath))
		{
			return ERefReadResult::NotFound;
		}

		FString Value;
		if (!ReadSingleMetadataLine(RefPath, Value) || !IsHexCommit(Value))
		{
			return ERefReadResult::Invalid;
		}
		OutCommit = Value.ToLower();
		return ERefReadResult::Found;
	}

	ERefReadResult ReadPackedRef(const FString& GitRoot, const FString& RefName, FString& OutCommit)
	{
		FString PackedRefsPath;
		if (!SafeMetadataPath(GitRoot, TEXT("packed-refs"), PackedRefsPath))
		{
			return ERefReadResult::Invalid;
		}
		if (!IFileManager::Get().FileExists(*PackedRefsPath))
		{
			return ERefReadResult::NotFound;
		}

		const int64 Size = IFileManager::Get().FileSize(*PackedRefsPath);
		FString PackedRefs;
		if (Size <= 0 || Size > MaxPackedRefsBytes || !FFileHelper::LoadFileToString(PackedRefs, *PackedRefsPath))
		{
			return ERefReadResult::Invalid;
		}

		TArray<FString> Lines;
		PackedRefs.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT("^")))
			{
				continue;
			}

			int32 Separator = INDEX_NONE;
			for (int32 Index = 0; Index < Line.Len(); ++Index)
			{
				if (Line[Index] == TEXT(' ') || Line[Index] == TEXT('\t'))
				{
					Separator = Index;
					break;
				}
			}
			if (Separator == INDEX_NONE)
			{
				continue;
			}

			const FString CandidateCommit = Line.Left(Separator);
			// packed-refs has exactly two fields. Do not trim or suffix-match a malformed second field.
			const FString CandidateRef = Line.Mid(Separator + 1);
			if (CandidateRef.Equals(RefName, ESearchCase::CaseSensitive))
			{
				if (!IsHexCommit(CandidateCommit))
				{
					return ERefReadResult::Invalid;
				}
				OutCommit = CandidateCommit.ToLower();
				return ERefReadResult::Found;
			}
		}
		return ERefReadResult::NotFound;
	}

	bool ReadSymbolicRef(const FResolvedGitMetadata& Metadata, const FString& RefName, FString& OutCommit)
	{
		// refs/heads is shared across linked worktrees. HEAD itself belongs to the worktree
		// administrative directory, but its branch target must be resolved exclusively from the
		// authenticated common Git directory. Looking in WorktreeGitDir would let a non-authoritative
		// refs/heads or packed-refs lookalike shadow repository identity evidence.
		if (!IsSafeHeadRef(RefName))
		{
			return false;
		}

		const ERefReadResult LooseResult = ReadLooseRef(Metadata.CommonGitDir, RefName, OutCommit);
		if (LooseResult == ERefReadResult::Found)
		{
			return true;
		}
		if (LooseResult == ERefReadResult::Invalid)
		{
			return false;
		}

		return ReadPackedRef(Metadata.CommonGitDir, RefName, OutCommit) == ERefReadResult::Found;
	}

	// Round to 4 decimals so serialized corruption values stay clean (0.6, not 0.60000002).
	double Round4(float V)
	{
		return FMath::RoundToDouble(static_cast<double>(V) * 10000.0) / 10000.0;
	}

	TSharedPtr<FJsonObject> MakePcgInit(const FGFPcgInit& In)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetBoolField(TEXT("initialized"), In.bInitialized);
		O->SetNumberField(TEXT("point_count"), In.PointCount);
		return O;
	}

	TSharedPtr<FJsonObject> MakeRestoration(const FGFRestoration& In)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetBoolField(TEXT("attempted"), In.bAttempted);
		O->SetBoolField(TEXT("applied"), In.bApplied);
		O->SetNumberField(TEXT("point_index"), In.PointIndex);
		return O;
	}

	TSharedPtr<FJsonObject> MakeNightLoop(const FGFNightLoop& In)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetBoolField(TEXT("started"), In.bStarted);
		O->SetStringField(TEXT("night_type"), In.NightType);
		O->SetStringField(TEXT("objective_kind"), In.ObjectiveKind);
		O->SetNumberField(TEXT("target_point_index"), In.TargetPointIndex);
		O->SetBoolField(TEXT("objective_resolved"), In.bObjectiveResolved);
		O->SetBoolField(TEXT("ended_intentionally"), In.bEndedIntentionally);
		O->SetStringField(TEXT("outcome_result"), In.OutcomeResult);
		O->SetStringField(TEXT("result_tag"), In.ResultTag);
		O->SetBoolField(TEXT("target_was_restored"), In.bTargetWasRestored);
		return O;
	}

	TSharedPtr<FJsonObject> MakeSanctuary(const FGFSanctuaryState& In)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("avg_corruption_before"), Round4(In.AvgCorruptionBefore));
		O->SetNumberField(TEXT("avg_corruption_after"), Round4(In.AvgCorruptionAfter));
		O->SetNumberField(TEXT("target_corruption_before"), Round4(In.TargetCorruptionBefore));
		O->SetNumberField(TEXT("target_corruption_after"), Round4(In.TargetCorruptionAfter));
		O->SetBoolField(TEXT("mutated"), In.bMutated);
		return O;
	}

	TSharedPtr<FJsonObject> MakeDawn(const FGFDawn& In)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetBoolField(TEXT("consumed_outcome"), In.bConsumedOutcome);
		O->SetStringField(TEXT("outcome_result"), In.OutcomeResult);
		return O;
	}

	TSharedPtr<FJsonObject> MakeSaveLoad(const FGFSaveLoad& In)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetBoolField(TEXT("checked"), In.bChecked);
		O->SetBoolField(TEXT("roundtrip_ok"), In.bRoundtripOk);
		return O;
	}
}

FString GloamsteadForgeEvidence::ReadGitBranch()
{
	FString Commit;
	FString Branch;
	ReadGitIdentityForProjectRoot(FPaths::ProjectDir(), Commit, Branch);
	return Branch;
}

FString GloamsteadForgeEvidence::ReadGitCommit()
{
	FString Commit;
	FString Branch;
	ReadGitIdentityForProjectRoot(FPaths::ProjectDir(), Commit, Branch);
	return Commit;
}

bool GloamsteadForgeEvidence::ReadGitIdentityForProjectRoot(
	const FString& ProjectRoot,
	FString& OutCommit,
	FString& OutBranch)
{
	OutCommit.Reset();
	OutBranch.Reset();

	FResolvedGitMetadata Metadata;
	if (!ResolveGitMetadata(ProjectRoot, Metadata))
	{
		return false;
	}

	FString Head;
	FString HeadPath;
	if (!SafeMetadataPath(Metadata.WorktreeGitDir, TEXT("HEAD"), HeadPath)
		|| !ReadSingleMetadataLine(HeadPath, Head))
	{
		return false;
	}

	const FString Prefix = TEXT("ref: ");
	if (!Head.StartsWith(Prefix))
	{
		if (!IsHexCommit(Head))
		{
			return false;
		}
		OutCommit = Head.ToLower();
		return true;
	}

	const FString RefName = Head.RightChop(Prefix.Len());
	if (!IsSafeHeadRef(RefName) || !ReadSymbolicRef(Metadata, RefName, OutCommit))
	{
		OutCommit.Reset();
		return false;
	}

	OutBranch = RefName.RightChop(FCString::Strlen(TEXT("refs/heads/")));
	return true;
}

FString GloamsteadForgeEvidence::DefaultReportDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("procedural"), TEXT("reports"), TEXT("gloamsteadforge"));
}

FString GloamsteadForgeEvidence::ToJson(const FGloamsteadForgeReport& Report)
{
	FString GitCommit;
	FString GitBranch;
	ReadGitIdentityForProjectRoot(FPaths::ProjectDir(), GitCommit, GitBranch);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("GloamsteadForgeRuntimeReport/v1"));
	Root->SetStringField(TEXT("scenario_id"), Report.ScenarioId);
	Root->SetStringField(TEXT("generated_at_utc"), FDateTime::UtcNow().ToIso8601());
	Root->SetStringField(TEXT("git_commit"), GitCommit);
	Root->SetStringField(TEXT("git_branch"), GitBranch);
	Root->SetStringField(TEXT("engine"), Report.Engine);
	Root->SetStringField(TEXT("run_nonce"), FPlatformMisc::GetEnvironmentVariable(TEXT("GLOAMSTEAD_FORGE_NONCE")));
	Root->SetBoolField(TEXT("quiet"), Report.bQuiet);
	Root->SetBoolField(TEXT("continuity"), Report.bContinuity);
	Root->SetBoolField(TEXT("human_playtest"), Report.bHumanPlaytest);

	Root->SetObjectField(TEXT("pcg_init"), MakePcgInit(Report.PcgInit));
	Root->SetObjectField(TEXT("restoration"), MakeRestoration(Report.Restoration));
	Root->SetObjectField(TEXT("night_loop"), MakeNightLoop(Report.NightLoop));
	Root->SetObjectField(TEXT("sanctuary_state"), MakeSanctuary(Report.Sanctuary));
	Root->SetObjectField(TEXT("dawn_reflection"), MakeDawn(Report.Dawn));
	Root->SetObjectField(TEXT("save_load"), MakeSaveLoad(Report.SaveLoad));

	TArray<TSharedPtr<FJsonValue>> Codes;
	for (const FString& Code : Report.FailureCodes)
	{
		Codes.Add(MakeShared<FJsonValueString>(Code));
	}
	Root->SetArrayField(TEXT("failure_codes"), Codes);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

bool GloamsteadForgeEvidence::WriteReport(const FGloamsteadForgeReport& Report, const FString& OutDir, FString& OutPath)
{
	IFileManager::Get().MakeDirectory(*OutDir, /*Tree*/ true);
	OutPath = FPaths::Combine(OutDir, Report.ScenarioId + TEXT(".json"));
	return FFileHelper::SaveStringToFile(ToJson(Report), *OutPath);
}

bool GloamsteadForgeEvidence::WriteRunManifest(const FString& OutDir, const TArray<FString>& ScenarioIds, FString& OutPath)
{
	IFileManager::Get().MakeDirectory(*OutDir, /*Tree*/ true);
	FString GitCommit;
	FString GitBranch;
	ReadGitIdentityForProjectRoot(FPaths::ProjectDir(), GitCommit, GitBranch);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("GloamsteadForgeRunManifest/v1"));
	Root->SetStringField(TEXT("run_nonce"), FPlatformMisc::GetEnvironmentVariable(TEXT("GLOAMSTEAD_FORGE_NONCE")));
	Root->SetStringField(TEXT("git_commit"), GitCommit);
	Root->SetStringField(TEXT("git_branch"), GitBranch);
	Root->SetStringField(TEXT("generated_at_utc"), FDateTime::UtcNow().ToIso8601());

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FString& Id : ScenarioIds)
	{
		Arr.Add(MakeShared<FJsonValueString>(Id));
	}
	Root->SetArrayField(TEXT("scenarios"), Arr);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	OutPath = FPaths::Combine(OutDir, TEXT("_run_manifest.json"));
	return FFileHelper::SaveStringToFile(Out, *OutPath);
}
