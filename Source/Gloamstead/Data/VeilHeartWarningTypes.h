#pragma once

#include "CoreMinimal.h"
#include "Data/ExperienceCycleTypes.h"
#include "Engine/DataAsset.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/RitualTypes.h"
#include "VeilHeartWarningTypes.generated.h"

/** One player-readable support channel for an authored Heart warning. */
USTRUCT(BlueprintType)
struct FVeilHeartWarningSupportChannel
{
	GENERATED_BODY()

	/** Stable ID reported by environment, object, audio, or enemy evidence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning|Support")
	FName SupportId = NAME_None;

	/** Short player-facing evidence text for journal/caption presentation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning|Support")
	FText EvidenceText;

	/** Readable medium, for example Environmental, ObjectReaction, or Audio. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning|Support")
	FName ChannelType = NAME_None;
};

USTRUCT(BlueprintType)
struct FVeilHeartWarningFragment
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	FName WarningId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	FText Fragment;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	ENightConsequenceType AssociatedNightType = ENightConsequenceType::Invalid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	TArray<FName> SatisfiableTags;

	/** Stable Gloamstead-owned place this warning asks the player to understand. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	FName SemanticSubject = NAME_None;

	/** The ritual form required to turn this warning's evidence into restoration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	ERitualType RequiredRitualType = ERitualType::Invalid;

	/** Distinct readable evidence channels that may be encountered for this warning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	TArray<FVeilHeartWarningSupportChannel> SupportChannels;

	/** Stable authored receipt key for the exact interpreted result. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	FName InterpretationReceiptId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning", meta = (ClampMin = "0"))
	int32 ClarityTier = 0;

	/**
	 * Validates that this warning says exactly what the armed authored plan says:
	 * identity, subject, ritual/tag, readable support IDs, and receipt all agree.
	 */
	bool MatchesExactPlanContract(const FExperienceCyclePlan& Plan, FString* OutError = nullptr) const
	{
		auto Fail = [OutError](const TCHAR* Message)
		{
			if (OutError)
			{
				*OutError = Message;
			}
			return false;
		};

		if (!Plan.IsAuthoredPlan()
			|| WarningId == NAME_None
			|| WarningId != Plan.WarningId
			|| AssociatedNightType != Plan.NightType
			|| SemanticSubject == NAME_None
			|| SemanticSubject != Plan.SemanticSubject
			|| RequiredRitualType == ERitualType::Invalid
			|| RequiredRitualType != Plan.RequiredRitualType
			|| InterpretationReceiptId == NAME_None
			|| InterpretationReceiptId != Plan.InterpretationReceiptId)
		{
			return Fail(TEXT("warning identity, subject, ritual, or receipt does not match its authored plan"));
		}

		if (SatisfiableTags.Num() != Plan.RequiredRestorationTags.Num()
			|| SatisfiableTags.IsEmpty())
		{
			return Fail(TEXT("warning restoration tags are missing or do not match the authored plan"));
		}
		for (const FName Tag : SatisfiableTags)
		{
			if (Tag == NAME_None || !Plan.RequiredRestorationTags.Contains(Tag))
			{
				return Fail(TEXT("warning restoration tags are missing or do not match the authored plan"));
			}
		}

		if (Plan.RequiredSupportIds.Num() != SupportChannels.Num()
			|| Plan.RequiredSupportIds.Num() != Plan.RequiredSupportChannelTypes.Num()
			|| Plan.RequiredSupportIds.IsEmpty()
			|| Plan.MinimumDistinctSupportCount < 2
			|| Plan.MinimumDistinctSupportCount > Plan.RequiredSupportIds.Num())
		{
			return Fail(TEXT("warning support channels are sparse or do not match the authored plan"));
		}

		TMap<FName, FName> RequiredChannels;
		for (int32 SupportIndex = 0; SupportIndex < Plan.RequiredSupportIds.Num(); ++SupportIndex)
		{
			const FName SupportId = Plan.RequiredSupportIds[SupportIndex];
			const FName ChannelType = Plan.RequiredSupportChannelTypes[SupportIndex];
			if (SupportId == NAME_None || ChannelType == NAME_None || RequiredChannels.Contains(SupportId))
			{
				return Fail(TEXT("authored plan declares duplicate or empty support identifiers or media"));
			}
			RequiredChannels.Add(SupportId, ChannelType);
		}

		TSet<FName> EncounterableIds;
		for (const FVeilHeartWarningSupportChannel& Channel : SupportChannels)
		{
			if (Channel.SupportId == NAME_None
				|| EncounterableIds.Contains(Channel.SupportId)
				|| !RequiredChannels.Contains(Channel.SupportId)
				|| Channel.ChannelType == NAME_None
				|| Channel.ChannelType != RequiredChannels.FindRef(Channel.SupportId)
				|| Channel.EvidenceText.ToString().TrimStartAndEnd().IsEmpty())
			{
				return Fail(TEXT("warning declares duplicate, unknown, wrong-medium, or unreadable support data"));
			}
			EncounterableIds.Add(Channel.SupportId);
		}

		if (EncounterableIds.Num() != RequiredChannels.Num()
			|| (WarningId == FName(TEXT("GardenRot")) && EncounterableIds.Num() != 3))
		{
			return Fail(TEXT("warning support channels do not provide the exact authored evidence set"));
		}

		return true;
	}
};

UCLASS(BlueprintType)
class GLOAMSTEAD_API UVeilHeartWarningCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warning")
	TArray<FVeilHeartWarningFragment> Warnings;
};
