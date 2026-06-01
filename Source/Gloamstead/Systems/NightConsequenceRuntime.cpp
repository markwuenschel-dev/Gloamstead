#include "Systems/NightConsequenceRuntime.h"
#include "Systems/NightConsequenceManager.h"

void UNightConsequenceRuntime::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceManager* Manager = World->GetSubsystem<UNightConsequenceManager>())
		{
			Manager->OnNightPlanReady.AddDynamic(this, &UNightConsequenceRuntime::HandleNightPlanReady);
		}
	}
}

void UNightConsequenceRuntime::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UNightConsequenceManager* Manager = World->GetSubsystem<UNightConsequenceManager>())
		{
			Manager->OnNightPlanReady.RemoveDynamic(this, &UNightConsequenceRuntime::HandleNightPlanReady);
		}
	}

	Super::Deinitialize();
}

void UNightConsequenceRuntime::HandleNightPlanReady(ENightConsequenceType SelectedNightType)
{
	PlannedNightType = SelectedNightType;
	UE_LOG(LogTemp, Log, TEXT("NightRuntime: Plan ready for %s"),
		*GetNightConsequenceTypeDisplayName(PlannedNightType));
}

void UNightConsequenceRuntime::BeginNight()
{
	if (bNightActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("NightRuntime: BeginNight ignored — night already active."));
		return;
	}

	if (PlannedNightType == ENightConsequenceType::Invalid)
	{
		if (UWorld* World = GetWorld())
		{
			if (UNightConsequenceManager* Manager = World->GetSubsystem<UNightConsequenceManager>())
			{
				PlannedNightType = Manager->GetLastSelectedNightType();
			}
		}
	}

	ActiveNightType = PlannedNightType;
	bNightActive = true;

	UE_LOG(LogTemp, Log, TEXT("NightRuntime: Night started — type %s"),
		*GetNightConsequenceTypeDisplayName(ActiveNightType));

	OnNightStarted.Broadcast(ActiveNightType);
}

void UNightConsequenceRuntime::EndNight()
{
	if (!bNightActive)
	{
		return;
	}

	const ENightConsequenceType EndedType = ActiveNightType;

	UE_LOG(LogTemp, Log, TEXT("NightRuntime: Night ended — type %s"),
		*GetNightConsequenceTypeDisplayName(EndedType));

	OnNightEnded.Broadcast(EndedType);

	bNightActive = false;
	ActiveNightType = ENightConsequenceType::Invalid;
}