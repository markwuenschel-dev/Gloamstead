#include "Data/NightRuntimeTypes.h"

FString GetNightOutcomeResultDisplayName(ENightOutcomeResult Result)
{
	switch (Result)
	{
	case ENightOutcomeResult::Success: return TEXT("Success");
	case ENightOutcomeResult::Partial: return TEXT("Partial");
	case ENightOutcomeResult::Failure: return TEXT("Failure");
	case ENightOutcomeResult::None:
	default:                           return TEXT("None");
	}
}

FString GetNightObjectiveKindDisplayName(ENightObjectiveKind Kind)
{
	switch (Kind)
	{
	case ENightObjectiveKind::CleanseCorruptionBloom: return TEXT("Cleanse Corruption Bloom");
	case ENightObjectiveKind::TutorialTeach:          return TEXT("Tutorial Teach");
	case ENightObjectiveKind::HeedOmen:               return TEXT("Heed Omen");
	case ENightObjectiveKind::HoldRestored:           return TEXT("Hold Restored");
	case ENightObjectiveKind::None:
	default:                                          return TEXT("None");
	}
}
