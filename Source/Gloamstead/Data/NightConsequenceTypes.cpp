#include "Data/NightConsequenceTypes.h"

FString GetNightConsequenceTypeDisplayName(ENightConsequenceType Type)
{
	switch (Type)
	{
	case ENightConsequenceType::Tutorial:   return TEXT("Tutorial");
	case ENightConsequenceType::Corruption: return TEXT("Corruption");
	case ENightConsequenceType::Omen:       return TEXT("Omen");
	case ENightConsequenceType::Retrieval:  return TEXT("Retrieval");
	case ENightConsequenceType::SilencePossession: return TEXT("Silence Possession");
	case ENightConsequenceType::Mirror:     return TEXT("Mirror");
	case ENightConsequenceType::Bargain:    return TEXT("Bargain");
	case ENightConsequenceType::Fracture:   return TEXT("Fracture");
	case ENightConsequenceType::TrueSiege:  return TEXT("True Siege");
	default:                                return TEXT("Invalid");
	}
}

void PopulateMVPNightConsequenceRules(UNightConsequenceCatalog& Catalog)
{
	Catalog.Rules.Reset();
	Catalog.FallbackNightType = ENightConsequenceType::Corruption;
	Catalog.bForceTutorialOnFirstNight = true;

	FNightConsequenceRule TutorialRule;
	TutorialRule.NightType = ENightConsequenceType::Tutorial;
	TutorialRule.Weight = 10.f;
	TutorialRule.MinAverageLight = 0.f;
	TutorialRule.MaxAverageLight = 1.f;
	TutorialRule.MinAverageCorruption = 0.f;
	TutorialRule.MaxAverageCorruption = 1.f;

	FNightConsequenceRule CorruptionRule;
	CorruptionRule.NightType = ENightConsequenceType::Corruption;
	CorruptionRule.Weight = 5.f;
	CorruptionRule.MinAverageLight = 0.f;
	CorruptionRule.MaxAverageLight = 0.6f;
	CorruptionRule.MinAverageCorruption = 0.15f;
	CorruptionRule.MaxAverageCorruption = 1.f;
	CorruptionRule.FavoredRitualTypes.Add(ERitualType::LanternPost);

	FNightConsequenceRule OmenRule;
	OmenRule.NightType = ENightConsequenceType::Omen;
	OmenRule.Weight = 4.f;
	OmenRule.MinAverageLight = 0.2f;
	OmenRule.MaxAverageLight = 1.f;
	OmenRule.MinAverageCorruption = 0.f;
	OmenRule.MaxAverageCorruption = 0.5f;
	OmenRule.FavoredRitualTypes.Add(ERitualType::GardenBed);
	OmenRule.OmenClueTag = FName(TEXT("GardenRot"));

	Catalog.Rules.Add(TutorialRule);
	Catalog.Rules.Add(CorruptionRule);
	Catalog.Rules.Add(OmenRule);
}
