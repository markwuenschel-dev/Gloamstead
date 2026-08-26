#include "PCG/GloamsteadSanctuaryBootstrap.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "Systems/GloamsteadInterpretationSiteBuilder.h"
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
	ApplyPersistencePolicy();

	BindToPCGComponent();

	if (PCGComponent && PCGComponent->GenerationTrigger != EPCGComponentGenerationTrigger::GenerateOnLoad)
	{
		UE_LOG(LogTemp, Warning, TEXT("GloamsteadSanctuaryBootstrap '%s': PCGComponent is not set to GenerateOnLoad."), *GetName());
	}

	TryInitializeSanctuary();
}

void AGloamsteadSanctuaryBootstrap::ApplyPersistencePolicy()
{
	if (UWorld* World = GetWorld())
	{
		if (UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>())
		{
			DayNight->SetDawnAutosaveEnabled(bEnablePersistence);
		}
	}
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


	// Load-on-start is owned by DayNight so the PCG baseline, authored cycle,
	// first-rest eligibility, and saved phase are restored as one payload.
	if (bEnablePersistence)
	{
		if (UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>())
		{
			if (DayNight->LoadProgressionFromSlot())
			{
				if (DayNight->IsWarningPresentationPending())
				{
					UE_LOG(LogTemp, Log, TEXT("GloamsteadSanctuaryBootstrap '%s': loaded valid sanctuary progression; exact warning presentation is pending a ready Heart (slot=%s)."),
						*GetName(), *UGloamsteadPCGSubsystem::DefaultSaveSlot);
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("GloamsteadSanctuaryBootstrap '%s': loaded full sanctuary progression (slot=%s)."),
						*GetName(), *UGloamsteadPCGSubsystem::DefaultSaveSlot);
				}
			}
		}
	}

	bInitializedSanctuary = true;

	// The authored clues and choices are placed only now, because both need the PCG points that the
	// call above created and the authored site contracts it stamped onto them. Doing it here rather
	// than in the builder's own Initialize is what guarantees that ordering.
	if (bMaterializeInterpretationSites)
	{
		if (UGloamsteadInterpretationSiteBuilder* SiteBuilder = World->GetSubsystem<UGloamsteadInterpretationSiteBuilder>())
		{
			SiteBuilder->MaterializeAuthoredInterpretationSites();
		}
	}

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
