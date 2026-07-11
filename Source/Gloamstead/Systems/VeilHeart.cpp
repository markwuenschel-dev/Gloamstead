#include "Systems/VeilHeart.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Engine/World.h"

AVeilHeart::AVeilHeart()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AVeilHeart::BeginPlay()
{
	Super::BeginPlay();

	if (!WarningCatalog)
	{
		WarningCatalog = Cast<UVeilHeartWarningCatalog>(
			StaticLoadObject(UVeilHeartWarningCatalog::StaticClass(), nullptr,
				TEXT("/Game/Data/DA_VeilHeartWarningCatalog.DA_VeilHeartWarningCatalog")));
		if (WarningCatalog)
		{
			UE_LOG(LogTemp, Log, TEXT("VeilHeart: Loaded warning catalog from /Game/Data/DA_VeilHeartWarningCatalog."));
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UGloamsteadPCGSubsystem* PCGSub = World->GetSubsystem<UGloamsteadPCGSubsystem>())
		{
			PCGSub->OnStructureRestored.AddDynamic(this, &AVeilHeart::OnRestorationComplete);
		}
	}
}

void AVeilHeart::OnRestorationComplete(const FRestorationEventPayload& Payload)
{
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: Restoration received - Ritual: %d, LightDelta: %.2f, CorruptionCleared: %.2f, WarningTag: %s"),
		static_cast<int32>(Payload.RitualType), Payload.LightDelta, Payload.CorruptionCleared,
		*Payload.WarningTagSatisfied.ToString());

	EvaluateRestorationAgainstWarnings(Payload);
}

void AVeilHeart::EvaluateRestorationAgainstWarnings(const FRestorationEventPayload& Payload)
{
	FName TagToCheck = Payload.WarningTagSatisfied;
	if (TagToCheck == NAME_None)
	{
		TagToCheck = FName(*GetRitualTypeDisplayName(Payload.RitualType));
	}

	if (TagToCheck == NAME_None)
	{
		return;
	}

	if (WarningCatalog)
	{
		for (const FVeilHeartWarningFragment& Warning : WarningCatalog->Warnings)
		{
			if (Warning.SatisfiableTags.Contains(TagToCheck))
			{
				if (!SatisfiedWarningTags.Contains(TagToCheck))
				{
					SatisfiedWarningTags.Add(TagToCheck);
					UE_LOG(LogTemp, Log, TEXT("VeilHeart: Warning tag satisfied via catalog: %s"), *TagToCheck.ToString());
				}
				return;
			}
		}
		UE_LOG(LogTemp, Verbose, TEXT("VeilHeart: Tag %s did not match any catalog warning."), *TagToCheck.ToString());
		return;
	}

	if (!SatisfiedWarningTags.Contains(TagToCheck))
	{
		SatisfiedWarningTags.Add(TagToCheck);
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Warning tag satisfied (no catalog): %s"), *TagToCheck.ToString());
	}
}

const FVeilHeartWarningFragment* AVeilHeart::FindWarningForNight(ENightConsequenceType NightType) const
{
	if (!WarningCatalog)
	{
		return nullptr;
	}

	const FVeilHeartWarningFragment* Best = nullptr;
	for (const FVeilHeartWarningFragment& Warning : WarningCatalog->Warnings)
	{
		if (Warning.AssociatedNightType == NightType)
		{
			if (!Best || Warning.ClarityTier >= Best->ClarityTier)
			{
				Best = &Warning;
			}
		}
	}
	return Best;
}

void AVeilHeart::EmitWarningForNight(ENightConsequenceType NightType)
{
	if (const FVeilHeartWarningFragment* Warning = FindWarningForNight(NightType))
	{
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dusk warning [%s] for night %s"),
			*Warning->WarningId.ToString(), *GetNightConsequenceTypeDisplayName(NightType));
		OnWarningEmitted(*Warning);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: No catalog warning for night %s (catalog %s)."),
			*GetNightConsequenceTypeDisplayName(NightType),
			WarningCatalog ? TEXT("assigned") : TEXT("missing"));
	}
}

void AVeilHeart::ProcessDawnReflection()
{
	// BP-compat entry point: reflect with no night outcome data.
	ProcessDawnReflectionWithOutcome(FNightRuntimeOutcome());
}

void AVeilHeart::ProcessDawnReflectionWithOutcome(const FNightRuntimeOutcome& Outcome)
{
	const int32 TagsThisCycle = SatisfiedWarningTags.Num();
	LastNightOutcome = Outcome;

	// Distinguish how the night resolved so payoff (and the next cycle) can react meaningfully.
	switch (Outcome.Result)
	{
	case ENightOutcomeResult::Success:
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dawn Reflection - the sanctuary held (%s). %d warning tag(s) heeded; night '%s' resolved."),
			*Outcome.ResultTag.ToString(), TagsThisCycle, *GetNightConsequenceTypeDisplayName(Outcome.NightType));
		break;
	case ENightOutcomeResult::Partial:
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dawn Reflection - the dark receded but lingers (%s). Bloom reduced by %.2f; %d tag(s) heeded."),
			*Outcome.ResultTag.ToString(), -Outcome.TargetCorruptionDelta, TagsThisCycle);
		break;
	case ENightOutcomeResult::Failure:
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: Dawn Reflection - a scar remains (%s). Bloom worsened by %.2f; the sanctuary carries this into the next night."),
			*Outcome.ResultTag.ToString(), Outcome.TargetCorruptionDelta);
		break;
	case ENightOutcomeResult::None:
	default:
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dawn Reflection - %d warning tag(s) satisfied this cycle (no night outcome)."),
			TagsThisCycle);
		break;
	}

	OnDawnReflection(Outcome);

	SatisfiedWarningTags.Empty();
}