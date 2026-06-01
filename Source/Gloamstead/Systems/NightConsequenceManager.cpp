#include "Systems/NightConsequenceManager.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UNightConsequenceManager::EnsureNightCatalog()
{
	if (NightCatalog)
	{
		return;
	}

	NightCatalog = NewObject<UNightConsequenceCatalog>(this, TEXT("DefaultNightCatalog"));
	PopulateMVPNightConsequenceRules(*NightCatalog);
	UE_LOG(LogTemp, Log, TEXT("NightConsequenceManager: Using built-in MVP night catalog (assign a DA to override)."));
}

void UNightConsequenceManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsureNightCatalog();

	if (UWorld* World = GetWorld())
	{
		if (UGloamsteadPCGSubsystem* Subsystem = World->GetSubsystem<UGloamsteadPCGSubsystem>())
		{
			PCGSubsystem = Subsystem;
			PCGSubsystem->OnStructureRestored.AddDynamic(this, &UNightConsequenceManager::OnStructureRestored);
		}
	}
}

void UNightConsequenceManager::OnStructureRestored(const FRestorationEventPayload& Payload)
{
	if (Payload.PathSegmentID >= 0)
	{
		float& Coverage = PathSegmentLightCoverage.FindOrAdd(Payload.PathSegmentID);
		Coverage += Payload.LightDelta;
	}
}

float UNightConsequenceManager::ScoreRule(const FNightConsequenceRule& Rule, const FNightSanctuarySnapshot& Snapshot) const
{
	if (Rule.NightType == ENightConsequenceType::Invalid)
	{
		return 0.f;
	}

	if (Snapshot.AverageLightLevel < Rule.MinAverageLight || Snapshot.AverageLightLevel > Rule.MaxAverageLight)
	{
		return 0.f;
	}

	if (Snapshot.AverageCorruptionLevel < Rule.MinAverageCorruption || Snapshot.AverageCorruptionLevel > Rule.MaxAverageCorruption)
	{
		return 0.f;
	}

	float Score = Rule.Weight;

	for (ERitualType RitualType : Rule.FavoredRitualTypes)
	{
		switch (RitualType)
		{
		case ERitualType::LanternPost:
			Score += static_cast<float>(Snapshot.LanternPostRestored) * 0.25f;
			break;
		case ERitualType::GardenBed:
			Score += static_cast<float>(Snapshot.GardenBedRestored) * 0.25f;
			break;
		case ERitualType::PathPoint:
			Score += static_cast<float>(Snapshot.PathPointRestored) * 0.25f;
			break;
		default:
			break;
		}
	}

	return Score;
}

ENightConsequenceType UNightConsequenceManager::SelectNightTypeFromCatalog(const FNightSanctuarySnapshot& Snapshot)
{
	if (!NightCatalog || NightCatalog->Rules.Num() == 0)
	{
		return ENightConsequenceType::Corruption;
	}

	const bool bForceTutorial = bForceTutorialOnFirstNight || NightCatalog->bForceTutorialOnFirstNight;
	if (bForceTutorial && NightsPrepared == 0)
	{
		return ENightConsequenceType::Tutorial;
	}

	float BestScore = -1.f;
	ENightConsequenceType BestType = NightCatalog->FallbackNightType;

	for (int32 RuleIndex = 0; RuleIndex < NightCatalog->Rules.Num(); ++RuleIndex)
	{
		const FNightConsequenceRule& Rule = NightCatalog->Rules[RuleIndex];
		const float RuleScore = ScoreRule(Rule, Snapshot);
		if (RuleScore > BestScore || (RuleScore == BestScore && RuleIndex < NightCatalog->Rules.Num()))
		{
			BestScore = RuleScore;
			BestType = Rule.NightType;
		}
	}

	if (BestScore <= 0.f)
	{
		BestType = NightCatalog->FallbackNightType;
	}

	return BestType;
}

void UNightConsequenceManager::PrepareNightConsequences()
{
	EnsureNightCatalog();

	if (!PCGSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("NightConsequenceManager: PCGSubsystem missing; skipping night prep."));
		return;
	}

	const FNightSanctuarySnapshot Snapshot = PCGSubsystem->BuildSanctuarySnapshot();
	LastSelectedNightType = SelectNightTypeFromCatalog(Snapshot);
	++NightsPrepared;

	UE_LOG(LogTemp, Log, TEXT("NightConsequenceManager: Prepared night type %s (avg light=%.2f corruption=%.2f restored=%d)"),
		*GetNightConsequenceTypeDisplayName(LastSelectedNightType),
		Snapshot.AverageLightLevel,
		Snapshot.AverageCorruptionLevel,
		Snapshot.RestoredPointCount);

	OnNightPlanReady.Broadcast(LastSelectedNightType);
}