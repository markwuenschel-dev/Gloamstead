#include "Systems/GloamsteadWorldStateProjectionSubsystem.h"

#include "Data/ExperienceCycleTypes.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "WorldForgeStateTypes.h"
#include "WorldStateSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
	const FName Cycle2GardenPlanId(TEXT("Cycle2_Garden"));
	const FName Cycle2GardenWarningId(TEXT("GardenRot"));
	const FName Cycle2GardenRestorationTag(TEXT("GardenBed"));
	const FName Cycle2GardenRegionId(TEXT("Cycle2_Garden"));
	const FName RestorationLevelKey(TEXT("restoration_level"));
	const FString GloamsteadMapAsset(TEXT("/Game/Maps/Lvl_Gloamstead"));
	const FString Cycle2GardenAnchorId(TEXT("Cycle2_Garden.Anchor"));
	const FString Cycle2GardenPoiId(TEXT("Cycle2_Garden"));
	const FString SanctuaryBootstrapLocalCoordinateSpace(TEXT("sanctuary_bootstrap_local"));
	const FString Cycle2GenerationInputVersion(TEXT("gloamstead-cycle2-corruption-neglect.v1"));
	const TCHAR* const GeneratedOutputRoot = TEXT("/Game/Generated/WorldForge/Cycle2/");
	constexpr int32 Cycle2GenerationSeed = 42;
	const FVector SanctuaryBootstrapHalfExtent(800.0f, 800.0f, 400.0f);
	const FVector Cycle2GardenAnchorTranslation(480.0f, 160.0f, 0.0f);
	const FVector Cycle2GardenPoiHalfExtent(240.0f, 280.0f, 160.0f);

	const FExperienceCyclePlan& GetCycle2GardenTargetContract()
	{
		// This is intentionally the complete immutable opening-cycle contract,
		// not a ritual-type filter or an active-plan lookup. The active plan may
		// advance after restoration while the physical sanctuary state persists.
		static const FExperienceCyclePlan Target = []
		{
			FExperienceCyclePlan Plan;
			Plan.Slot = 2;
			Plan.PlanId = Cycle2GardenPlanId;
			Plan.WarningId = Cycle2GardenWarningId;
			Plan.NightType = ENightConsequenceType::Corruption;
			Plan.SemanticSubject = Cycle2GardenPlanId;
			Plan.RequiredRestorationTags = { Cycle2GardenRestorationTag };
			Plan.RequiredRitualType = ERitualType::GardenBed;
			Plan.RequiredSupportIds = {
				TEXT("GardenRot.WitheredVines"),
				TEXT("GardenRot.ColdSoil"),
				TEXT("GardenRot.BellMoths")
			};
			Plan.RequiredSupportChannelTypes = {
				TEXT("Environmental"),
				TEXT("ObjectReaction"),
				TEXT("Audio")
			};
			Plan.MinimumDistinctSupportCount = 2;
			Plan.InterpretationReceiptId = TEXT("GardenRot.Interpreted");
			Plan.VisualStateKey = RestorationLevelKey;
			Plan.OutcomeSummaryKey = Cycle2GardenPlanId;
			Plan.Resolution = EExperiencePlanResolution::Authored;
			return Plan;
		}();
		return Target;
	}

	bool FailSpecificationValidation(FString* OutError, const FString& Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	}

	bool TryGetRequiredNonEmptyString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		FString& OutValue,
		FString* OutError)
	{
		if (!Object.IsValid()
			|| !Object->TryGetStringField(FieldName, OutValue)
			|| OutValue.IsEmpty())
		{
			return FailSpecificationValidation(OutError,
				FString::Printf(TEXT("World specification requires non-empty %s."), FieldName));
		}
		return true;
	}

	bool TryGetRequiredObject(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		TSharedPtr<FJsonObject>& OutObject,
		FString* OutError)
	{
		const TSharedPtr<FJsonObject>* ObjectValue = nullptr;
		if (!Object.IsValid()
			|| !Object->TryGetObjectField(FieldName, ObjectValue)
			|| !ObjectValue
			|| !ObjectValue->IsValid())
		{
			return FailSpecificationValidation(OutError,
				FString::Printf(TEXT("World specification requires object %s."), FieldName));
		}
		OutObject = *ObjectValue;
		return true;
	}

	bool TryGetRequiredArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const TArray<TSharedPtr<FJsonValue>>*& OutArray,
		FString* OutError)
	{
		if (!Object.IsValid()
			|| !Object->TryGetArrayField(FieldName, OutArray)
			|| !OutArray
			|| OutArray->IsEmpty())
		{
			return FailSpecificationValidation(OutError,
				FString::Printf(TEXT("World specification requires non-empty array %s."), FieldName));
		}
		return true;
	}

	bool ReadObjectArrayEntry(
		const TSharedPtr<FJsonValue>& Value,
		const TCHAR* ArrayName,
		int32 Index,
		TSharedPtr<FJsonObject>& OutObject,
		FString* OutError)
	{
		const TSharedPtr<FJsonObject>* ObjectValue = nullptr;
		if (!Value.IsValid()
			|| !Value->TryGetObject(ObjectValue)
			|| !ObjectValue
			|| !ObjectValue->IsValid())
		{
			return FailSpecificationValidation(OutError,
				FString::Printf(TEXT("World specification %s[%d] must be an object."), ArrayName, Index));
		}
		OutObject = *ObjectValue;
		return true;
	}

	bool ValidateExactObjectFields(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* ObjectName,
		const TSet<FString>& ExpectedFields,
		FString* OutError)
	{
		if (!Object.IsValid())
		{
			return FailSpecificationValidation(OutError,
				FString::Printf(TEXT("World specification %s must be an object."), ObjectName));
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
		{
			if (!ExpectedFields.Contains(Field.Key))
			{
				return FailSpecificationValidation(OutError,
					FString::Printf(TEXT("World specification %s has unknown property %s."), ObjectName, *Field.Key));
			}
		}

		for (const FString& ExpectedField : ExpectedFields)
		{
			if (!Object->HasField(ExpectedField))
			{
				return FailSpecificationValidation(OutError,
					FString::Printf(TEXT("World specification %s is missing property %s."), ObjectName, *ExpectedField));
			}
		}

		return true;
	}

	bool TryGetFixedNumberArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		int32 ExpectedCount,
		TArray<double>& OutValues,
		FString* OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid()
			|| !Object->TryGetArrayField(FieldName, Values)
			|| !Values
			|| Values->Num() != ExpectedCount)
		{
			return FailSpecificationValidation(OutError,
				FString::Printf(TEXT("World specification requires %s to be a %d-value numeric array."), FieldName, ExpectedCount));
		}

		OutValues.Reset(ExpectedCount);
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			double Value = 0.0;
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetNumber(Value))
			{
				return FailSpecificationValidation(OutError,
					FString::Printf(TEXT("World specification %s[%d] must be numeric."), FieldName, Index));
			}
			OutValues.Add(Value);
		}

		return true;
	}

	bool MatchesExpectedVector(const TArray<double>& Values, const FVector& Expected)
	{
		return Values.Num() == 3
			&& FMath::IsNearlyEqual(static_cast<float>(Values[0]), Expected.X)
			&& FMath::IsNearlyEqual(static_cast<float>(Values[1]), Expected.Y)
			&& FMath::IsNearlyEqual(static_cast<float>(Values[2]), Expected.Z);
	}

}

void UGloamsteadWorldStateProjectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UGloamsteadPCGSubsystem>();
	Collection.InitializeDependency<UWorldStateSubsystem>();
	BindToAuthoritativePCG();
	RebuildFromAuthoritativeState();
}

void UGloamsteadWorldStateProjectionSubsystem::Deinitialize()
{
	UnbindFromAuthoritativePCG();
	Super::Deinitialize();
}

void UGloamsteadWorldStateProjectionSubsystem::RebuildFromAuthoritativeState()
{
	WriteWorldForgeRestorationLevel(DetermineCycle2GardenRestorationLevel());
}

void UGloamsteadWorldStateProjectionSubsystem::HandleStructureRestored(const FRestorationEventPayload& /*Payload*/)
{
	// Payload literals are deliberately discarded. Only the current PCG point
	// metadata and restored state can affect the external generic mirror.
	RebuildFromAuthoritativeState();
}

void UGloamsteadWorldStateProjectionSubsystem::HandleAuthoritativePCGStateRebuilt()
{
	RebuildFromAuthoritativeState();
}

void UGloamsteadWorldStateProjectionSubsystem::BindToAuthoritativePCG()
{
	UnbindFromAuthoritativePCG();

	UWorld* World = GetWorld();
	UGloamsteadPCGSubsystem* PCG = World ? World->GetSubsystem<UGloamsteadPCGSubsystem>() : nullptr;
	if (!PCG)
	{
		return;
	}

	BoundPCG = PCG;
	PCG->OnStructureRestored.AddDynamic(this, &UGloamsteadWorldStateProjectionSubsystem::HandleStructureRestored);
	AuthoritativeStateRebuiltHandle = PCG->AddAuthoritativeStateRebuiltListener(
		FSimpleDelegate::CreateUObject(this, &UGloamsteadWorldStateProjectionSubsystem::HandleAuthoritativePCGStateRebuilt));
}

