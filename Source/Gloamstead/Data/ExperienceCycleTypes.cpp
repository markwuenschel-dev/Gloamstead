#include "Data/ExperienceCycleTypes.h"

FString GetExperienceReadingGradeDisplayName(EExperienceReadingGrade Grade)
{
	switch (Grade)
	{
	case EExperienceReadingGrade::Insight:   return TEXT("Insight");
	case EExperienceReadingGrade::Plain:     return TEXT("Plain");
	case EExperienceReadingGrade::Overreach: return TEXT("Overreach");
	case EExperienceReadingGrade::Unread:
	default:                                 return TEXT("Unread");
	}
}

FExperienceCyclePlan FExperienceCyclePlan::MakeInvalid(int32 InSlot)
{
	FExperienceCyclePlan Plan;
	Plan.Slot = InSlot;
	Plan.Resolution = EExperiencePlanResolution::Invalid;
	return Plan;
}

FExperienceCyclePlan FExperienceCyclePlan::MakeGenericHandoff(int32 InSlot)
{
	FExperienceCyclePlan Plan;
	Plan.Slot = InSlot;
	Plan.Resolution = EExperiencePlanResolution::GenericHandoff;
	return Plan;
}

const FExperienceCycleSecondReading* FExperienceCyclePlan::FindSecondReading(FName InReadingId) const
{
	if (InReadingId == NAME_None)
	{
		return nullptr;
	}

	const FExperienceCycleSecondReading* Match = nullptr;
	for (const FExperienceCycleSecondReading& Reading : SecondReadings)
	{
		if (Reading.ReadingId != InReadingId)
		{
			continue;
		}

		// Two rows sharing an ID are refused exactly like a missing one: the grade would be
		// whichever row happened to be authored first, which is not something a player can read.
		if (Match)
		{
			return nullptr;
		}
		Match = &Reading;
	}
	return Match;
}

bool FExperienceCyclePlan::HasCoherentSecondReadings(FString* OutError) const
{
	auto Fail = [OutError](const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	};

	// No readings at all is a complete, legal answer: the cycle asks only for the minimum.
	if (SecondReadings.IsEmpty())
	{
		return true;
	}

	int32 InsightCount = 0;
	int32 OverreachCount = 0;
	int32 PlainCount = 0;
	TSet<FName> SeenIds;
	TSet<FName> SeenConsequenceTags;

	for (const FExperienceCycleSecondReading& Reading : SecondReadings)
	{
		if (!Reading.IsAuthored())
		{
			return Fail(TEXT("a second reading is missing its id, grade, prompt, or outcome summary"));
		}
		if (SeenIds.Contains(Reading.ReadingId))
		{
			return Fail(TEXT("two second readings share one id, which makes the player choice unresolvable"));
		}
		SeenIds.Add(Reading.ReadingId);

		switch (Reading.Grade)
		{
		case EExperienceReadingGrade::Insight:   ++InsightCount;   break;
		case EExperienceReadingGrade::Overreach: ++OverreachCount; break;
		case EExperienceReadingGrade::Plain:     ++PlainCount;     break;
		default:
			return Fail(TEXT("a second reading carries no grade"));
		}

		if (Reading.Grade == EExperienceReadingGrade::Plain)
		{
			// A middle reading that pays out is not a middle reading.
			if (Reading.ConsequenceTag != NAME_None)
			{
				return Fail(TEXT("a Plain second reading may not carry a consequence tag"));
			}
		}
		else
		{
			if (Reading.ConsequenceTag == NAME_None)
			{
				return Fail(TEXT("an Insight or Overreach second reading must carry a durable consequence tag"));
			}
			if (SeenConsequenceTags.Contains(Reading.ConsequenceTag))
			{
				return Fail(TEXT("two second readings share one consequence tag, so the night cannot tell them apart"));
			}
			SeenConsequenceTags.Add(Reading.ConsequenceTag);
		}
	}

	if (InsightCount != 1 || OverreachCount != 1)
	{
		return Fail(TEXT("a plan that offers second readings needs exactly one Insight and exactly one Overreach"));
	}
	if (PlainCount < 1)
	{
		// Without a defensible middle the sharp reading is a coin flip between reward and scar,
		// which is the random-feeling punishment the Heart contract exists to prevent.
		return Fail(TEXT("a plan that offers second readings needs at least one Plain middle reading"));
	}

	return true;
}

