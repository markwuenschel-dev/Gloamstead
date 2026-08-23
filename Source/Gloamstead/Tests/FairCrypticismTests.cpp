// Fair-crypticism contract: the GardenRot warning is difficult by observation,
// not arbitrary by implementation. Every near miss below must remain closed.
#include "Misc/AutomationTest.h"

#include "Data/ExperienceCycleTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/GloamsteadFirstNightDirector.h"
#include "Systems/NightConsequenceRuntime.h"
#include "Systems/NightStrategy.h"
#include "Systems/VeilHeart.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FExperienceCyclePlan MakeGardenPlan()
	{
		UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>();
		PopulateDefaultExperienceCyclePlans(*Catalog);
		return Catalog->AuthoredPlans[1];
	}

	FVeilHeartWarningFragment MakeGardenWarning(const FExperienceCyclePlan& Plan)
	{
		FVeilHeartWarningFragment Warning;
		Warning.WarningId = Plan.WarningId;
		Warning.Fragment = FText::FromString(TEXT("What grows in darkness must be tended before the bell tolls."));
		Warning.AssociatedNightType = Plan.NightType;
		Warning.SatisfiableTags = Plan.RequiredRestorationTags;
		Warning.SemanticSubject = Plan.SemanticSubject;
		Warning.RequiredRitualType = Plan.RequiredRitualType;
		Warning.InterpretationReceiptId = Plan.InterpretationReceiptId;
		Warning.ClarityTier = 1;

		const TArray<FName> ChannelTypes = {
			FName(TEXT("Environmental")),
			FName(TEXT("ObjectReaction")),
			FName(TEXT("Audio"))
		};
		const TArray<FText> Evidence = {
			FText::FromString(TEXT("Grey leaves curl toward the eastern bed.")),
			FText::FromString(TEXT("A root-chime answers beside the cracked bed.")),
			FText::FromString(TEXT("Moths gather where soil whispers beneath the bell."))
		};
		for (int32 Index = 0; Index < Plan.RequiredSupportIds.Num(); ++Index)
		{
			FVeilHeartWarningSupportChannel& Channel = Warning.SupportChannels.AddDefaulted_GetRef();
			Channel.SupportId = Plan.RequiredSupportIds[Index];
			Channel.ChannelType = ChannelTypes[Index];
			Channel.EvidenceText = Evidence[Index];
		}
		return Warning;
	}

	FRestorationEventPayload MakeExactGardenRestoration(const FExperienceCyclePlan& Plan)
	{
		FRestorationEventPayload Payload;
		Payload.RitualType = Plan.RequiredRitualType;
		Payload.WarningId = Plan.WarningId;
		Payload.WarningTagSatisfied = Plan.RequiredRestorationTags[0];
		Payload.SemanticSubject = Plan.SemanticSubject;
		Payload.PointIndex = 17;
		return Payload;
	}

	struct FHeartFixture
	{
		FExperienceCyclePlan Plan;
		TObjectPtr<UVeilHeartWarningCatalog> Catalog;
		TObjectPtr<AVeilHeart> Heart;
		TObjectPtr<AGloamsteadFirstNightDirector> Presenter;
		bool bReady = false;
	};

	FHeartFixture MakeHeartFixture(bool bAddSameTypeDecoy = false)
	{
		FHeartFixture Fixture;
		Fixture.Plan = MakeGardenPlan();
		Fixture.Catalog = NewObject<UVeilHeartWarningCatalog>();
		Fixture.Catalog->Warnings.Add(MakeGardenWarning(Fixture.Plan));
		if (bAddSameTypeDecoy)
		{
			FVeilHeartWarningFragment Decoy = MakeGardenWarning(Fixture.Plan);
			Decoy.WarningId = TEXT("GardenRotDecoy");
			Fixture.Catalog->Warnings.Add(Decoy);
		}

		Fixture.Heart = NewObject<AVeilHeart>();
		Fixture.Heart->WarningCatalog = Fixture.Catalog;
		Fixture.Heart->Test_SetActivePlan(Fixture.Plan);
		Fixture.Presenter = NewObject<AGloamsteadFirstNightDirector>();
		Fixture.Heart->OnWarningEmittedDelegate.AddDynamic(Fixture.Presenter, &AGloamsteadFirstNightDirector::HandleHeartWarning);
		Fixture.bReady = Fixture.Heart->RegisterWarningPresenter(
			Fixture.Presenter, GET_FUNCTION_NAME_CHECKED(AGloamsteadFirstNightDirector, HandleHeartWarning))
			&& Fixture.Heart->EmitWarningById(Fixture.Plan.WarningId, Fixture.Plan.NightType);
		return Fixture;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismRequiresDistinctKnownSupportsTest,
	"Gloamstead.FairCrypticism.Supports.RequireDistinctKnownEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismRequiresDistinctKnownSupportsTest::RunTest(const FString& /*Parameters*/)
{
	FHeartFixture OneSupport = MakeHeartFixture();
	if (!TestTrue(TEXT("fixture presents the exact GardenRot warning"), OneSupport.bReady))
	{
		return false;
	}
	TestTrue(TEXT("one known support is recorded"),
		OneSupport.Heart->RecordSupportEncounter(OneSupport.Plan.WarningId, OneSupport.Plan.RequiredSupportIds[0]));
	TestFalse(TEXT("one support cannot produce an interpretation receipt"),
		OneSupport.Heart->EvaluateRestorationAgainstActivePlan(MakeExactGardenRestoration(OneSupport.Plan)));
	TestFalse(TEXT("a duplicate support is not counted twice"),
		OneSupport.Heart->RecordSupportEncounter(OneSupport.Plan.WarningId, OneSupport.Plan.RequiredSupportIds[0]));
	TestFalse(TEXT("a duplicate still cannot produce an interpretation receipt"),
		OneSupport.Heart->EvaluateRestorationAgainstActivePlan(MakeExactGardenRestoration(OneSupport.Plan)));

	FHeartFixture UnknownSupport = MakeHeartFixture();
	if (!TestTrue(TEXT("unknown-support fixture presents GardenRot"), UnknownSupport.bReady))
	{
		return false;
	}
	TestFalse(TEXT("an unknown support id is not accepted"),
		UnknownSupport.Heart->RecordSupportEncounter(UnknownSupport.Plan.WarningId, TEXT("GardenRot.InventedClue")));
	TestTrue(TEXT("one valid support still records after an unknown id"),
		UnknownSupport.Heart->RecordSupportEncounter(UnknownSupport.Plan.WarningId, UnknownSupport.Plan.RequiredSupportIds[0]));
	TestFalse(TEXT("unknown plus one known support remains insufficient"),
		UnknownSupport.Heart->EvaluateRestorationAgainstActivePlan(MakeExactGardenRestoration(UnknownSupport.Plan)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismExactWarningAndRestorationTest,
	"Gloamstead.FairCrypticism.Interpretation.RequiresExactWarningAndGarden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismExactWarningAndRestorationTest::RunTest(const FString& /*Parameters*/)
{
	FHeartFixture Fixture = MakeHeartFixture(/*bAddSameTypeDecoy*/ true);
	if (!TestTrue(TEXT("fixture presents the exact GardenRot warning"), Fixture.bReady))
	{
		return false;
	}

	TestTrue(TEXT("a same-type decoy warning can emit through the exact emitter"),
		Fixture.Heart->EmitWarningById(TEXT("GardenRotDecoy"), Fixture.Plan.NightType));
	TestFalse(TEXT("a same-type/clarity decoy cannot substitute for GardenRot support"),
		Fixture.Heart->RecordSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0]));
	TestTrue(TEXT("the canonical GardenRot warning can be re-presented exactly"),
		Fixture.Heart->EmitWarningById(Fixture.Plan.WarningId, Fixture.Plan.NightType));
	TestTrue(TEXT("first distinct GardenRot support is recorded"),
		Fixture.Heart->RecordSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[0]));
	TestTrue(TEXT("second distinct GardenRot support is recorded"),
		Fixture.Heart->RecordSupportEncounter(Fixture.Plan.WarningId, Fixture.Plan.RequiredSupportIds[1]));

	FRestorationEventPayload WrongWarning = MakeExactGardenRestoration(Fixture.Plan);
	WrongWarning.WarningId = TEXT("GardenRotDecoy");
	TestFalse(TEXT("wrong warning identity cannot earn the GardenRot receipt"),
		Fixture.Heart->EvaluateRestorationAgainstActivePlan(WrongWarning));

	FRestorationEventPayload WrongSubject = MakeExactGardenRestoration(Fixture.Plan);
	WrongSubject.SemanticSubject = TEXT("Cycle2_Elsewhere");
	TestFalse(TEXT("wrong garden subject cannot earn the receipt"),
		Fixture.Heart->EvaluateRestorationAgainstActivePlan(WrongSubject));

	FRestorationEventPayload WrongTag = MakeExactGardenRestoration(Fixture.Plan);
	WrongTag.WarningTagSatisfied = TEXT("Growth");
	TestFalse(TEXT("wrong GardenBed tag cannot earn the receipt"),
		Fixture.Heart->EvaluateRestorationAgainstActivePlan(WrongTag));

	const FRestorationEventPayload ExactRestoration = MakeExactGardenRestoration(Fixture.Plan);
	TestTrue(TEXT("two exact supports plus the exact GardenBed restoration earn a receipt"),
		Fixture.Heart->EvaluateRestorationAgainstActivePlan(ExactRestoration));
	const FExperienceInterpretationReceipt Receipt = Fixture.Heart->GetLastInterpretationReceipt();
	TestTrue(TEXT("the exact receipt is concrete"), Receipt.IsValid());
	TestEqual(TEXT("the receipt retains the canonical id"), Receipt.ReceiptId, Fixture.Plan.InterpretationReceiptId);
	TestEqual(TEXT("the receipt retains the GardenRot warning"), Receipt.WarningId, Fixture.Plan.WarningId);
	TestEqual(TEXT("the receipt retains the stable garden subject"), Receipt.SemanticSubject, Fixture.Plan.SemanticSubject);
	TestEqual(TEXT("the receipt retains GardenBed ritual semantics"), Receipt.RestorationRitualType, ERitualType::GardenBed);
	TestTrue(TEXT("the Heart recognizes its exact active-plan receipt"), Fixture.Heart->HasExactInterpretationForPlan(Fixture.Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismDataContractRejectsNegativeFixturesTest,
	"Gloamstead.FairCrypticism.Data.RejectsSparseDuplicateUnknownAndMismatchedWarnings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismDataContractRejectsNegativeFixturesTest::RunTest(const FString& /*Parameters*/)
{
	const FExperienceCyclePlan Plan = MakeGardenPlan();
	FString Error;
	const FVeilHeartWarningFragment ValidWarning = MakeGardenWarning(Plan);
	TestTrue(TEXT("canonical GardenRot warning matches its plan"), ValidWarning.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment Sparse = ValidWarning;
	Sparse.SupportChannels.SetNum(1);
	TestFalse(TEXT("sparse support arrays are rejected"), Sparse.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment Duplicate = ValidWarning;
	Duplicate.SupportChannels[1].SupportId = Duplicate.SupportChannels[0].SupportId;
	TestFalse(TEXT("duplicate support identifiers are rejected"), Duplicate.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment Unknown = ValidWarning;
	Unknown.SupportChannels[2].SupportId = TEXT("GardenRot.InventedClue");
	TestFalse(TEXT("unknown support identifiers are rejected"), Unknown.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment WrongSubject = ValidWarning;
	WrongSubject.SemanticSubject = TEXT("Cycle2_Elsewhere");
	TestFalse(TEXT("plan-warning subject mismatches are rejected"), WrongSubject.MatchesExactPlanContract(Plan, &Error));

	FVeilHeartWarningFragment WrongTag = ValidWarning;
	WrongTag.SatisfiableTags = { TEXT("Growth") };
	TestFalse(TEXT("plan-warning tag mismatches are rejected"), WrongTag.MatchesExactPlanContract(Plan, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamFairCrypticismMissingSubjectNeverFallsBackTest,
	"Gloamstead.FairCrypticism.Runtime.MissingSubjectNeverFallsBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamFairCrypticismMissingSubjectNeverFallsBackTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadPCGSubsystem* PCG = NewObject<UGloamsteadPCGSubsystem>();
	PCG->Test_SeedPoints({ FVector::ZeroVector, FVector(100.f, 0.f, 0.f) });
	TArray<FRitualPointState> States;
	States.SetNum(2);
	States[0].CorruptionLevel = 0.10f;
	States[1].CorruptionLevel = 0.95f;
	PCG->Test_SeedPointStates(States);

	UNightConsequenceRuntime* Runtime = NewObject<UNightConsequenceRuntime>();
	const int32 GenericMostCorrupted = PCG->FindMostCorruptedPointIndex(/*bOnlyUnrestored*/ true);
	TestEqual(TEXT("fixture exposes a tempting generic bloom"), GenericMostCorrupted, 1);
	const int32 ResolvedSubject = Runtime->ResolveSemanticSubjectToPoint(TEXT("Cycle2_Garden"), PCG);
	TestEqual(TEXT("missing Cycle2_Garden metadata resolves no target"), ResolvedSubject, INDEX_NONE);
	TestNotEqual(TEXT("missing semantic mapping never falls back to the generic bloom"), ResolvedSubject, GenericMostCorrupted);

	FNightRuntimeContext ExactTargetContext;
	ExactTargetContext.NightType = ENightConsequenceType::Corruption;
	ExactTargetContext.bRequiresExactSemanticTarget = true;
	ExactTargetContext.RequiredSemanticSubject = TEXT("Cycle2_Garden");
	ExactTargetContext.TargetPointIndex = ResolvedSubject;
	UNightCorruptionStrategy* Strategy = NewObject<UNightCorruptionStrategy>();
	Strategy->EnterNight(ExactTargetContext, PCG);
	TestEqual(TEXT("the Corruption strategy keeps a missing exact subject untargeted"),
		Strategy->GetObjective().TargetPointIndex, INDEX_NONE);
	TestEqual(TEXT("the Corruption strategy turns a missing exact subject into a quiet objective"),
		Strategy->GetObjective().Kind, ENightObjectiveKind::None);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
