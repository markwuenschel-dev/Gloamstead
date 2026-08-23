#include "Actors/GloamsteadEvidenceSource.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/VeilHeart.h"

AGloamsteadEvidenceSource::AGloamsteadEvidenceSource()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->InitSphereRadius(100.0f);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldStatic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(false);

	InteractionPrompt = NSLOCTEXT("Gloamstead", "EvidenceSourceExamine", "Study the sign");
}

bool AGloamsteadEvidenceSource::CanInteract_Implementation(AActor* /*Interactor*/) const
{
	return WarningId != NAME_None && SupportId != NAME_None && ChannelType != NAME_None;
}

FText AGloamsteadEvidenceSource::GetInteractionPrompt_Implementation() const
{
	return InteractionPrompt.IsEmpty()
		? NSLOCTEXT("Gloamstead", "EvidenceSourceExamineFallback", "Study the sign")
		: InteractionPrompt;
}

void AGloamsteadEvidenceSource::Interact_Implementation(AActor* Interactor)
{
	ReportEncounter(Interactor);
}

void AGloamsteadEvidenceSource::Examine_Implementation(AActor* Interactor)
{
	ReportEncounter(Interactor);
}

bool AGloamsteadEvidenceSource::ReportEncounter(AActor* /*Interactor*/)
{
	UWorld* World = GetWorld();
	if (!World || WarningId == NAME_None || SupportId == NAME_None || ChannelType == NAME_None)
	{
		return false;
	}

	TArray<AActor*> Hearts;
	UGameplayStatics::GetAllActorsOfClass(World, AVeilHeart::StaticClass(), Hearts);
	if (Hearts.Num() != 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("EvidenceSource: support %s cannot be encountered because Heart ownership is ambiguous (%d Hearts)."),
			*SupportId.ToString(), Hearts.Num());
		return false;
	}

	AVeilHeart* Heart = Cast<AVeilHeart>(Hearts[0]);
	return Heart && Heart->RecordSupportEncounterFromEvidenceSource(this);
}
