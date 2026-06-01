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
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: Restoration received - Ritual: %d, LightDelta: %.2f, CorruptionCleared: %.2f"),
		static_cast<int32>(Payload.RitualType), Payload.LightDelta, Payload.CorruptionCleared);

	if (Payload.RitualType == ERitualType::LanternPost)
	{
		const FName Tag = FName(TEXT("LanternPost"));
		if (!SatisfiedWarningTags.Contains(Tag))
		{
			SatisfiedWarningTags.Add(Tag);
			UE_LOG(LogTemp, Log, TEXT("VeilHeart: LanternPost warning tag satisfied."));
		}
	}
}

void AVeilHeart::ProcessDawnReflection()
{
	const int32 TagsThisCycle = SatisfiedWarningTags.Num();
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dawn Reflection - %d warning tags satisfied this cycle."), TagsThisCycle);

	SatisfiedWarningTags.Empty();

	// TODO Phase 2: Trigger journal entries, emotional feedback, resource bonuses, etc.
}