void UGloamsteadWorldStateProjectionSubsystem::UnbindFromAuthoritativePCG()
{
	if (UGloamsteadPCGSubsystem* PCG = BoundPCG.Get())
	{
		PCG->OnStructureRestored.RemoveDynamic(this, &UGloamsteadWorldStateProjectionSubsystem::HandleStructureRestored);
		PCG->RemoveAuthoritativeStateRebuiltListener(AuthoritativeStateRebuiltHandle);
	}
	AuthoritativeStateRebuiltHandle.Reset();
	BoundPCG.Reset();
}

float UGloamsteadWorldStateProjectionSubsystem::DetermineCycle2GardenRestorationLevel() const
{
	const UGloamsteadPCGSubsystem* PCG = BoundPCG.Get();
	if (!PCG)
	{
		return 0.0f;
	}

	const FExperienceCyclePlan& TargetContract = GetCycle2GardenTargetContract();
	int32 ExactTargetCount = 0;
	bool bExactTargetRestored = false;
	for (int32 PointIndex = 0; PointIndex < PCG->GetRitualPointCount(); ++PointIndex)
	{
		if (!PCG->PointMatchesExperiencePlan(PointIndex, TargetContract))
		{
			continue;
		}

		++ExactTargetCount;
		bExactTargetRestored = PCG->IsPointRestored(PointIndex);
	}

	// A semantic target must be singular. Missing and duplicate exact targets
	// fail closed instead of allowing an arbitrary GardenBed or nearest point.
	return ExactTargetCount == 1 && bExactTargetRestored ? 1.0f : 0.0f;
}

void UGloamsteadWorldStateProjectionSubsystem::WriteWorldForgeRestorationLevel(float RestorationLevel) const
{
	UWorld* World = GetWorld();
	if (UWorldStateSubsystem* WorldState = World ? World->GetSubsystem<UWorldStateSubsystem>() : nullptr)
	{
		WorldState->SetStateValue(
			EWorldForgeStateScope::Region,
			Cycle2GardenRegionId,
			RestorationLevelKey,
			RestorationLevel);
	}
}

