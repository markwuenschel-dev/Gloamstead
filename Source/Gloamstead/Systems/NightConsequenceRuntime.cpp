#include "Systems/NightConsequenceRuntime.h"
#include "Systems/NightConsequenceManager.h"
#include "PCG/GloamsteadPCGSubsystem.h"

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
	ExecuteNightStub();
}

void UNightConsequenceRuntime::ExecuteNightStub()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();

	switch (ActiveNightType)
	{
	case ENightConsequenceType::Corruption:
		if (PCG)
		{
			const float Before = PCG->GetSanctuaryAverageCorruptionLevel();
			const int32 Mutated = PCG->ApplyCorruptionSpread(0.12f, 8);
			const float After = PCG->GetSanctuaryAverageCorruptionLevel();
			UE_LOG(LogTemp, Log, TEXT("NightRuntime: Corruption night spread — %d points, avg %.2f -> %.2f"),
				Mutated, Before, After);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("NightRuntime: Corruption night — PCG subsystem missing."));
		}
		break;
	case ENightConsequenceType::Tutorial:
		UE_LOG(LogTemp, Log, TEXT("NightRuntime: Tutorial night — teaching beat (no spread in stub)."));
		break;
	case ENightConsequenceType::Omen:
		UE_LOG(LogTemp, Log, TEXT("NightRuntime: Omen night — environmental clue stub (delegate in wave-nc-4)."));
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("NightRuntime: No stub for night type %s"),
			*GetNightConsequenceTypeDisplayName(ActiveNightType));
		break;
	}
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