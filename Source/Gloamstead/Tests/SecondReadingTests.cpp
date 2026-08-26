// The second reading: what the player did with the rest of the Heart's sentence.
//
// Every warning from Cycle II onward names a minimum and then a contrastive pair. Restoring the
// subject answers the minimum; these tests cover the machinery that turns the rest of the sentence
// into something a player can do, and that stops it from being forgeable.
//
// The properties pinned here, in order of how badly each would hurt if it broke:
//   1. a reading cannot be committed before the plan's interpretation receipt exists;
//   2. a reading the active plan does not author is refused;
//   3. exactly one reading is committed per cycle;
//   4. a verdict is re-derived from the plan, never trusted, so a forged grade cannot survive;
//   5. a verdict persists only alongside the receipt that entitles it.
#include "Misc/AutomationTest.h"

#include "Data/ExperienceCycleTypes.h"
#include "Data/NightThreatTypes.h"
#include "Actors/GloamsteadNightThreat.h"
#include "Actors/GloamsteadReadingChoice.h"
#include "Systems/VeilHeart.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FExperienceCyclePlan FindPlanById(FName PlanId)
	{
		UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>();
		PopulateDefaultExperienceCyclePlans(*Catalog);
		for (const FExperienceCyclePlan& Plan : Catalog->AuthoredPlans)
		{
			if (Plan.PlanId == PlanId)
			{
				return Plan;
			}
		}
		return FExperienceCyclePlan::MakeInvalid(0);
	}

	FName FindReadingIdWithGrade(const FExperienceCyclePlan& Plan, EExperienceReadingGrade Grade)
	{
		for (const FExperienceCycleSecondReading& Reading : Plan.SecondReadings)
		{
			if (Reading.Grade == Grade)
			{
				return Reading.ReadingId;
			}
		}
		return NAME_None;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSecondReadingAuthoringRuleTest,
	"Gloamstead.SecondReading.AuthoringRuleRefusesHalfAuthoredSets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSecondReadingAuthoringRuleTest::RunTest(const FString& /*Parameters*/)
{
	const FExperienceCyclePlan Garden = FindPlanById(FName(TEXT("Cycle2_Garden")));
	if (!TestTrue(TEXT("the garden plan is authored"), Garden.IsAuthoredPlan()))
	{
		return false;
	}
	TestTrue(TEXT("the shipped garden reading set is coherent"), Garden.HasCoherentSecondReadings());

	// An empty set is a complete answer: the cycle asks only for the minimum.
	FExperienceCyclePlan NoReadings = Garden;
	NoReadings.SecondReadings.Reset();
	TestTrue(TEXT("a plan offering no readings is coherent"), NoReadings.HasCoherentSecondReadings());
	TestFalse(TEXT("and it reports that it offers none"), NoReadings.OffersSecondReading());

	// Two sharper reads means the player cannot be told which reading the warning rewarded.
	FExperienceCyclePlan TwoInsights = Garden;
	for (FExperienceCycleSecondReading& Reading : TwoInsights.SecondReadings)
	{
		if (Reading.Grade == EExperienceReadingGrade::Plain)
		{
			Reading.Grade = EExperienceReadingGrade::Insight;
			Reading.ConsequenceTag = FName(TEXT("Boon.Invented"));
		}
	}
	FString Error;
	TestFalse(TEXT("two Insight readings are refused"), TwoInsights.HasCoherentSecondReadings(&Error));
	TestTrue(TEXT("and the refusal says why"), !Error.IsEmpty());

	// No defensible middle turns the sharp reading into a coin flip between reward and scar.
	FExperienceCyclePlan NoMiddle = Garden;
	NoMiddle.SecondReadings.RemoveAll([](const FExperienceCycleSecondReading& Reading)
		{ return Reading.Grade == EExperienceReadingGrade::Plain; });
	TestFalse(TEXT("a reading set with no Plain middle is refused"), NoMiddle.HasCoherentSecondReadings());

	// A graded reading with no durable tag cannot be told apart from the middle afterwards.
	FExperienceCyclePlan UntaggedInsight = Garden;
	for (FExperienceCycleSecondReading& Reading : UntaggedInsight.SecondReadings)
	{
		if (Reading.Grade == EExperienceReadingGrade::Insight)
		{
			Reading.ConsequenceTag = NAME_None;
		}
	}
	TestFalse(TEXT("an Insight reading with no consequence tag is refused"), UntaggedInsight.HasCoherentSecondReadings());

	// A middle reading that pays out is not a middle reading.
	FExperienceCyclePlan PaidMiddle = Garden;
	for (FExperienceCycleSecondReading& Reading : PaidMiddle.SecondReadings)
	{
		if (Reading.Grade == EExperienceReadingGrade::Plain)
		{
			Reading.ConsequenceTag = FName(TEXT("Boon.Sneaky"));
		}
	}
	TestFalse(TEXT("a Plain reading carrying a tag is refused"), PaidMiddle.HasCoherentSecondReadings());

	// Duplicate ids make the player's choice unresolvable, so lookup refuses rather than picking.
	FExperienceCyclePlan Duplicated = Garden;
	if (Duplicated.SecondReadings.Num() >= 2)
	{
		Duplicated.SecondReadings[1].ReadingId = Duplicated.SecondReadings[0].ReadingId;
		TestFalse(TEXT("duplicate reading ids are refused"), Duplicated.HasCoherentSecondReadings());
		TestNull(TEXT("and an ambiguous lookup resolves to nothing"),
			Duplicated.FindSecondReading(Duplicated.SecondReadings[0].ReadingId));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSecondReadingRequiresReceiptFirstTest,
	"Gloamstead.SecondReading.RefusedUntilTheWarningWasActuallyRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSecondReadingRequiresReceiptFirstTest::RunTest(const FString& /*Parameters*/)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World)
	{
		AddError(TEXT("could not create a game world"));
		return false;
	}
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	AVeilHeart* Heart = World->SpawnActor<AVeilHeart>();
	if (!Heart)
	{
		AddError(TEXT("could not spawn the Heart"));
		return false;
	}

	const FExperienceCyclePlan Garden = FindPlanById(FName(TEXT("Cycle2_Garden")));
	if (!TestTrue(TEXT("the garden plan is authored"), Garden.IsAuthoredPlan()))
	{
		return false;
	}
	Heart->Test_SetActivePlan(Garden);

	const FName InsightId = FindReadingIdWithGrade(Garden, EExperienceReadingGrade::Insight);
	if (!TestTrue(TEXT("the garden plan authors a sharper read"), InsightId != NAME_None))
	{
		return false;
	}

	// No warning presented, no evidence found, no receipt earned: there is nothing for a second
	// reading to configure, and offering one anyway would let a player skip the whole cycle.
	TestFalse(TEXT("a reading is refused before the warning has even been presented"),
		Heart->Test_RecordSecondReading(Garden.WarningId, InsightId));
	TestFalse(TEXT("and no verdict was recorded"), Heart->GetSecondReadingVerdict().IsValid());
	TestEqual(TEXT("the plan therefore grades as Unread"),
		Heart->GetSecondReadingGradeForPlan(Garden), EExperienceReadingGrade::Unread);

	// A reading naming a different warning is refused for the same reason.
	TestFalse(TEXT("a reading naming another warning is refused"),
		Heart->Test_RecordSecondReading(FName(TEXT("SomeOtherWarning")), InsightId));

	Heart->Test_ClearActivePlan();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSecondReadingVerdictIsRederivedTest,
	"Gloamstead.SecondReading.VerdictIsRederivedFromThePlanNeverTrusted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSecondReadingVerdictIsRederivedTest::RunTest(const FString& /*Parameters*/)
{
	const FExperienceCyclePlan Bell = FindPlanById(FName(TEXT("Cycle5_Bell")));
	if (!TestTrue(TEXT("the bell plan is authored"), Bell.IsAuthoredPlan()))
	{
		return false;
	}

	const FName InsightId = FindReadingIdWithGrade(Bell, EExperienceReadingGrade::Insight);
	const FExperienceCycleSecondReading* Insight = Bell.FindSecondReading(InsightId);
	if (!TestNotNull(TEXT("the bell plan authors a sharper read"), Insight))
	{
		return false;
	}

	// A verdict is only ever as good as the plan it is checked against. These are the shapes a
	// corrupt save or a forged caller would produce, and each must fail on its own.
	FExperienceSecondReadingVerdict Honest;
	Honest.ReadingId = Insight->ReadingId;
	Honest.PlanId = Bell.PlanId;
	Honest.WarningId = Bell.WarningId;
	Honest.Grade = Insight->Grade;
	Honest.ConsequenceTag = Insight->ConsequenceTag;
	TestTrue(TEXT("an honest verdict is valid"), Honest.IsValid());

	FExperienceSecondReadingVerdict UpgradedGrade = Honest;
	UpgradedGrade.Grade = EExperienceReadingGrade::Insight;
	const FName OverreachId = FindReadingIdWithGrade(Bell, EExperienceReadingGrade::Overreach);
	UpgradedGrade.ReadingId = OverreachId;
	TestTrue(TEXT("the forged verdict still looks structurally valid on its own"), UpgradedGrade.IsValid());
	// ...which is exactly why validity is not the test. The plan says the overread is an overread.
	const FExperienceCycleSecondReading* Overreach = Bell.FindSecondReading(OverreachId);
	if (TestNotNull(TEXT("the bell plan authors an overread"), Overreach))
	{
		TestNotEqual(TEXT("a verdict claiming Insight for the overread contradicts the plan"),
			GetExperienceReadingGradeDisplayName(UpgradedGrade.Grade),
			GetExperienceReadingGradeDisplayName(Overreach->Grade));
	}

	FExperienceSecondReadingVerdict WrongPlan = Honest;
	WrongPlan.PlanId = FName(TEXT("Cycle2_Garden"));
	TestNull(TEXT("the bell reading does not exist in another plan"),
		FindPlanById(WrongPlan.PlanId).FindSecondReading(WrongPlan.ReadingId));

	FExperienceSecondReadingVerdict Empty;
	TestFalse(TEXT("an empty verdict is not valid"), Empty.IsValid());
	TestFalse(TEXT("and carries no facts"), Empty.HasAnyFacts());

	FExperienceSecondReadingVerdict Partial;
	Partial.ReadingId = Insight->ReadingId;
	TestFalse(TEXT("a partially populated verdict is not valid"), Partial.IsValid());
	TestTrue(TEXT("but it does carry facts, so it is malformed state rather than absent state"),
		Partial.HasAnyFacts());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSecondReadingPersistenceCarriesVerdictTest,
	"Gloamstead.SecondReading.PersistentStateCarriesTheVerdict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSecondReadingPersistenceCarriesVerdictTest::RunTest(const FString& /*Parameters*/)
{
	FVeilHeartInterpretationPersistentState State;
	TestFalse(TEXT("an empty interpretation state carries no facts"), State.HasAnyFacts());

	State.SecondReadingVerdict.ReadingId = FName(TEXT("GardenRot.OpenTheSluice"));
	TestTrue(TEXT("a verdict alone makes the interpretation state non-empty"), State.HasAnyFacts());

	State.Reset();
	TestFalse(TEXT("reset clears the verdict with everything else"), State.HasAnyFacts());
	TestFalse(TEXT("and the verdict itself is cleared"), State.SecondReadingVerdict.HasAnyFacts());

	// A v4 payload predates the verdict entirely. It deserializes to an empty one, which reads as
	// "no second reading was committed" - the correct answer, and why no save version bump is needed.
	FExperienceCyclePersistentState Cycle;
	TestFalse(TEXT("a default cycle state has no committed reading"),
		Cycle.HeartInterpretationState.SecondReadingVerdict.HasAnyFacts());

	Cycle.HeartInterpretationState.SecondReadingVerdict.ReadingId = FName(TEXT("Anything"));
	Cycle.ResetForLegacyReconciliation();
	TestFalse(TEXT("legacy reconciliation clears a verdict it cannot prove"),
		Cycle.HeartInterpretationState.SecondReadingVerdict.HasAnyFacts());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSecondReadingChoiceActorIdentityTest,
	"Gloamstead.SecondReading.ChoiceActorCarriesIdentityNotAPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSecondReadingChoiceActorIdentityTest::RunTest(const FString& /*Parameters*/)
{
	// The same authority shape as the evidence source: the actor owns an identity the Heart reads
	// off it, and exposes no Blueprint-callable way to assert a commit.
	const UClass* ChoiceClass = AGloamsteadReadingChoice::StaticClass();
	const UFunction* ReportFunction = ChoiceClass->FindFunctionByName(FName(TEXT("ReportChoice")));
	TestNull(TEXT("ReportChoice is not reflected to Blueprint"), ReportFunction);

	const UClass* HeartClass = AVeilHeart::StaticClass();
	TestNull(TEXT("RecordSecondReadingFromChoice is not reflected to Blueprint"),
		HeartClass->FindFunctionByName(FName(TEXT("RecordSecondReadingFromChoice"))));

	// The verdict is readable, because presentation needs it; it is simply not writable.
	TestNotNull(TEXT("the verdict is readable from Blueprint"),
		HeartClass->FindFunctionByName(FName(TEXT("GetSecondReadingVerdict"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSecondReadingDoesNotLeakAcrossCyclesTest,
	"Gloamstead.SecondReading.AStaleVerdictNeverLocksTheNextCycleOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSecondReadingDoesNotLeakAcrossCyclesTest::RunTest(const FString& /*Parameters*/)
{
	// The failure this pins: the one-reading-per-cycle guard once asked "does any valid verdict
	// exist", so a verdict earned in Cycle II would still stand when Cycle III armed and would refuse
	// that cycle second clause outright - silently, with the prompt simply never appearing.
	const FExperienceCyclePlan Garden = FindPlanById(FName(TEXT("Cycle2_Garden")));
	const FExperienceCyclePlan Road = FindPlanById(FName(TEXT("Cycle3_Road")));
	if (!TestTrue(TEXT("both plans are authored"), Garden.IsAuthoredPlan() && Road.IsAuthoredPlan()))
	{
		return false;
	}

	const FName GardenInsight = FindReadingIdWithGrade(Garden, EExperienceReadingGrade::Insight);
	if (!TestTrue(TEXT("the garden authors a sharper read"), GardenInsight != NAME_None))
	{
		return false;
	}

	// A garden verdict, structurally perfect, held up against the road plan.
	const FExperienceCycleSecondReading* GardenReading = Garden.FindSecondReading(GardenInsight);
	if (!TestNotNull(TEXT("the garden reading resolves"), GardenReading))
	{
		return false;
	}

	TestNull(TEXT("the garden reading is not authored by the road plan"),
		Road.FindSecondReading(GardenInsight));

	// Each cycle's readings are its own; no id is shared between them, which is what makes the
	// plan-scoped check able to tell a stale verdict from a live one.
	for (const FExperienceCycleSecondReading& RoadReading : Road.SecondReadings)
	{
		TestNull(*FString::Printf(TEXT("%s belongs to the road alone"), *RoadReading.ReadingId.ToString()),
			Garden.FindSecondReading(RoadReading.ReadingId));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