bool UGloamsteadWorldStateProjectionSubsystem::ValidateWorldSpecificationJson(
	const FString& SpecificationJson,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SpecificationJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return FailSpecificationValidation(OutError, TEXT("World specification is not a JSON object."));
	}
	static const TSet<FString> RootFields = {
		TEXT("specVersion"), TEXT("worldId"), TEXT("map"), TEXT("output"),
		TEXT("anchors"), TEXT("subjects"), TEXT("poi"), TEXT("generationInput"),
		TEXT("evidence"), TEXT("reactiveCategories"), TEXT("worldState")
	};
	if (!ValidateExactObjectFields(Root, TEXT("root"), RootFields, OutError))
	{
		return false;
	}

	double SpecVersion = 0.0;
	if (!Root->TryGetNumberField(TEXT("specVersion"), SpecVersion) || !FMath::IsNearlyEqual(SpecVersion, 1.0))
	{
		return FailSpecificationValidation(OutError, TEXT("World specification must use semantic specVersion 1."));
	}

	FString WorldId;
	if (!TryGetRequiredNonEmptyString(Root, TEXT("worldId"), WorldId, OutError)
		|| WorldId != Cycle2GardenPlanId.ToString())
	{
		return FailSpecificationValidation(OutError, TEXT("World specification must name Cycle2_Garden."));
	}

	TSharedPtr<FJsonObject> Output;
	FString OutputRoot;
	static const TSet<FString> OutputFields = { TEXT("root") };
	if (!TryGetRequiredObject(Root, TEXT("output"), Output, OutError)
		|| !ValidateExactObjectFields(Output, TEXT("output"), OutputFields, OutError)
		|| !TryGetRequiredNonEmptyString(Output, TEXT("root"), OutputRoot, OutError)
		|| OutputRoot != GeneratedOutputRoot)
	{
		return FailSpecificationValidation(OutError,
			FString::Printf(TEXT("World specification output root must be %s."), GeneratedOutputRoot));
	}

	TSharedPtr<FJsonObject> Map;
	FString MapAsset;
	FString AnchorId;
	static const TSet<FString> MapFields = { TEXT("asset"), TEXT("anchorId") };
	if (!TryGetRequiredObject(Root, TEXT("map"), Map, OutError)
		|| !ValidateExactObjectFields(Map, TEXT("map"), MapFields, OutError)
		|| !TryGetRequiredNonEmptyString(Map, TEXT("asset"), MapAsset, OutError)
		|| !TryGetRequiredNonEmptyString(Map, TEXT("anchorId"), AnchorId, OutError))
	{
		return false;
	}
	if (MapAsset != GloamsteadMapAsset || AnchorId != Cycle2GardenAnchorId)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification map or anchor does not name the Cycle II Gloamstead target."));
	}

	const TArray<TSharedPtr<FJsonValue>>* Anchors = nullptr;
	if (!TryGetRequiredArray(Root, TEXT("anchors"), Anchors, OutError))
	{
		return false;
	}
	if (Anchors->Num() != 1)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification requires exactly one Cycle II garden anchor."));
	}
	static const TSet<FString> AnchorFields = { TEXT("anchorId"), TEXT("mapAsset"), TEXT("surveyId") };
	int32 MatchingMapAnchorCount = 0;
	TSet<FString> SurveyIds;
	TSet<FString> SeenAnchorIds;
	for (int32 Index = 0; Index < Anchors->Num(); ++Index)
	{
		TSharedPtr<FJsonObject> Anchor;
		FString CandidateAnchorId;
		FString CandidateMapAsset;
		FString SurveyId;
		if (!ReadObjectArrayEntry((*Anchors)[Index], TEXT("anchors"), Index, Anchor, OutError)
			|| !ValidateExactObjectFields(Anchor, TEXT("anchors[]"), AnchorFields, OutError)
			|| !TryGetRequiredNonEmptyString(Anchor, TEXT("anchorId"), CandidateAnchorId, OutError)
			|| !TryGetRequiredNonEmptyString(Anchor, TEXT("mapAsset"), CandidateMapAsset, OutError)
			|| !TryGetRequiredNonEmptyString(Anchor, TEXT("surveyId"), SurveyId, OutError))
		{
			return false;
		}
		if (SurveyIds.Contains(SurveyId))
		{
			return FailSpecificationValidation(OutError, TEXT("World specification has a duplicate surveyId."));
		}
		if (SeenAnchorIds.Contains(CandidateAnchorId))
		{
			return FailSpecificationValidation(OutError, TEXT("World specification has an ambiguous anchorId."));
		}
		SurveyIds.Add(SurveyId);
		SeenAnchorIds.Add(CandidateAnchorId);
		MatchingMapAnchorCount += CandidateAnchorId == AnchorId && CandidateMapAsset == MapAsset ? 1 : 0;
	}
	if (MatchingMapAnchorCount != 1)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification map anchor is missing or mismatched."));
	}

	TSharedPtr<FJsonObject> Poi;
	FString PoiId;
	FString PoiAnchorId;
	static const TSet<FString> PoiFields = {
		TEXT("poiId"), TEXT("anchorId"), TEXT("anchorTransform"), TEXT("bounds")
	};
	if (!TryGetRequiredObject(Root, TEXT("poi"), Poi, OutError)
		|| !ValidateExactObjectFields(Poi, TEXT("poi"), PoiFields, OutError)
		|| !TryGetRequiredNonEmptyString(Poi, TEXT("poiId"), PoiId, OutError)
		|| !TryGetRequiredNonEmptyString(Poi, TEXT("anchorId"), PoiAnchorId, OutError))
	{
		return false;
	}
	if (PoiId != Cycle2GardenPoiId || PoiAnchorId != Cycle2GardenAnchorId || PoiAnchorId != AnchorId)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification POI must identify the one Cycle2_Garden anchor."));
	}

	TSharedPtr<FJsonObject> AnchorTransform;
	FString CoordinateSpace;
	TArray<double> TranslationValues;
	static const TSet<FString> AnchorTransformFields = { TEXT("coordinateSpace"), TEXT("translation") };
	if (!TryGetRequiredObject(Poi, TEXT("anchorTransform"), AnchorTransform, OutError)
		|| !ValidateExactObjectFields(AnchorTransform, TEXT("poi.anchorTransform"), AnchorTransformFields, OutError)
		|| !TryGetRequiredNonEmptyString(AnchorTransform, TEXT("coordinateSpace"), CoordinateSpace, OutError)
		|| !TryGetFixedNumberArray(AnchorTransform, TEXT("translation"), 3, TranslationValues, OutError))
	{
		return false;
	}
	if (CoordinateSpace != SanctuaryBootstrapLocalCoordinateSpace
		|| !MatchesExpectedVector(TranslationValues, Cycle2GardenAnchorTranslation))
	{
		return FailSpecificationValidation(OutError, TEXT("World specification POI anchor transform does not match the Cycle II sanctuary-local target."));
	}

	TSharedPtr<FJsonObject> PoiBounds;
	FString BoundsShape;
	TArray<double> HalfExtentValues;
	static const TSet<FString> BoundsFields = { TEXT("shape"), TEXT("halfExtents") };
	if (!TryGetRequiredObject(Poi, TEXT("bounds"), PoiBounds, OutError)
		|| !ValidateExactObjectFields(PoiBounds, TEXT("poi.bounds"), BoundsFields, OutError)
		|| !TryGetRequiredNonEmptyString(PoiBounds, TEXT("shape"), BoundsShape, OutError)
		|| !TryGetFixedNumberArray(PoiBounds, TEXT("halfExtents"), 3, HalfExtentValues, OutError))
	{
		return false;
	}
	if (BoundsShape != TEXT("box")
		|| HalfExtentValues[0] <= 0.0
		|| HalfExtentValues[1] <= 0.0
		|| HalfExtentValues[2] <= 0.0
		|| !MatchesExpectedVector(HalfExtentValues, Cycle2GardenPoiHalfExtent))
	{
		return FailSpecificationValidation(OutError, TEXT("World specification POI bounds must be the positive authored Cycle II box."));
	}

	const FVector AnchorTranslation(
		static_cast<float>(TranslationValues[0]),
		static_cast<float>(TranslationValues[1]),
		static_cast<float>(TranslationValues[2]));
	const FVector HalfExtents(
		static_cast<float>(HalfExtentValues[0]),
		static_cast<float>(HalfExtentValues[1]),
		static_cast<float>(HalfExtentValues[2]));
	if (AnchorTranslation.X - HalfExtents.X < -SanctuaryBootstrapHalfExtent.X
		|| AnchorTranslation.X + HalfExtents.X > SanctuaryBootstrapHalfExtent.X
		|| AnchorTranslation.Y - HalfExtents.Y < -SanctuaryBootstrapHalfExtent.Y
		|| AnchorTranslation.Y + HalfExtents.Y > SanctuaryBootstrapHalfExtent.Y
		|| AnchorTranslation.Z - HalfExtents.Z < -SanctuaryBootstrapHalfExtent.Z
		|| AnchorTranslation.Z + HalfExtents.Z > SanctuaryBootstrapHalfExtent.Z)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification POI bounds escape the source-owned sanctuary bootstrap extent."));
	}

	TSharedPtr<FJsonObject> GenerationInput;
	FString InputVersion;
	double GenerationSeed = 0.0;
	static const TSet<FString> GenerationInputFields = { TEXT("seed"), TEXT("inputVersion") };
	if (!TryGetRequiredObject(Root, TEXT("generationInput"), GenerationInput, OutError)
		|| !ValidateExactObjectFields(GenerationInput, TEXT("generationInput"), GenerationInputFields, OutError)
		|| !GenerationInput->TryGetNumberField(TEXT("seed"), GenerationSeed)
		|| !TryGetRequiredNonEmptyString(GenerationInput, TEXT("inputVersion"), InputVersion, OutError)
		|| GenerationSeed != static_cast<double>(Cycle2GenerationSeed)
		|| InputVersion != Cycle2GenerationInputVersion)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification generation input is missing or invalid."));
	}

	const TArray<TSharedPtr<FJsonValue>>* Subjects = nullptr;
	if (!TryGetRequiredArray(Root, TEXT("subjects"), Subjects, OutError))
	{
		return false;
	}
	if (Subjects->Num() != 1)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification requires exactly one Cycle II garden subject."));
	}
	static const TSet<FString> SubjectFields = {
		TEXT("subjectId"), TEXT("warningId"), TEXT("ritualType"), TEXT("restorationTag"), TEXT("surveyId")
	};
	int32 ExactGardenSubjectCount = 0;
	TSet<FString> SeenSubjectIds;
	for (int32 Index = 0; Index < Subjects->Num(); ++Index)
	{
		TSharedPtr<FJsonObject> Subject;
		FString SubjectId;
		FString WarningId;
		FString RitualType;
		FString RestorationTag;
		FString SurveyId;
		if (!ReadObjectArrayEntry((*Subjects)[Index], TEXT("subjects"), Index, Subject, OutError)
			|| !ValidateExactObjectFields(Subject, TEXT("subjects[]"), SubjectFields, OutError)
			|| !TryGetRequiredNonEmptyString(Subject, TEXT("subjectId"), SubjectId, OutError)
			|| !TryGetRequiredNonEmptyString(Subject, TEXT("warningId"), WarningId, OutError)
			|| !TryGetRequiredNonEmptyString(Subject, TEXT("ritualType"), RitualType, OutError)
			|| !TryGetRequiredNonEmptyString(Subject, TEXT("restorationTag"), RestorationTag, OutError)
			|| !TryGetRequiredNonEmptyString(Subject, TEXT("surveyId"), SurveyId, OutError))
		{
			return false;
		}
		if (SurveyIds.Contains(SurveyId))
		{
			return FailSpecificationValidation(OutError, TEXT("World specification has a duplicate surveyId."));
		}
		if (SeenSubjectIds.Contains(SubjectId))
		{
			return FailSpecificationValidation(OutError, TEXT("World specification has an ambiguous subjectId."));
		}
		SurveyIds.Add(SurveyId);
		SeenSubjectIds.Add(SubjectId);
		ExactGardenSubjectCount += SubjectId == Cycle2GardenPlanId.ToString()
			&& WarningId == Cycle2GardenWarningId.ToString()
			&& RitualType == TEXT("GardenBed")
			&& RestorationTag == Cycle2GardenRestorationTag.ToString() ? 1 : 0;
	}
	if (ExactGardenSubjectCount != 1)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification lacks the exact Cycle II GardenRot subject."));
	}

	TSharedPtr<FJsonObject> Evidence;
	const TArray<TSharedPtr<FJsonValue>>* SupportBindings = nullptr;
	static const TSet<FString> EvidenceFields = { TEXT("supportBindings"), TEXT("dawnReport") };
	if (!TryGetRequiredObject(Root, TEXT("evidence"), Evidence, OutError)
		|| !ValidateExactObjectFields(Evidence, TEXT("evidence"), EvidenceFields, OutError)
		|| !TryGetRequiredArray(Evidence, TEXT("supportBindings"), SupportBindings, OutError))
	{
		return false;
	}
	const TMap<FString, FString> RequiredSupportSurfaces = {
		{ TEXT("GardenRot.WitheredVines"), TEXT("environmental") },
		{ TEXT("GardenRot.ColdSoil"), TEXT("object_reaction") },
		{ TEXT("GardenRot.BellMoths"), TEXT("audio") }
	};
	if (SupportBindings->Num() != RequiredSupportSurfaces.Num())
	{
		return FailSpecificationValidation(OutError, TEXT("World specification requires exactly the approved GardenRot support bindings."));
	}
	static const TSet<FString> SupportBindingFields = { TEXT("supportId"), TEXT("surface"), TEXT("surfaceId") };
	TSet<FString> BoundSupportIds;
	for (int32 Index = 0; Index < SupportBindings->Num(); ++Index)
	{
		TSharedPtr<FJsonObject> Binding;
		FString SupportId;
		FString Surface;
		FString SurfaceId;
		if (!ReadObjectArrayEntry((*SupportBindings)[Index], TEXT("evidence.supportBindings"), Index, Binding, OutError)
			|| !ValidateExactObjectFields(Binding, TEXT("evidence.supportBindings[]"), SupportBindingFields, OutError)
			|| !TryGetRequiredNonEmptyString(Binding, TEXT("supportId"), SupportId, OutError)
			|| !TryGetRequiredNonEmptyString(Binding, TEXT("surface"), Surface, OutError)
			|| !TryGetRequiredNonEmptyString(Binding, TEXT("surfaceId"), SurfaceId, OutError))
		{
			return false;
		}
		const FString* RequiredSurface = RequiredSupportSurfaces.Find(SupportId);
		if (!RequiredSurface || *RequiredSurface != Surface || BoundSupportIds.Contains(SupportId))
		{
			return FailSpecificationValidation(OutError, TEXT("World specification has an invalid GardenRot support binding."));
		}
		BoundSupportIds.Add(SupportId);
	}
	if (BoundSupportIds.Num() != RequiredSupportSurfaces.Num())
	{
		return FailSpecificationValidation(OutError, TEXT("World specification lacks a required GardenRot support binding."));
	}

	TSharedPtr<FJsonObject> DawnReport;
	const TArray<TSharedPtr<FJsonValue>>* DawnSupportIds = nullptr;
	FString DawnSurface;
	FString DawnSurfaceId;
	static const TSet<FString> DawnReportFields = { TEXT("surface"), TEXT("surfaceId"), TEXT("supportIds") };
	if (!TryGetRequiredObject(Evidence, TEXT("dawnReport"), DawnReport, OutError)
		|| !ValidateExactObjectFields(DawnReport, TEXT("evidence.dawnReport"), DawnReportFields, OutError)
		|| !TryGetRequiredNonEmptyString(DawnReport, TEXT("surface"), DawnSurface, OutError)
		|| !TryGetRequiredNonEmptyString(DawnReport, TEXT("surfaceId"), DawnSurfaceId, OutError)
		|| !TryGetRequiredArray(DawnReport, TEXT("supportIds"), DawnSupportIds, OutError)
		|| DawnSurface != TEXT("dawn_report"))
	{
		return FailSpecificationValidation(OutError, TEXT("World specification requires the GardenRot dawn-report surface."));
	}
	if (DawnSupportIds->Num() != RequiredSupportSurfaces.Num())
	{
		return FailSpecificationValidation(OutError, TEXT("World specification dawn report must name exactly the approved GardenRot supports."));
	}
	TSet<FString> DawnSupportSet;
	for (const TSharedPtr<FJsonValue>& DawnSupportValue : *DawnSupportIds)
	{
		FString DawnSupportId;
		if (!DawnSupportValue.IsValid()
			|| !DawnSupportValue->TryGetString(DawnSupportId)
			|| !RequiredSupportSurfaces.Contains(DawnSupportId)
			|| DawnSupportSet.Contains(DawnSupportId))
		{
			return FailSpecificationValidation(OutError, TEXT("World specification has an invalid GardenRot dawn-report support."));
		}
		DawnSupportSet.Add(DawnSupportId);
	}
	for (const TPair<FString, FString>& RequiredSupport : RequiredSupportSurfaces)
	{
		if (!DawnSupportSet.Contains(RequiredSupport.Key))
		{
			return FailSpecificationValidation(OutError, TEXT("World specification dawn report omits a GardenRot support."));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ReactiveCategories = nullptr;
	if (!TryGetRequiredArray(Root, TEXT("reactiveCategories"), ReactiveCategories, OutError))
	{
		return false;
	}
	const TSet<FString> RequiredCategories = {
		TEXT("foliage"), TEXT("ruins"), TEXT("paths"), TEXT("lighting_materials")
	};
	if (ReactiveCategories->Num() != RequiredCategories.Num())
	{
		return FailSpecificationValidation(OutError, TEXT("World specification requires exactly the four Cycle II reactive categories."));
	}
	static const TSet<FString> ReactiveCategoryFields = { TEXT("category"), TEXT("stateKey") };
	TSet<FString> SeenCategories;
	for (int32 Index = 0; Index < ReactiveCategories->Num(); ++Index)
	{
		TSharedPtr<FJsonObject> Category;
		FString CategoryName;
		FString StateKey;
		if (!ReadObjectArrayEntry((*ReactiveCategories)[Index], TEXT("reactiveCategories"), Index, Category, OutError)
			|| !ValidateExactObjectFields(Category, TEXT("reactiveCategories[]"), ReactiveCategoryFields, OutError)
			|| !TryGetRequiredNonEmptyString(Category, TEXT("category"), CategoryName, OutError)
			|| !TryGetRequiredNonEmptyString(Category, TEXT("stateKey"), StateKey, OutError)
			|| StateKey != RestorationLevelKey.ToString()
			|| !RequiredCategories.Contains(CategoryName)
			|| SeenCategories.Contains(CategoryName))
		{
			return FailSpecificationValidation(OutError, TEXT("World specification has an invalid reactive category."));
		}
		SeenCategories.Add(CategoryName);
	}
	if (SeenCategories.Num() != RequiredCategories.Num())
	{
		return FailSpecificationValidation(OutError, TEXT("World specification omits a required reactive category."));
	}

	TSharedPtr<FJsonObject> WorldState;
	const TArray<TSharedPtr<FJsonValue>>* StateScenarios = nullptr;
	FString Scope;
	FString ContextId;
	FString Key;
	static const TSet<FString> WorldStateFields = { TEXT("scope"), TEXT("contextId"), TEXT("key"), TEXT("scenarios") };
	if (!TryGetRequiredObject(Root, TEXT("worldState"), WorldState, OutError)
		|| !ValidateExactObjectFields(WorldState, TEXT("worldState"), WorldStateFields, OutError)
		|| !TryGetRequiredNonEmptyString(WorldState, TEXT("scope"), Scope, OutError)
		|| !TryGetRequiredNonEmptyString(WorldState, TEXT("contextId"), ContextId, OutError)
		|| !TryGetRequiredNonEmptyString(WorldState, TEXT("key"), Key, OutError)
		|| !TryGetRequiredArray(WorldState, TEXT("scenarios"), StateScenarios, OutError)
		|| Scope != TEXT("Region")
		|| ContextId != Cycle2GardenRegionId.ToString()
		|| Key != RestorationLevelKey.ToString())
	{
		return FailSpecificationValidation(OutError, TEXT("World specification world-state address is invalid."));
	}
	if (StateScenarios->Num() != 2)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification requires exactly untouched and restored state scenarios."));
	}
	bool bHasUntouchedScenario = false;
	bool bHasRestoredScenario = false;
	for (const TSharedPtr<FJsonValue>& ScenarioValue : *StateScenarios)
	{
		double Scenario = -1.0;
		if (!ScenarioValue.IsValid() || !ScenarioValue->TryGetNumber(Scenario))
		{
			return FailSpecificationValidation(OutError, TEXT("World specification state scenarios must be numeric."));
		}
		bHasUntouchedScenario |= FMath::IsNearlyEqual(static_cast<float>(Scenario), 0.0f);
		bHasRestoredScenario |= FMath::IsNearlyEqual(static_cast<float>(Scenario), 1.0f);
	}
	if (!bHasUntouchedScenario || !bHasRestoredScenario)
	{
		return FailSpecificationValidation(OutError, TEXT("World specification requires 0.0 and 1.0 state scenarios."));
	}

	return true;
}
