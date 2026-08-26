#include "Data/NightThreatTypes.h"

FString GetNightThreatArchetypeDisplayName(ENightThreatArchetype Archetype)
{
	switch (Archetype)
	{
	case ENightThreatArchetype::Gatherer:  return TEXT("The Gatherer");
	case ENightThreatArchetype::Borrowed:  return TEXT("The Borrowed");
	case ENightThreatArchetype::Bargainer: return TEXT("The Bargainer");
	case ENightThreatArchetype::Echo:      return TEXT("An Echo");
	case ENightThreatArchetype::None:
	default:                               return TEXT("None");
	}
}

namespace
{
	FNightThreatSpec MakeSpec(
		ENightThreatArchetype Archetype,
		int32 Count,
		ENightThreatTarget Target,
		float RepelledAtLightLevel)
	{
		FNightThreatSpec Spec;
		Spec.Archetype = Archetype;
		Spec.Count = Count;
		Spec.TargetPreference = Target;
		Spec.RepelledAtLightLevel = RepelledAtLightLevel;
		return Spec;
	}

	/**
	 * Clamps the roster to the design ceiling, dropping from the END of the list.
	 *
	 * Order therefore carries meaning: the archetype the night is ABOUT is always authored first, so
	 * an over-full roster loses the extra manifestation rather than the enemy that teaches the lesson.
	 */
	void ClampToDesignCeiling(FNightThreatRoster& Roster)
	{
		int32 Remaining = FNightThreatRoster::MaxSimultaneousThreats;
		for (int32 Index = 0; Index < Roster.Specs.Num(); ++Index)
		{
			FNightThreatSpec& Spec = Roster.Specs[Index];
			Spec.Count = FMath::Clamp(Spec.Count, 0, Remaining);
			Remaining -= Spec.Count;
		}

		Roster.Specs.RemoveAll([](const FNightThreatSpec& Spec) { return !Spec.IsAuthored(); });
	}
}

FNightThreatRoster BuildNightThreatRoster(
	ENightConsequenceType NightType,
	EExperienceReadingGrade ReadingGrade,
	bool bWarningHeeded)
{
	FNightThreatRoster Roster;

	const bool bInsight = ReadingGrade == EExperienceReadingGrade::Insight;
	const bool bOverreach = ReadingGrade == EExperienceReadingGrade::Overreach;

	// An Insight reading buys light, not immunity. Raising the threshold means the sanctuary the
	// player configured holds threats off at a level their existing restorations already reach.
	const float InsightLightBonus = bInsight ? -0.15f : 0.0f;

	switch (NightType)
	{
	case ENightConsequenceType::Tutorial:
	case ENightConsequenceType::Corruption:
	case ENightConsequenceType::Omen:
		// Cycles I and II field no threat at all. The pressure is the corruption, and the lesson is
		// cause and effect - an enemy on those nights would teach the player to watch the enemy.
		break;

	case ENightConsequenceType::Retrieval:
		// Cycle III. One Gatherer, coming for the light the player raised on Night 1.
		Roster.Specs.Add(MakeSpec(
			ENightThreatArchetype::Gatherer, 1,
			ENightThreatTarget::BrightestRestored,
			0.60f + InsightLightBonus));
		if (bOverreach)
		{
			// The dead end toward the outer gate is a shorter approach. Something else uses it.
			Roster.Specs.Add(MakeSpec(
				ENightThreatArchetype::Gatherer, 1,
				ENightThreatTarget::BrightestRestored,
				0.60f));
		}
		break;

	case ENightConsequenceType::SilencePossession:
		// Cycle IV. The Borrowed - the first true combat pressure.
		Roster.Specs.Add(MakeSpec(
			ENightThreatArchetype::Borrowed, 1,
			ENightThreatTarget::ObjectivePoint,
			0.70f + InsightLightBonus));
		if (bOverreach)
		{
			// The mirror was shown the Heart. What learned the centre now sends something to it.
			Roster.Specs.Add(MakeSpec(
				ENightThreatArchetype::Echo, 1,
				ENightThreatTarget::ObjectivePoint,
				0.55f));
		}
		break;

	case ENightConsequenceType::Bargain:
		// Cycle V. The Bargainer stands at the edge of the light and offers a shortcut.
		Roster.Specs.Add(MakeSpec(
			ENightThreatArchetype::Bargainer, 1,
			ENightThreatTarget::EdgeOfLight,
			0.75f + InsightLightBonus));
		if (bOverreach)
		{
			// Three answers invite company. This is the company.
			Roster.Specs.Add(MakeSpec(
				ENightThreatArchetype::Echo, 1,
				ENightThreatTarget::EdgeOfLight,
				0.55f));
		}
		break;

	case ENightConsequenceType::Mirror:
		// The legacy Mirror night keeps its bargain shape; it is resolved by choice, not by force.
		Roster.Specs.Add(MakeSpec(
			ENightThreatArchetype::Bargainer, 1,
			ENightThreatTarget::EdgeOfLight,
			0.75f + InsightLightBonus));
		break;

	case ENightConsequenceType::Fracture:
	case ENightConsequenceType::TrueSiege:
		// Cycle VI. Everything the player already knows how to read, at once: the thief, the
		// possession, and the liar's echo. Deliberately no new archetype on the last night.
		Roster.Specs.Add(MakeSpec(
			ENightThreatArchetype::Gatherer, 1,
			ENightThreatTarget::BrightestRestored,
			0.60f + InsightLightBonus));
		Roster.Specs.Add(MakeSpec(
			ENightThreatArchetype::Borrowed, 1,
			ENightThreatTarget::ObjectivePoint,
			0.70f + InsightLightBonus));
		Roster.Specs.Add(MakeSpec(
			ENightThreatArchetype::Echo, 1,
			ENightThreatTarget::EdgeOfLight,
			0.55f + InsightLightBonus));
		break;

	case ENightConsequenceType::Invalid:
	default:
		break;
	}

	// A warning the player never read means they arrive with no idea what the night wants. That is
	// already its own punishment through the objective; it does not also add a body. What it does do
	// is remove the slack an Insight reading would have bought.
	if (!bWarningHeeded)
	{
		for (FNightThreatSpec& Spec : Roster.Specs)
		{
			Spec.RepelledAtLightLevel = FMath::Min(1.0f, Spec.RepelledAtLightLevel + 0.10f);
		}
	}

	for (FNightThreatSpec& Spec : Roster.Specs)
	{
		Spec.RepelledAtLightLevel = FMath::Clamp(Spec.RepelledAtLightLevel, 0.0f, 1.0f);
	}

	ClampToDesignCeiling(Roster);
	return Roster;
}
