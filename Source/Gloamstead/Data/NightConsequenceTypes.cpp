#include "Data/NightConsequenceTypes.h"

FString GetNightConsequenceTypeDisplayName(ENightConsequenceType Type)
{
	switch (Type)
	{
	case ENightConsequenceType::Tutorial:   return TEXT("Tutorial");
	case ENightConsequenceType::Corruption: return TEXT("Corruption");
	case ENightConsequenceType::Omen:       return TEXT("Omen");
	default:                                return TEXT("Invalid");
	}
}