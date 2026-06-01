#include "Systems/VeilHeart.h"
#include "Components/RitualPlacementComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AVeilHeart::AVeilHeart()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AVeilHeart::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), FoundActors);

		for (AActor* Actor : FoundActors)
		{
			if (URitualPlacementComponent* RitualComp = Actor->FindComponentByClass<URitualPlacementComponent>())
			{
				RitualComp->OnRestorationComplete.AddDynamic(this, &AVeilHeart::OnRestorationComplete);
			}
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
	const int32 TagsThisCycle = SatisfiedWarningTags.Num();
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dawn Reflection - %d warning tags satisfied this cycle."), TagsThisCycle);

	SatisfiedWarningTags.Empty();

	// TODO Phase 2: Trigger journal entries, emotional feedback, resource bonuses, etc.
}