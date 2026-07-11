#include "PCG/GloamsteadSanctuaryBootstrap.h"

#include "Components/BoxComponent.h"
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

	// PCG schedules a component only if its actor has valid bounds; this box is the bounds source.
	// Without it: LogPCG "Component has invalid bounds, not registered" and zero points generated.
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetupAttachment(SceneRoot);
	Bounds->SetBoxExtent(FVector(800.0f, 800.0f, 400.0f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);

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

	// Load-on-start: if a prior save exists, restore the persisted per-point state over the fresh baseline.
	if (PCGSubsystem->LoadFromSlot(UGloamsteadPCGSubsystem::DefaultSaveSlot))
	{
		UE_LOG(LogTemp, Log, TEXT("GloamsteadSanctuaryBootstrap '%s': loaded saved sanctuary state (slot=%s)."),
			*GetName(), *UGloamsteadPCGSubsystem::DefaultSaveSlot);
	}

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