namespace
{
	/**
	 * Builds one authored second-reading triple.
	 *
	 * Every warning from Cycle II onward has the same shape, because the mechanic does: an
	 * imperative naming the minimum restoration, then a contrastive pair naming the sharper read
	 * and the plausible overread. Authoring the triples through one helper makes that promise
	 * structurally true instead of a convention five separate blocks happen to follow.
	 */
	void AddReadingTriple(
		FExperienceCyclePlan& Plan,
		const TCHAR* InsightId, const TCHAR* InsightPrompt, const TCHAR* InsightSummary, const TCHAR* InsightTag,
		const TCHAR* PlainId, const TCHAR* PlainPrompt, const TCHAR* PlainSummary,
		const TCHAR* OverreachId, const TCHAR* OverreachPrompt, const TCHAR* OverreachSummary, const TCHAR* OverreachTag)
	{
		FExperienceCycleSecondReading Insight;
		Insight.ReadingId = InsightId;
		Insight.Grade = EExperienceReadingGrade::Insight;
		Insight.ChoicePrompt = FText::FromString(InsightPrompt);
		Insight.OutcomeSummary = FText::FromString(InsightSummary);
		Insight.ConsequenceTag = InsightTag;
		Plan.SecondReadings.Add(MoveTemp(Insight));

		FExperienceCycleSecondReading Plain;
		Plain.ReadingId = PlainId;
		Plain.Grade = EExperienceReadingGrade::Plain;
		Plain.ChoicePrompt = FText::FromString(PlainPrompt);
		Plain.OutcomeSummary = FText::FromString(PlainSummary);
		Plan.SecondReadings.Add(MoveTemp(Plain));

		FExperienceCycleSecondReading Overreach;
		Overreach.ReadingId = OverreachId;
		Overreach.Grade = EExperienceReadingGrade::Overreach;
		Overreach.ChoicePrompt = FText::FromString(OverreachPrompt);
		Overreach.OutcomeSummary = FText::FromString(OverreachSummary);
		Overreach.ConsequenceTag = OverreachTag;
		Plan.SecondReadings.Add(MoveTemp(Overreach));
	}

	void SetSupports(
		FExperienceCyclePlan& Plan,
		const TCHAR* EnvironmentalId,
		const TCHAR* ObjectReactionId,
		const TCHAR* AudioId)
	{
		Plan.RequiredSupportIds = { EnvironmentalId, ObjectReactionId, AudioId };
		Plan.RequiredSupportChannelTypes = {
			TEXT("Environmental"),
			TEXT("ObjectReaction"),
			TEXT("Audio")
		};
		// Three authored channels, two required. The third is deliberate slack: one clue may be
		// missed or occluded without the warning becoming arbitrary.
		Plan.MinimumDistinctSupportCount = 2;
	}
}

