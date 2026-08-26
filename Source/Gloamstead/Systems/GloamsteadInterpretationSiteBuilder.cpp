#include "Systems/GloamsteadInterpretationSiteBuilder.h"

#include "Actors/GloamsteadEvidenceSource.h"
#include "Actors/GloamsteadReadingChoice.h"
#include "Data/ExperienceCycleTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/NightConsequenceRuntime.h"

bool UGloamsteadInterpretationSiteBuilder::HasExistingEvidence(FName WarningId, FName SupportId) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<AGloamsteadEvidenceSource> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const AGloamsteadEvidenceSource* Source = *It;
		if (IsValid(Source) && Source->GetWarningId() == WarningId && Source->GetSupportId() == SupportId)
		{
			return true;
		}
	}
	return false;
}

bool UGloamsteadInterpretationSiteBuilder::HasExistingChoice(FName WarningId, FName ReadingId) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<AGloamsteadReadingChoice> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const AGloamsteadReadingChoice* Choice = *It;
		if (IsValid(Choice) && Choice->GetWarningId() == WarningId && Choice->GetReadingId() == ReadingId)
		{
			return true;
		}
	}
	return false;
}

bool UGloamsteadInterpretationSiteBuilder::ResolvePlanAnchor(
	const FExperienceCyclePlan& Plan,
	const UGloamsteadPCGSubsystem* PCG,
	FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	UNightConsequenceRuntime* Runtime = World ? World->GetSubsystem<UNightConsequenceRuntime>() : nullptr;
	if (!Runtime || !PCG)
	{
		return false;
	}

	// The night runtime's resolver is reused deliberately: it returns INDEX_NONE for a missing OR an
	// ambiguous subject and never score-selects a substitute. Clues placed around a guessed point
	// would be worse than no clues at all - they would be a truthful Heart pointing at the wrong place.
	const int32 PointIndex = Runtime->ResolveSemanticSubjectToPoint(Plan.SemanticSubject, PCG);
	if (PointIndex == INDEX_NONE)
	{
		return false;
	}

	FPCGPoint Point;
	if (!PCG->GetPointByIndex(PointIndex, Point))
	{
		return false;
	}

	OutLocation = Point.Transform.GetLocation();
	return true;
}

int32 UGloamsteadInterpretationSiteBuilder::MaterializeAuthoredInterpretationSites()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	const UGameInstance* GameInstance = World->GetGameInstance();
	UGloamsteadExperienceCycleSubsystem* Experience =
		GameInstance ? GameInstance->GetSubsystem<UGloamsteadExperienceCycleSubsystem>() : nullptr;
	const UGloamsteadPCGSubsystem* PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	if (!Experience || !PCG)
	{
		return 0;
	}

	// Every authored plan is materialized at once, not just the armed one. The clues for a later
	// cycle standing in the world before that cycle arms is correct and deliberate: the sanctuary
	// does not rearrange itself between nights, and a player who notices the sluice gate on Night 2
	// has genuinely noticed something.
	UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>(this);
	PopulateDefaultExperienceCyclePlans(*Catalog);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	int32 SpawnedCount = 0;

	for (const FExperienceCyclePlan& Plan : Catalog->AuthoredPlans)
	{
		if (!Plan.IsAuthoredPlan())
		{
			continue;
		}

		FVector Anchor = FVector::ZeroVector;
		if (!ResolvePlanAnchor(Plan, PCG, Anchor))
		{
			UE_LOG(LogTemp, Log,
				TEXT("InterpretationSites: plan %s has no resolvable point for subject %s; its evidence and "
					 "choices are not placed. Author a ritual site for that subject to make this cycle readable."),
				*Plan.PlanId.ToString(), *Plan.SemanticSubject.ToString());
			continue;
		}

		const int32 EvidenceCount = FMath::Min(Plan.RequiredSupportIds.Num(), Plan.RequiredSupportChannelTypes.Num());
		for (int32 Index = 0; Index < EvidenceCount; ++Index)
		{
			const FName SupportId = Plan.RequiredSupportIds[Index];
			const FName ChannelType = Plan.RequiredSupportChannelTypes[Index];
			if (SupportId == NAME_None || ChannelType == NAME_None || HasExistingEvidence(Plan.WarningId, SupportId))
			{
				continue;
			}

			const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(FMath::Max(1, EvidenceCount));
			const FVector Offset(FMath::Cos(Angle) * EvidenceRingRadius, FMath::Sin(Angle) * EvidenceRingRadius, 0.f);

			AGloamsteadEvidenceSource* Source = World->SpawnActor<AGloamsteadEvidenceSource>(
				AGloamsteadEvidenceSource::StaticClass(), Anchor + Offset, FRotator::ZeroRotator, SpawnParams);
			if (!Source)
			{
				continue;
			}

			Source->WarningId = Plan.WarningId;
			Source->SupportId = SupportId;
			Source->ChannelType = ChannelType;
			SpawnedEvidence.Add(Source);
			++SpawnedCount;
		}

		const int32 ChoiceCount = Plan.SecondReadings.Num();
		for (int32 Index = 0; Index < ChoiceCount; ++Index)
		{
			const FExperienceCycleSecondReading& Reading = Plan.SecondReadings[Index];
			if (!Reading.IsAuthored() || HasExistingChoice(Plan.WarningId, Reading.ReadingId))
			{
				continue;
			}

			// Offset by half a step from the evidence ring so a clue and a choice never land on top of
			// one another and fight for the same interaction focus.
			const float Angle = 2.f * PI * (static_cast<float>(Index) + 0.5f) / static_cast<float>(FMath::Max(1, ChoiceCount));
			const FVector Offset(FMath::Cos(Angle) * ChoiceRingRadius, FMath::Sin(Angle) * ChoiceRingRadius, 0.f);

			AGloamsteadReadingChoice* Choice = World->SpawnActor<AGloamsteadReadingChoice>(
				AGloamsteadReadingChoice::StaticClass(), Anchor + Offset, FRotator::ZeroRotator, SpawnParams);
			if (!Choice)
			{
				continue;
			}

			Choice->WarningId = Plan.WarningId;
			Choice->ReadingId = Reading.ReadingId;
			SpawnedChoices.Add(Choice);
			++SpawnedCount;
		}
	}

	if (SpawnedCount > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("InterpretationSites: placed %d evidence source(s) and %d reading choice(s) from the authored plans."),
			SpawnedEvidence.Num(), SpawnedChoices.Num());
	}
	return SpawnedCount;
}
