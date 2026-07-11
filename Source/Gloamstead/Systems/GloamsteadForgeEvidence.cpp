#include "Systems/GloamsteadForgeEvidence.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"

namespace
{
	FString GitDir()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT(".git"));
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
	FString Head;
	if (!FFileHelper::LoadFileToString(Head, *FPaths::Combine(GitDir(), TEXT("HEAD"))))
	{
		return FString();
	}
	Head.TrimStartAndEndInline();
	const FString Prefix = TEXT("ref: refs/heads/");
	if (Head.StartsWith(Prefix))
	{
		return Head.RightChop(Prefix.Len());
	}
	return FString(); // detached HEAD
}

FString GloamsteadForgeEvidence::ReadGitCommit()
{
	FString Head;
	if (!FFileHelper::LoadFileToString(Head, *FPaths::Combine(GitDir(), TEXT("HEAD"))))
	{
		return FString();
	}
	Head.TrimStartAndEndInline();

	const FString Prefix = TEXT("ref: ");
	if (!Head.StartsWith(Prefix))
	{
		return Head; // detached HEAD holds the SHA directly
	}

	const FString RefPath = Head.RightChop(Prefix.Len()); // e.g. refs/heads/gloamstead/w3-...

	// Loose ref first.
	FString Sha;
	if (FFileHelper::LoadFileToString(Sha, *FPaths::Combine(GitDir(), RefPath)))
	{
		Sha.TrimStartAndEndInline();
		return Sha;
	}

	// Packed refs fallback.
	FString Packed;
	if (FFileHelper::LoadFileToString(Packed, *FPaths::Combine(GitDir(), TEXT("packed-refs"))))
	{
		TArray<FString> Lines;
		Packed.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			if (Line.EndsWith(RefPath))
			{
				FString Left, Right;
				if (Line.Split(TEXT(" "), &Left, &Right))
				{
					return Left.TrimStartAndEnd();
				}
			}
		}
	}
	return FString();
}

FString GloamsteadForgeEvidence::DefaultReportDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("procedural"), TEXT("reports"), TEXT("gloamsteadforge"));
}

FString GloamsteadForgeEvidence::ToJson(const FGloamsteadForgeReport& Report)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("GloamsteadForgeRuntimeReport/v1"));
	Root->SetStringField(TEXT("scenario_id"), Report.ScenarioId);
	Root->SetStringField(TEXT("generated_at_utc"), FDateTime::UtcNow().ToIso8601());
	Root->SetStringField(TEXT("git_commit"), ReadGitCommit());
	Root->SetStringField(TEXT("git_branch"), ReadGitBranch());
	Root->SetStringField(TEXT("engine"), Report.Engine);
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
