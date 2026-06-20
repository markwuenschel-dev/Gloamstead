#include "PCG/GloamsteadSanctuaryBootstrap.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "PCGComponent.h"
#include "PCGData.h"

AGloamsteadSanctuaryBootstrap::AGloamsteadSanctuaryBootstrap()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCGComponent"));
	PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnLoad;
}

void AGloamsteadSanctuaryBootstrap::BeginPlay()
{
	Super::BeginPlay();

	BindToPCGComponent();

	if (PCGComponent && PCGComponent->GenerationTrigger != EPCGComponentGenerationTrigger::GenerateOnLoad)
	{
		UE_LOG(LogTemp, Warning, TEXT("GloamsteadSanctuaryBootstrap '%s': PCGComponent is not set to GenerateOnLoad."), *GetName());
	}

	TryInitializeSanctuary();
}

void AGloamsteadSanctuaryBootstrap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromPCGComponent();
	Super::EndPlay(EndPlayReason);
}

bool AGloamsteadSanctuaryBootstrap::TryInitializeSanctuary()
{
	if (bInitializedSanctuary)
	{
		return true;
	}

	if (!PCGComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("GloamsteadSanctuaryBootstrap '%s': missing PCGComponent."), *GetName());
		return false;
	}

	if (!HasGeneratedOutput())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGloamsteadPCGSubsystem* PCGSubsystem = World ? World->GetSubsystem<UGloamsteadPCGSubsystem>() : nullptr;
	if (!PCGSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("GloamsteadSanctuaryBootstrap '%s': missing UGloamsteadPCGSubsystem."), *GetName());
		return false;
	}

	PCGSubsystem->InitializeFromPCGComponent(PCGComponent, WorldSeed);
	bInitializedSanctuary = true;

	UE_LOG(LogTemp, Log, TEXT("GloamsteadSanctuaryBootstrap '%s': initialized sanctuary PCG state with seed %d."), *GetName(), WorldSeed);
	return true;
}

void AGloamsteadSanctuaryBootstrap::BindToPCGComponent()
{
	if (PCGComponent)
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		PCGComponent->OnPCGGraphGeneratedDelegate.AddUObject(this, &AGloamsteadSanctuaryBootstrap::HandlePCGGraphGenerated);
	}
}

void AGloamsteadSanctuaryBootstrap::UnbindFromPCGComponent()
{
	if (PCGComponent)
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
	}
}

void AGloamsteadSanctuaryBootstrap::HandlePCGGraphGenerated(UPCGComponent* GeneratedComponent)
{
	if (GeneratedComponent == PCGComponent)
	{
		TryInitializeSanctuary();
	}
}

bool AGloamsteadSanctuaryBootstrap::HasGeneratedOutput() const
{
	return PCGComponent && PCGComponent->GetGeneratedGraphOutput().TaggedData.Num() > 0;
}
