#include "Actors/GloamsteadReadingChoice.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/VeilHeart.h"

AGloamsteadReadingChoice::AGloamsteadReadingChoice()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->InitSphereRadius(120.0f);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldStatic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(false);
}

AVeilHeart* AGloamsteadReadingChoice::ResolveSoleHeart() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Hearts;
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	if (Hearts.Num() != 1)
	{
		return nullptr;
	}
	return Cast<AVeilHeart>(Hearts[0]);
}

bool AGloamsteadReadingChoice::CanInteract_Implementation(AActor* /*Interactor*/) const
{
	if (WarningId == NAME_None || ReadingId == NAME_None)
	{
		return false;
	}

	// Offer the verb only when it can actually be taken. A prompt the player can press that silently
	// does nothing is worse than no prompt: it teaches that the sanctuary does not respond, which is
	// exactly the wrong lesson for the mechanic this actor exists to teach.
	const UWorld* World = GetWorld();
	const UGloamsteadDayNightSubsystem* DayNight = World ? World->GetSubsystem<UGloamsteadDayNightSubsystem>() : nullptr;
	const FExperienceCyclePlan* Plan = DayNight ? DayNight->GetUpcomingPlan() : nullptr;
	if (!Plan || !Plan->IsAuthoredPlan() || Plan->WarningId != WarningId || !Plan->FindSecondReading(ReadingId))
	{
		return false;
	}

	const AVeilHeart* Heart = ResolveSoleHeart();
	if (!Heart || !Heart->HasExactInterpretationForPlan(*Plan))
	{
		// The player has not yet read the warning and restored the place. The second clause is not
		// available to them, and saying so by staying silent is the honest presentation.
		return false;
	}

	// One reading per cycle, scoped to THIS plan. Asking whether any verdict exists would let a
	// verdict left over from the previous cycle hide this cycle prompt entirely.
	return Heart->GetSecondReadingGradeForPlan(*Plan) == EExperienceReadingGrade::Unread;
}

FText AGloamsteadReadingChoice::GetInteractionPrompt_Implementation() const
{
	if (!InteractionPromptOverride.IsEmpty())
	{
		return InteractionPromptOverride;
	}

	const UWorld* World = GetWorld();
	const UGloamsteadDayNightSubsystem* DayNight = World ? World->GetSubsystem<UGloamsteadDayNightSubsystem>() : nullptr;
	if (const FExperienceCyclePlan* Plan = DayNight ? DayNight->GetUpcomingPlan() : nullptr)
	{
		if (const FExperienceCycleSecondReading* Reading = Plan->FindSecondReading(ReadingId))
		{
			return Reading->ChoicePrompt;
		}
	}

	return NSLOCTEXT("Gloamstead", "ReadingChoiceFallback", "Act on what the Heart said");
}

void AGloamsteadReadingChoice::Interact_Implementation(AActor* Interactor)
{
	ReportChoice(Interactor);
}

void AGloamsteadReadingChoice::Examine_Implementation(AActor* Interactor)
{
	// Examining describes the choice without taking it. Committing a night-shaping decision on the
	// look verb would make the sharper reading something a player can trip over by accident.
	const UWorld* World = GetWorld();
	const UGloamsteadDayNightSubsystem* DayNight = World ? World->GetSubsystem<UGloamsteadDayNightSubsystem>() : nullptr;
	const FExperienceCyclePlan* Plan = DayNight ? DayNight->GetUpcomingPlan() : nullptr;
	const FExperienceCycleSecondReading* Reading = Plan ? Plan->FindSecondReading(ReadingId) : nullptr;

	UE_LOG(LogTemp, Log, TEXT("ReadingChoice: examined %s for warning %s - %s"),
		*ReadingId.ToString(),
		*WarningId.ToString(),
		Reading ? *Reading->ChoicePrompt.ToString() : TEXT("(no authored reading for the active plan)"));
}

bool AGloamsteadReadingChoice::ReportChoice(AActor* /*Interactor*/)
{
	if (WarningId == NAME_None || ReadingId == NAME_None)
	{
		return false;
	}

	AVeilHeart* Heart = ResolveSoleHeart();
	if (!Heart)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ReadingChoice: reading %s cannot be committed because Heart ownership is ambiguous."),
			*ReadingId.ToString());
		return false;
	}

	return Heart->RecordSecondReadingFromChoice(this);
}