void PopulateDefaultExperienceCyclePlans(UExperienceCycleCatalog& Catalog)
{
	Catalog.AuthoredPlans.Reset();

	// -------------------------------------------------------------------------------------------
	// Cycle I - the corridor and the Heart plaza. Tutorial.
	//
	// Deliberately carries no ritual type, no evidence contract, no receipt and no second reading.
	// Night 1 teaches one thing only: what I restore matters. Asking the player to read a second
	// clause before they have read a first one teaches nothing.
	// -------------------------------------------------------------------------------------------
	FExperienceCyclePlan Tutorial;
	Tutorial.Slot = 1;
	Tutorial.PlanId = TEXT("Cycle1_Tutorial");
	Tutorial.WarningId = TEXT("TutorialLostPath");
	Tutorial.NightType = ENightConsequenceType::Tutorial;
	Tutorial.SemanticSubject = TEXT("courtyard.lantern.first");
	Tutorial.RequiredRestorationTags = { TEXT("LanternPost") };
	Tutorial.VisualStateKey = TEXT("restoration_level");
	Tutorial.OutcomeSummaryKey = TEXT("Cycle1_Tutorial");
	Tutorial.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Tutorial);

	// -------------------------------------------------------------------------------------------
	// Cycle II - the overgrown garden, immediately off the plaza. Corruption.
	//
	// "Wake the roots. Wet earth shelters; bare ash feeds the Gloam."
	// The lesson is that the warning carries more than the minimum. Restoring the bed survives the
	// night; watering it is the sharper read; ashing it is the plausible wrong one. No true enemy
	// yet - the pressure is the corruption itself.
	// -------------------------------------------------------------------------------------------
	FExperienceCyclePlan Garden;
	Garden.Slot = 2;
	Garden.PlanId = TEXT("Cycle2_Garden");
	Garden.WarningId = TEXT("GardenRot");
	Garden.NightType = ENightConsequenceType::Corruption;
	Garden.SemanticSubject = TEXT("Cycle2_Garden");
	Garden.RequiredRestorationTags = { TEXT("GardenBed") };
	Garden.RequiredRitualType = ERitualType::GardenBed;
	SetSupports(Garden,
		TEXT("GardenRot.WitheredVines"),
		TEXT("GardenRot.ColdSoil"),
		TEXT("GardenRot.BellMoths"));
	Garden.InterpretationReceiptId = TEXT("GardenRot.Interpreted");
	AddReadingTriple(Garden,
		TEXT("GardenRot.OpenTheSluice"),
		TEXT("Reopen the old sluice and let the bed drink"),
		TEXT("You let the water back in. Wet earth sheltered the roots, and the rot spread slower for it."),
		TEXT("Boon.GardenAura"),
		TEXT("GardenRot.ClearByHand"),
		TEXT("Clear the dead growth by hand"),
		TEXT("You cleared what was dead and left the soil as you found it. The bed held, and no more."),
		TEXT("GardenRot.AshPurification"),
		TEXT("Empty the ash brazier over the bed to purify it"),
		TEXT("You covered the bed in ash. Bare earth fed the Gloam, and the rot ran ahead of you."),
		TEXT("Scar.AshFed"));
	Garden.VisualStateKey = TEXT("restoration_level");
	Garden.OutcomeSummaryKey = TEXT("Cycle2_Garden");
	Garden.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Garden);

	// -------------------------------------------------------------------------------------------
	// Cycle III - the broken road, back down the original approach. Retrieval.
	//
	// "Give the lantern a road. Loops guard; dead ends invite hands."
	// The lesson is that geometry BETWEEN restorations matters. It is also the first night where the
	// thing in danger is something the player already made: the first lantern.
	// Night pressure: one Gatherer - an objective thief, not yet a combatant.
	// -------------------------------------------------------------------------------------------
	FExperienceCyclePlan Road;
	Road.Slot = 3;
	Road.PlanId = TEXT("Cycle3_Road");
	Road.WarningId = TEXT("RoadUnbound");
	Road.NightType = ENightConsequenceType::Retrieval;
	Road.SemanticSubject = TEXT("Cycle3_Road");
	Road.RequiredRestorationTags = { TEXT("PathPoint") };
	Road.RequiredRitualType = ERitualType::PathPoint;
	SetSupports(Road,
		TEXT("RoadUnbound.BrokenFlagstones"),
		TEXT("RoadUnbound.LeaningWaymark"),
		TEXT("RoadUnbound.DraggingStep"));
	Road.InterpretationReceiptId = TEXT("RoadUnbound.Interpreted");
	AddReadingTriple(Road,
		TEXT("RoadUnbound.CloseTheLoop"),
		TEXT("Lay the last stones so the lit path closes on itself"),
		TEXT("You closed the road into a loop. The circuit relit a segment the dark had taken."),
		TEXT("Boon.PathLoop"),
		TEXT("RoadUnbound.MendTheBreak"),
		TEXT("Mend only the broken span"),
		TEXT("You mended the break and stopped there. The lantern kept its road, and nothing more."),
		TEXT("RoadUnbound.ReachTheOuterGate"),
		TEXT("Carry the road on toward the abandoned outer gate"),
		TEXT("You ran the road out to the old gate and left it open-ended. Something used it to come in."),
		TEXT("Scar.DeadEnd"));
	Road.VisualStateKey = TEXT("restoration_level");
	Road.OutcomeSummaryKey = TEXT("Cycle3_Road");
	Road.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Road);

	// -------------------------------------------------------------------------------------------
	// Cycle IV - the mirror overlook, above the plaza. Silence / Possession.
	//
	// "Raise the mirror. Face stolen light; never show it the Heart."
	// The lesson is that how a restoration is CONFIGURED matters, not only that it exists. From the
	// overlook the player can see plaza, lantern, garden and road at once, which is what makes
	// "point it at the right one" a readable question rather than a guess.
	// Night pressure: The Borrowed - the first true combat pressure.
	// -------------------------------------------------------------------------------------------
	FExperienceCyclePlan Mirror;
	Mirror.Slot = 4;
	Mirror.PlanId = TEXT("Cycle4_Mirror");
	Mirror.WarningId = TEXT("StolenLight");
	Mirror.NightType = ENightConsequenceType::SilencePossession;
	Mirror.SemanticSubject = TEXT("Cycle4_Overlook");
	Mirror.RequiredRestorationTags = { TEXT("MirrorPillar") };
	Mirror.RequiredRitualType = ERitualType::MirrorPillar;
	SetSupports(Mirror,
		TEXT("StolenLight.TwinnedGlow"),
		TEXT("StolenLight.ColdPillarFace"),
		TEXT("StolenLight.BorrowedVoice"));
	Mirror.InterpretationReceiptId = TEXT("StolenLight.Interpreted");
	AddReadingTriple(Mirror,
		TEXT("StolenLight.FaceTheLantern"),
		TEXT("Turn the mirror to face the lantern below"),
		TEXT("You faced the mirror at the stolen light. Its tether stayed visible long enough to cut."),
		TEXT("Boon.TetherExposed"),
		TEXT("StolenLight.FaceTheRoad"),
		TEXT("Turn the mirror along the road"),
		TEXT("You faced the mirror down the road. It showed you the way out, and nothing that mattered."),
		TEXT("StolenLight.FaceTheHeart"),
		TEXT("Turn the mirror inward, toward the Heart"),
		TEXT("You showed the mirror the Heart. Whatever borrows light now knows where the centre is."),
		TEXT("Scar.HeartRevealed"));
	Mirror.VisualStateKey = TEXT("restoration_level");
	Mirror.OutcomeSummaryKey = TEXT("Cycle4_Mirror");
	Mirror.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Mirror);

	// -------------------------------------------------------------------------------------------
	// Cycle V - the bell shrine, at the sanctuary far edge. Bargain.
	//
	// "Wake the bell. One answer frees; three answers invite company."
	// The lesson is timing and restraint. The bell is the first restoration that is an ACTIVE night
	// tool rather than preparation, which is why the overread here is the intuitive one: louder
	// feels safer.
	// Night pressure: The Bargainer, joined by an Echo when the bell was overrung.
	// -------------------------------------------------------------------------------------------
	FExperienceCyclePlan Bell;
	Bell.Slot = 5;
	Bell.PlanId = TEXT("Cycle5_Bell");
	Bell.WarningId = TEXT("BellBargain");
	Bell.NightType = ENightConsequenceType::Bargain;
	Bell.SemanticSubject = TEXT("Cycle5_BellShrine");
	Bell.RequiredRestorationTags = { TEXT("BellShrine") };
	Bell.RequiredRitualType = ERitualType::BellShrine;
	SetSupports(Bell,
		TEXT("BellBargain.WornInscription"),
		TEXT("BellBargain.CrackedClapper"),
		TEXT("BellBargain.AnsweringToll"));
	Bell.InterpretationReceiptId = TEXT("BellBargain.Interpreted");
	AddReadingTriple(Bell,
		TEXT("BellBargain.RingOnce"),
		TEXT("Strike the bell once, on the answering beat"),
		TEXT("You answered once. The resonance carried, repelled what stood in it, and relit what had gone out."),
		TEXT("Boon.Resonance"),
		TEXT("BellBargain.RingTwice"),
		TEXT("Strike the bell twice"),
		TEXT("You struck twice. The shrine woke, and the night neither helped nor hindered you for it."),
		TEXT("BellBargain.RingThrice"),
		TEXT("Strike the bell three times, to be certain"),
		TEXT("You rang three times to be safe. Something answered that had not been invited."),
		TEXT("Scar.CompanyCalled"));
	Bell.VisualStateKey = TEXT("restoration_level");
	Bell.OutcomeSummaryKey = TEXT("Cycle5_Bell");
	Bell.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Bell);

	// -------------------------------------------------------------------------------------------
	// Cycle VI - the whole sanctuary. Fracture into True Siege.
	//
	// "Bind three lights apart. A closed ring holds; a crown breaks."
	// No new spoke of the map. The sanctuary the player has been repairing for five cycles becomes
	// the puzzle, and the climax recombines rules they already know rather than introducing a sixth
	// arbitrary monster.
	// Night pressure: Gatherer + Borrowed + Echo. Three is the ceiling the design locks.
	// -------------------------------------------------------------------------------------------
	FExperienceCyclePlan Siege;
	Siege.Slot = 6;
	Siege.PlanId = TEXT("Cycle6_Siege");
	Siege.WarningId = TEXT("ThreeLights");
	Siege.NightType = ENightConsequenceType::TrueSiege;
	Siege.SemanticSubject = TEXT("Cycle6_Sanctuary");
	Siege.RequiredRestorationTags = { TEXT("AnchorStone") };
	Siege.RequiredRitualType = ERitualType::AnchorStone;
	SetSupports(Siege,
		TEXT("ThreeLights.FractureSeams"),
		TEXT("ThreeLights.LeaningKeystone"),
		TEXT("ThreeLights.RingingSilence"));
	Siege.InterpretationReceiptId = TEXT("ThreeLights.Interpreted");
	AddReadingTriple(Siege,
		TEXT("ThreeLights.ClosedRing"),
		TEXT("Bind the garden, the road and the bell - the far three"),
		TEXT("You bound the outer three into a closed ring. Their light linked and held the fracture seams shut."),
		TEXT("Boon.RingHeld"),
		TEXT("ThreeLights.WideArc"),
		TEXT("Bind three lights in a wide arc"),
		TEXT("You bound an arc, not a ring. It held one flank of the sanctuary and left the other open."),
		TEXT("ThreeLights.CrownOnHeart"),
		TEXT("Bind the three nearest lights, close around the Heart"),
		TEXT("You crowned the Heart with all three. The outer sanctuary collapsed and funnelled everything inward."),
		TEXT("Scar.CrownBroken"));
	Siege.VisualStateKey = TEXT("restoration_level");
	Siege.OutcomeSummaryKey = TEXT("Cycle6_Siege");
	Siege.Resolution = EExperiencePlanResolution::Authored;
	Catalog.AuthoredPlans.Add(Siege);
}

void FExperienceCyclePersistentState::ResetForLegacyReconciliation()
{
    CompletedCycleSlot = 0;
    ArmedPlanId = NAME_None;
    LastPlanId = NAME_None;
    LastOutcomeResultTag = NAME_None;
    ScarTags.Reset();
    bFirstRestCompleted = false;
    SavedPhaseOrdinal = INDEX_NONE;
    bRequiresLegacyReconciliation = true;
	HeartInterpretationState.Reset();
}
