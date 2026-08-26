// The night threat roster and the one rule every threat shares: light answers it.
//
// Two things are pinned here that the design would quietly lose otherwise.
//
// The first is the ceiling. "1-3 light-vulnerable enemies acting as pressure while you cleanse or
// activate" is the locked combat constraint, and the way that constraint dies is not a decision -
// it is arithmetic: a roster that adds one for the overread and one for the climax and one for the
// missed warning, until the last night is an action game nobody chose to make.
//
// The second is that the sharper reading buys light, not immunity. An Insight reading that deleted
// the night's threat would make the second clause a skip button, and the whole mechanic rests on it
// being an advantage instead.
#include "Misc/AutomationTest.h"

#include "Actors/GloamsteadNightThreat.h"
#include "Data/NightThreatTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 CountOf(const FNightThreatRoster& Roster, ENightThreatArchetype Archetype)
	{
		int32 Total = 0;
		for (const FNightThreatSpec& Spec : Roster.Specs)
		{
			if (Spec.Archetype == Archetype)
			{
				Total += Spec.Count;
			}
		}
		return Total;
	}

	FNightThreatRoster RosterFor(ENightConsequenceType Night, EExperienceReadingGrade Grade)
	{
		return BuildNightThreatRoster(Night, Grade, /*bWarningHeeded*/ true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightThreatEarlyCyclesAreUnarmedTest,
	"Gloamstead.NightThreat.EarlyCyclesFieldNoThreatAtAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightThreatEarlyCyclesAreUnarmedTest::RunTest(const FString& /*Parameters*/)
{
	// Cycles I and II teach cause and effect, and an enemy on those nights would teach the player to
	// watch the enemy. Combat deliberately enters at Cycle III as a thief and at Cycle IV as a fight.
	for (const EExperienceReadingGrade Grade : {
			EExperienceReadingGrade::Unread,
			EExperienceReadingGrade::Insight,
			EExperienceReadingGrade::Plain,
			EExperienceReadingGrade::Overreach })
	{
		TestTrue(TEXT("the tutorial night fields no threat"), RosterFor(ENightConsequenceType::Tutorial, Grade).IsEmpty());
		TestTrue(TEXT("the corruption night fields no threat"), RosterFor(ENightConsequenceType::Corruption, Grade).IsEmpty());
		TestTrue(TEXT("the omen night fields no threat"), RosterFor(ENightConsequenceType::Omen, Grade).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightThreatRosterMatchesTheArcTest,
	"Gloamstead.NightThreat.EachCycleFieldsTheThreatItTeaches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightThreatRosterMatchesTheArcTest::RunTest(const FString& /*Parameters*/)
{
	const FNightThreatRoster Road = RosterFor(ENightConsequenceType::Retrieval, EExperienceReadingGrade::Plain);
	TestEqual(TEXT("Cycle III fields exactly one threat"), Road.TotalCount(), 1);
	TestEqual(TEXT("and it is the Gatherer"), CountOf(Road, ENightThreatArchetype::Gatherer), 1);

	const FNightThreatRoster Overlook = RosterFor(ENightConsequenceType::SilencePossession, EExperienceReadingGrade::Plain);
	TestEqual(TEXT("Cycle IV fields exactly one threat"), Overlook.TotalCount(), 1);
	TestEqual(TEXT("and it is the Borrowed"), CountOf(Overlook, ENightThreatArchetype::Borrowed), 1);

	const FNightThreatRoster Bell = RosterFor(ENightConsequenceType::Bargain, EExperienceReadingGrade::Plain);
	TestEqual(TEXT("Cycle V fields exactly one threat"), Bell.TotalCount(), 1);
	TestEqual(TEXT("and it is the Bargainer"), CountOf(Bell, ENightThreatArchetype::Bargainer), 1);

	// The climax recombines what the player already knows how to read. No new archetype on the last
	// night is the whole point - if this ever grows a fifth family, the arc has lost its argument.
	const FNightThreatRoster Siege = RosterFor(ENightConsequenceType::TrueSiege, EExperienceReadingGrade::Plain);
	TestEqual(TEXT("the climax fields exactly three threats"), Siege.TotalCount(), 3);
	TestEqual(TEXT("one Gatherer"), CountOf(Siege, ENightThreatArchetype::Gatherer), 1);
	TestEqual(TEXT("one Borrowed"), CountOf(Siege, ENightThreatArchetype::Borrowed), 1);
	TestEqual(TEXT("one Echo"), CountOf(Siege, ENightThreatArchetype::Echo), 1);
	TestEqual(TEXT("and no Bargainer, because the bell already taught that lesson"),
		CountOf(Siege, ENightThreatArchetype::Bargainer), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightThreatReadingShapesPressureTest,
	"Gloamstead.NightThreat.TheSecondReadingBuysLightNotImmunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightThreatReadingShapesPressureTest::RunTest(const FString& /*Parameters*/)
{
	const FNightThreatRoster Plain = RosterFor(ENightConsequenceType::Bargain, EExperienceReadingGrade::Plain);
	const FNightThreatRoster Sharp = RosterFor(ENightConsequenceType::Bargain, EExperienceReadingGrade::Insight);
	const FNightThreatRoster Over = RosterFor(ENightConsequenceType::Bargain, EExperienceReadingGrade::Overreach);

	// The sharper reading does not delete the night.
	TestEqual(TEXT("the sharper read still faces the night's own threat"), Sharp.TotalCount(), Plain.TotalCount());
	if (!Plain.Specs.IsEmpty() && !Sharp.Specs.IsEmpty())
	{
		TestTrue(TEXT("but it is held off at a lower light level, which the player's own restorations reach"),
			Sharp.Specs[0].RepelledAtLightLevel < Plain.Specs[0].RepelledAtLightLevel);
	}

	// "Three answers invite company." The overread is the only thing that adds a body.
	TestEqual(TEXT("the overread calls exactly one more manifestation"), Over.TotalCount(), Plain.TotalCount() + 1);
	TestEqual(TEXT("and the company is an Echo"), CountOf(Over, ENightThreatArchetype::Echo), 1);

	// A warning that was never read does not add a body either; it removes the slack.
	const FNightThreatRoster Unheeded = BuildNightThreatRoster(
		ENightConsequenceType::Retrieval, EExperienceReadingGrade::Plain, /*bWarningHeeded*/ false);
	const FNightThreatRoster Heeded = BuildNightThreatRoster(
		ENightConsequenceType::Retrieval, EExperienceReadingGrade::Plain, /*bWarningHeeded*/ true);
	TestEqual(TEXT("missing the warning does not spawn more threats"), Unheeded.TotalCount(), Heeded.TotalCount());
	if (!Unheeded.Specs.IsEmpty() && !Heeded.Specs.IsEmpty())
	{
		TestTrue(TEXT("it takes more light to hold the same threat off"),
			Unheeded.Specs[0].RepelledAtLightLevel > Heeded.Specs[0].RepelledAtLightLevel);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightThreatCeilingHoldsTest,
	"Gloamstead.NightThreat.NoNightEverExceedsThreeSimultaneousThreats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightThreatCeilingHoldsTest::RunTest(const FString& /*Parameters*/)
{
	// Exhaustive over every night type and every grade, including combinations the arc never
	// authors. The ceiling must be a property of the function, not of the cases it happens to hit.
	const ENightConsequenceType AllNights[] = {
		ENightConsequenceType::Invalid,
		ENightConsequenceType::Tutorial,
		ENightConsequenceType::Corruption,
		ENightConsequenceType::Omen,
		ENightConsequenceType::Retrieval,
		ENightConsequenceType::SilencePossession,
		ENightConsequenceType::Mirror,
		ENightConsequenceType::Bargain,
		ENightConsequenceType::Fracture,
		ENightConsequenceType::TrueSiege,
	};
	const EExperienceReadingGrade AllGrades[] = {
		EExperienceReadingGrade::Unread,
		EExperienceReadingGrade::Insight,
		EExperienceReadingGrade::Plain,
		EExperienceReadingGrade::Overreach,
	};

	for (const ENightConsequenceType Night : AllNights)
	{
		for (const EExperienceReadingGrade Grade : AllGrades)
		{
			for (const bool bHeeded : { true, false })
			{
				const FNightThreatRoster Roster = BuildNightThreatRoster(Night, Grade, bHeeded);
				TestTrue(
					*FString::Printf(TEXT("%s / %s / heeded=%d stays within the design ceiling (%d)"),
						*GetNightConsequenceTypeDisplayName(Night),
						*GetExperienceReadingGradeDisplayName(Grade),
						bHeeded ? 1 : 0,
						Roster.TotalCount()),
					Roster.TotalCount() <= FNightThreatRoster::MaxSimultaneousThreats);

				for (const FNightThreatSpec& Spec : Roster.Specs)
				{
					TestTrue(TEXT("every emitted spec is authored"), Spec.IsAuthored());
					TestTrue(TEXT("and every threat is answerable by some amount of light"),
						Spec.RepelledAtLightLevel > 0.f && Spec.RepelledAtLightLevel <= 1.f);
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamNightThreatLightAdvanceCurveTest,
	"Gloamstead.NightThreat.LightSlowsAThreatAndEnoughLightStopsIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightThreatLightAdvanceCurveTest::RunTest(const FString& /*Parameters*/)
{
	// The pure decision at the centre of every threat, with no world in sight.
	const float Threshold = 0.6f;

	TestEqual(TEXT("full speed in complete darkness"),
		AGloamsteadNightThreat::ComputeAdvanceScale(0.f, Threshold), 1.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("pinned at the threshold"),
		AGloamsteadNightThreat::ComputeAdvanceScale(Threshold, Threshold), 0.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("still pinned past the threshold"),
		AGloamsteadNightThreat::ComputeAdvanceScale(1.f, Threshold), 0.f, KINDA_SMALL_NUMBER);

	// Monotonic, so more light is never worse for the player. This is the property that makes the
	// restoration fantasy legible: adding light always helps, and never surprises.
	float Previous = AGloamsteadNightThreat::ComputeAdvanceScale(0.f, Threshold);
	for (int32 Step = 1; Step <= 20; ++Step)
	{
		const float Light = static_cast<float>(Step) / 20.f;
		const float Scale = AGloamsteadNightThreat::ComputeAdvanceScale(Light, Threshold);
		TestTrue(*FString::Printf(TEXT("advance never increases as light rises (light %.2f)"), Light),
			Scale <= Previous + KINDA_SMALL_NUMBER);
		Previous = Scale;
	}

	TestTrue(TEXT("threshold light repels"), AGloamsteadNightThreat::IsRepelledByLight(Threshold, Threshold));
	TestFalse(TEXT("half the threshold does not"), AGloamsteadNightThreat::IsRepelledByLight(Threshold * 0.5f, Threshold));

	// Out-of-range inputs are clamped rather than trusted; a corrupt light value must not make a
	// threat faster than it can ever legitimately be.
	TestEqual(TEXT("negative light is treated as darkness"),
		AGloamsteadNightThreat::ComputeAdvanceScale(-5.f, Threshold), 1.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("light beyond full still repels"),
		AGloamsteadNightThreat::ComputeAdvanceScale(5.f, Threshold), 0.f, KINDA_SMALL_NUMBER);

	// A threat authored to be stopped by any light at all is a legal, if extreme, authoring choice.
	TestEqual(TEXT("a zero threshold is stopped by any light"),
		AGloamsteadNightThreat::ComputeAdvanceScale(0.01f, 0.f), 0.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("and still moves in true darkness"),
		AGloamsteadNightThreat::ComputeAdvanceScale(0.f, 0.f), 1.f, KINDA_SMALL_NUMBER);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
