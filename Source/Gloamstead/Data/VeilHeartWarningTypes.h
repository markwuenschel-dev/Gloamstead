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

/**
 * One authored clue for the standing warning, and whether the player has actually found it.
 *
 * FVeilHeartWarningSupportChannel::EvidenceText has always been documented as "player-facing
 * evidence text for journal/caption presentation" and has never been presented anywhere: the only
 * production reader of SupportChannels is the predicate that decides whether an encounter counts.
 * So the game asked the player to gather a minimum number of distinct clues, authored the sentence
 * each clue says, and then showed them neither the sentence nor the count.
 */
USTRUCT(BlueprintType)
struct FVeilHeartEvidenceLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warning|Support")
	FName SupportId = NAME_None;

	/** Readable medium, for example Environmental, ObjectReaction, or Audio. */
	UPROPERTY(BlueprintReadOnly, Category = "Warning|Support")
	FName ChannelType = NAME_None;

	/** What this clue says, authored in the catalog. Empty until the player has encountered it. */
	UPROPERTY(BlueprintReadOnly, Category = "Warning|Support")
	FText EvidenceText;

	UPROPERTY(BlueprintReadOnly, Category = "Warning|Support")
	bool bFound = false;
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

		if (EncounterableIds.Num() != RequiredChannels.Num())
		{
			return Fail(TEXT("warning support channels do not provide the exact authored evidence set"));
		}

		// Fair crypticism needs slack, not just a minimum. Two encountered channels are required to
		// interpret, so three must be AVAILABLE: otherwise one occluded or missed clue turns a fair
		// warning into an unanswerable one. This was previously spelled as a literal exemption for
		// the GardenRot identity, which meant every warning authored after it was unguarded.
		if (RequiredChannels.Num() < 3)
		{
			return Fail(TEXT("an authored warning must offer at least three readable support channels so two can be found"));
		}

		// The second clause of the warning is part of the same contract as the first. A plan whose
		// readings are half-authored would let a night grade the player against a reading they were
		// never offered, so the warning refuses to present at all.
		FString ReadingError;
		if (!Plan.HasCoherentSecondReadings(&ReadingError))
		{
			return Fail(TEXT("the plan second-reading set is incoherent, so its warning cannot be presented fairly"));
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

/**
 * The single authority on whether an authored warning catalog satisfies the whole experience contract.
 *
 * Every authored plan must resolve EXACTLY one warning row for its (WarningId, NightType) pair, because
 * AVeilHeart::FindExactWarningById refuses an ambiguous match exactly as it refuses a missing one - two
 * rows fail identically to zero. A plan whose row is missing cannot present its warning, which means
 * PresentedPlanId is never set, which means UGloamsteadDayNightSubsystem::CanRestNow denies rest and that
 * night can never begin.
 *
 * This exists so the contract is checked in ONE place: the runtime load path fails closed against it, and
 * the shipped-catalog automation test asserts against the same function rather than reimplementing the
 * rule. Returns true when the catalog is complete; otherwise fills OutProblems with actionable defects
 * naming the slot, the plan, and the missing pair.
 */
GLOAMSTEAD_API bool ValidateWarningCatalogCoversAuthoredPlans(
	const UVeilHeartWarningCatalog& Catalog,
	TArray<FString>& OutProblems);
