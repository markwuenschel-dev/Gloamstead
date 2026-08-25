#include "Commandlets/GloamsteadImportDataAssetsCommandlet.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Data/GloamsteadRitualSiteCatalog.h"

#include "Data/NightConsequenceTypes.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/RitualDefinition.h"
#include "Data/VeilHeartWarningTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace GloamsteadDataImport
{
	static bool ReadNumberField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, float& OutValue)
	{
		double Temp = 0.0;
		if (Obj->TryGetNumberField(Field, Temp))
		{
			OutValue = static_cast<float>(Temp);
			return true;
		}
		return false;
	}

	static bool ReadNumberField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, int32& OutValue)
	{
		double Temp = 0.0;
		if (Obj->TryGetNumberField(Field, Temp))
		{
			OutValue = static_cast<int32>(Temp);
			return true;
		}
		return false;
	}

	static bool ParseEnumByName(const UEnum* Enum, const FString& Name, int64& OutValue)
	{
		if (!Enum || Name.IsEmpty())
		{
			return false;
		}

		OutValue = Enum->GetValueByNameString(Name);
		if (OutValue == INDEX_NONE)
		{
			OutValue = Enum->GetValueByNameString(FString::Printf(TEXT("%s::%s"), *Enum->GetName(), *Name));
		}
		return OutValue != INDEX_NONE;
	}

	static bool ParseStringArray(const TArray<TSharedPtr<FJsonValue>>& Values, TArray<FName>& OutNames)
	{
		OutNames.Reset();
		for (const TSharedPtr<FJsonValue>& Value : Values)
		{
			if (!Value.IsValid() || Value->Type != EJson::String)
			{
				return false;
			}
			OutNames.Add(FName(*Value->AsString()));
		}
		return true;
	}

	static bool ParseRitualTypeArray(const TArray<TSharedPtr<FJsonValue>>& Values, TArray<ERitualType>& OutTypes)
	{
		OutTypes.Reset();
		const UEnum* RitualEnum = StaticEnum<ERitualType>();
		for (const TSharedPtr<FJsonValue>& Value : Values)
		{
			if (!Value.IsValid() || Value->Type != EJson::String)
			{
				return false;
			}
			int64 EnumValue = INDEX_NONE;
			if (!ParseEnumByName(RitualEnum, Value->AsString(), EnumValue))
			{
				return false;
			}
			OutTypes.Add(static_cast<ERitualType>(EnumValue));
		}
		return true;
	}

	static bool ParseSupportChannels(const TArray<TSharedPtr<FJsonValue>>& Values, TArray<FVeilHeartWarningSupportChannel>& OutChannels)
	{
		OutChannels.Reset();
		for (const TSharedPtr<FJsonValue>& Value : Values)
		{
			const TSharedPtr<FJsonObject> ChannelObj = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!ChannelObj.IsValid())
			{
				return false;
			}

			FString SupportId;
			FString EvidenceText;
			FString ChannelType;
			if (!ChannelObj->TryGetStringField(TEXT("SupportId"), SupportId)
				|| !ChannelObj->TryGetStringField(TEXT("EvidenceText"), EvidenceText)
				|| !ChannelObj->TryGetStringField(TEXT("ChannelType"), ChannelType)
				|| SupportId.IsEmpty()
				|| EvidenceText.TrimStartAndEnd().IsEmpty()
				|| ChannelType.IsEmpty())
			{
				return false;
			}

			FVeilHeartWarningSupportChannel& Channel = OutChannels.AddDefaulted_GetRef();
			Channel.SupportId = FName(*SupportId);
			Channel.EvidenceText = FText::FromString(EvidenceText);
			Channel.ChannelType = FName(*ChannelType);
		}
		return true;
	}

	static bool ValidateCanonicalGardenRotContract(const FVeilHeartWarningFragment& Fragment, FString& OutError)
	{
		UExperienceCycleCatalog* Catalog = NewObject<UExperienceCycleCatalog>(GetTransientPackage());
		PopulateDefaultExperienceCyclePlans(*Catalog);
		for (const FExperienceCyclePlan& Plan : Catalog->AuthoredPlans)
		{
			if (Plan.WarningId == Fragment.WarningId && Plan.NightType == Fragment.AssociatedNightType)
			{
				return Fragment.MatchesExactPlanContract(Plan, &OutError);
			}
		}

		OutError = FString::Printf(TEXT("the canonical plan for %s/%d is unavailable"),
			*Fragment.WarningId.ToString(), static_cast<int32>(Fragment.AssociatedNightType));
		return false;
	}

	static bool ImportExperienceCycleCatalog(
		UExperienceCycleCatalog* Catalog,
		const TSharedPtr<FJsonObject>& Properties,
		int32& OutErrorCount)
	{
		if (!Catalog || !Properties.IsValid())
		{
			++OutErrorCount;
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* PlanValues = nullptr;
		if (!Properties->TryGetArrayField(TEXT("AuthoredPlans"), PlanValues) || !PlanValues)
		{
			UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: ExperienceCycleCatalog needs an AuthoredPlans array."));
			++OutErrorCount;
			return false;
		}

		const UEnum* NightEnum = StaticEnum<ENightConsequenceType>();
		const UEnum* RitualEnum = StaticEnum<ERitualType>();
		const UEnum* ResolutionEnum = StaticEnum<EExperiencePlanResolution>();

		TArray<FExperienceCyclePlan> Plans;
		for (const TSharedPtr<FJsonValue>& Value : *PlanValues)
		{
			const TSharedPtr<FJsonObject>* PlanObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(PlanObjPtr) || !PlanObjPtr)
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: an AuthoredPlans entry is not an object."));
				++OutErrorCount;
				return false;
			}
			const TSharedPtr<FJsonObject>& PlanObj = *PlanObjPtr;

			FExperienceCyclePlan Plan;

			if (!ReadNumberField(PlanObj, TEXT("Slot"), Plan.Slot) || Plan.Slot < 1)
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: an authored plan needs a Slot >= 1."));
				++OutErrorCount;
				return false;
			}

			FString PlanId;
			FString WarningId;
			if (!PlanObj->TryGetStringField(TEXT("PlanId"), PlanId) || PlanId.IsEmpty()
				|| !PlanObj->TryGetStringField(TEXT("WarningId"), WarningId) || WarningId.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: plan in slot %d needs a PlanId and a WarningId."), Plan.Slot);
				++OutErrorCount;
				return false;
			}
			Plan.PlanId = FName(*PlanId);
			Plan.WarningId = FName(*WarningId);

			FString NightTypeName;
			int64 EnumValue = INDEX_NONE;
			if (!PlanObj->TryGetStringField(TEXT("NightType"), NightTypeName)
				|| !ParseEnumByName(NightEnum, NightTypeName, EnumValue))
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: plan %s has an unknown NightType."), *PlanId);
				++OutErrorCount;
				return false;
			}
			Plan.NightType = static_cast<ENightConsequenceType>(EnumValue);

			FString SemanticSubject;
			if (PlanObj->TryGetStringField(TEXT("SemanticSubject"), SemanticSubject) && !SemanticSubject.IsEmpty())
			{
				Plan.SemanticSubject = FName(*SemanticSubject);
			}

			const TArray<TSharedPtr<FJsonValue>>* TagValues = nullptr;
			if (PlanObj->TryGetArrayField(TEXT("RequiredRestorationTags"), TagValues) && TagValues
				&& !ParseStringArray(*TagValues, Plan.RequiredRestorationTags))
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: plan %s has a malformed RequiredRestorationTags."), *PlanId);
				++OutErrorCount;
				return false;
			}

			FString RitualTypeName;
			if (PlanObj->TryGetStringField(TEXT("RequiredRitualType"), RitualTypeName) && !RitualTypeName.IsEmpty())
			{
				int64 RitualValue = INDEX_NONE;
				if (!ParseEnumByName(RitualEnum, RitualTypeName, RitualValue))
				{
					UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: plan %s has an unknown RequiredRitualType."), *PlanId);
					++OutErrorCount;
					return false;
				}
				Plan.RequiredRitualType = static_cast<ERitualType>(RitualValue);
			}

			const TArray<TSharedPtr<FJsonValue>>* SupportIdValues = nullptr;
			if (PlanObj->TryGetArrayField(TEXT("RequiredSupportIds"), SupportIdValues) && SupportIdValues
				&& !ParseStringArray(*SupportIdValues, Plan.RequiredSupportIds))
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: plan %s has a malformed RequiredSupportIds."), *PlanId);
				++OutErrorCount;
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>>* ChannelTypeValues = nullptr;
			if (PlanObj->TryGetArrayField(TEXT("RequiredSupportChannelTypes"), ChannelTypeValues) && ChannelTypeValues
				&& !ParseStringArray(*ChannelTypeValues, Plan.RequiredSupportChannelTypes))
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: plan %s has a malformed RequiredSupportChannelTypes."), *PlanId);
				++OutErrorCount;
				return false;
			}

			ReadNumberField(PlanObj, TEXT("MinimumDistinctSupportCount"), Plan.MinimumDistinctSupportCount);

			FString ReceiptId;
			if (PlanObj->TryGetStringField(TEXT("InterpretationReceiptId"), ReceiptId) && !ReceiptId.IsEmpty())
			{
				Plan.InterpretationReceiptId = FName(*ReceiptId);
			}
			FString VisualStateKey;
			if (PlanObj->TryGetStringField(TEXT("VisualStateKey"), VisualStateKey) && !VisualStateKey.IsEmpty())
			{
				Plan.VisualStateKey = FName(*VisualStateKey);
			}
			FString OutcomeSummaryKey;
			if (PlanObj->TryGetStringField(TEXT("OutcomeSummaryKey"), OutcomeSummaryKey) && !OutcomeSummaryKey.IsEmpty())
			{
				Plan.OutcomeSummaryKey = FName(*OutcomeSummaryKey);
			}

			// Resolution defaults to Authored: a plan written into the authored catalog IS an authored plan.
			// Anything else must be stated explicitly, so an accidental omission cannot silently produce a row
			// that IsAuthoredPlan() rejects and that therefore blocks its cycle forever.
			FString ResolutionName;
			if (PlanObj->TryGetStringField(TEXT("Resolution"), ResolutionName) && !ResolutionName.IsEmpty())
			{
				int64 ResolutionValue = INDEX_NONE;
				if (!ParseEnumByName(ResolutionEnum, ResolutionName, ResolutionValue))
				{
					UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: plan %s has an unknown Resolution."), *PlanId);
					++OutErrorCount;
					return false;
				}
				Plan.Resolution = static_cast<EExperiencePlanResolution>(ResolutionValue);
			}
			else
			{
				Plan.Resolution = EExperiencePlanResolution::Authored;
			}

			Plans.Add(MoveTemp(Plan));
		}

		// Slots must be unique and contiguous from 1: EnsureUpcomingPlan walks CompletedCycleSlot + 1, so a
		// gap silently ends the experience at the gap rather than at the last authored cycle.
		Plans.Sort([](const FExperienceCyclePlan& A, const FExperienceCyclePlan& B) { return A.Slot < B.Slot; });
		for (int32 Index = 0; Index < Plans.Num(); ++Index)
		{
			if (Plans[Index].Slot != Index + 1)
			{
				UE_LOG(LogTemp, Error,
					TEXT("GloamsteadImportDataAssets: authored plan slots must be unique and contiguous from 1; expected %d but found %d."),
					Index + 1, Plans[Index].Slot);
				++OutErrorCount;
				return false;
			}
		}

		Catalog->AuthoredPlans = MoveTemp(Plans);
		return true;
	}

	static bool ImportRitualSiteCatalog(
		UGloamsteadRitualSiteCatalog* Catalog,
		const TSharedPtr<FJsonObject>& Properties,
		int32& OutErrorCount)
	{
		if (!Catalog || !Properties.IsValid())
		{
			++OutErrorCount;
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* SiteValues = nullptr;
		if (!Properties->TryGetArrayField(TEXT("Sites"), SiteValues) || !SiteValues)
		{
			UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: RitualSiteCatalog needs a Sites array."));
			++OutErrorCount;
			return false;
		}

		const UEnum* RitualEnum = StaticEnum<ERitualType>();
		const UEnum* AnchorEnum = StaticEnum<EGloamsteadSiteAnchor>();

		TArray<FGloamsteadRitualSiteDeclaration> Sites;
		for (const TSharedPtr<FJsonValue>& Value : *SiteValues)
		{
			const TSharedPtr<FJsonObject>* SiteObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(SiteObjPtr) || !SiteObjPtr)
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: a Sites entry is not an object."));
				++OutErrorCount;
				return false;
			}
			const TSharedPtr<FJsonObject>& SiteObj = *SiteObjPtr;

			FGloamsteadRitualSiteDeclaration Site;

			FString SemanticSubject;
			FString WarningId;
			FString RestorationTag;
			if (!SiteObj->TryGetStringField(TEXT("SemanticSubject"), SemanticSubject) || SemanticSubject.IsEmpty()
				|| !SiteObj->TryGetStringField(TEXT("RecommendedForWarning"), WarningId) || WarningId.IsEmpty()
				|| !SiteObj->TryGetStringField(TEXT("RestorationTag"), RestorationTag) || RestorationTag.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: a ritual site needs SemanticSubject, RecommendedForWarning and RestorationTag."));
				++OutErrorCount;
				return false;
			}
			Site.SemanticSubject = FName(*SemanticSubject);
			Site.RecommendedForWarning = FName(*WarningId);
			Site.RestorationTag = FName(*RestorationTag);

			FString RitualTypeName;
			int64 RitualValue = INDEX_NONE;
			if (!SiteObj->TryGetStringField(TEXT("RitualType"), RitualTypeName)
				|| !ParseEnumByName(RitualEnum, RitualTypeName, RitualValue))
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: ritual site %s has an unknown RitualType."), *SemanticSubject);
				++OutErrorCount;
				return false;
			}
			Site.RitualType = static_cast<ERitualType>(RitualValue);

			FString AnchorName;
			if (SiteObj->TryGetStringField(TEXT("Anchor"), AnchorName) && !AnchorName.IsEmpty())
			{
				int64 AnchorValue = INDEX_NONE;
				if (!ParseEnumByName(AnchorEnum, AnchorName, AnchorValue))
				{
					UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: ritual site %s has an unknown Anchor '%s'."), *SemanticSubject, *AnchorName);
					++OutErrorCount;
					return false;
				}
				Site.Anchor = static_cast<EGloamsteadSiteAnchor>(AnchorValue);
			}

			float Radius = 0.f;
			if (ReadNumberField(SiteObj, TEXT("BindRadius"), Radius) && Radius > 0.f)
			{
				Site.BindRadius = Radius;
			}
			float MinDistance = 0.f;
			if (ReadNumberField(SiteObj, TEXT("MinimumAnchorDistance"), MinDistance) && MinDistance >= 0.f)
			{
				Site.MinimumAnchorDistance = MinDistance;
			}

			TArray<FString> Problems;
			if (!Site.IsCompleteDeclaration(Problems))
			{
				for (const FString& Problem : Problems)
				{
					UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: %s"), *Problem);
				}
				++OutErrorCount;
				return false;
			}

			Sites.Add(MoveTemp(Site));
		}

		Catalog->Sites = MoveTemp(Sites);
		return true;
	}

	static bool SaveDataAsset(UObject* Asset, const FString& PackageName, int32& OutErrorCount)
	{
		if (!Asset)
		{
			++OutErrorCount;
			return false;
		}

		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();

		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FAssetRegistryModule::AssetCreated(Asset);

		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		const FString Directory = FPaths::GetPath(Filename);
		IFileManager::Get().MakeDirectory(*Directory, true);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
		if (!bSaved)
		{
			UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: failed to save %s"), *PackageName);
			++OutErrorCount;
			return false;
		}

		UE_LOG(LogTemp, Display, TEXT("GloamsteadImportDataAssets: saved %s"), *PackageName);
		return true;
	}

	static UObject* LoadOrCreateAsset(const FString& PackageName, const FString& AssetName, UClass* AssetClass)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
		if (UObject* Existing = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			return Existing;
		}

		UPackage* Package = CreatePackage(*PackageName);
		return NewObject<UObject>(Package, AssetClass, *AssetName, RF_Public | RF_Standalone);
	}

	/**
	 * Give authored meshes working collision.
	 *
	 * The sanctuary kit ships 27 meshes with a BodySetup but ZERO collision primitives and no trace flag,
	 * so simple-collision tracing finds nothing and the player walks through every wall and column. The
	 * mesh forge contract has no collision concept at all, so regenerating reintroduces it - this is the
	 * repair, not the cure.
	 *
	 * Sets CollisionTraceFlag so the render geometry is used as collision. Complex-as-simple is the honest
	 * choice for greybox ruins: authored convex hulls are better for a shipping build, but a wall you
	 * cannot walk through today beats a perfect hull nobody has authored.
	 */
	static bool ImportStaticMeshCollisionPolicy(const TSharedPtr<FJsonObject>& Properties, int32& OutErrorCount)
	{
		if (!Properties.IsValid())
		{
			++OutErrorCount;
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* FolderValues = nullptr;
		if (!Properties->TryGetArrayField(TEXT("Folders"), FolderValues) || !FolderValues)
		{
			UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: StaticMeshCollisionPolicy needs a Folders array."));
			++OutErrorCount;
			return false;
		}

		FString FlagName;
		Properties->TryGetStringField(TEXT("CollisionTraceFlag"), FlagName);
		ECollisionTraceFlag Flag = CTF_UseComplexAsSimple;
		if (!FlagName.IsEmpty())
		{
			if (FlagName == TEXT("UseComplexAsSimple")) { Flag = CTF_UseComplexAsSimple; }
			else if (FlagName == TEXT("UseSimpleAsComplex")) { Flag = CTF_UseSimpleAsComplex; }
			else if (FlagName == TEXT("UseDefault")) { Flag = CTF_UseDefault; }
			else
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: unknown CollisionTraceFlag '%s'."), *FlagName);
				++OutErrorCount;
				return false;
			}
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetRegistry.SearchAllAssets(/*bSynchronousSearch*/ true);

		int32 Changed = 0;
		int32 Inspected = 0;
		for (const TSharedPtr<FJsonValue>& Value : *FolderValues)
		{
			if (!Value.IsValid() || Value->Type != EJson::String)
			{
				UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: a Folders entry is not a string."));
				++OutErrorCount;
				return false;
			}

			const FString Folder = Value->AsString();
			FARFilter Filter;
			Filter.bRecursivePaths = true;
			Filter.PackagePaths.Add(FName(*Folder));
			Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());

			TArray<FAssetData> Assets;
			AssetRegistry.GetAssets(Filter, Assets);
			if (Assets.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("GloamsteadImportDataAssets: no static meshes found under %s."), *Folder);
			}

			for (const FAssetData& AssetData : Assets)
			{
				UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset());
				if (!Mesh || !Mesh->GetBodySetup())
				{
					continue;
				}
				++Inspected;

				if (Mesh->GetBodySetup()->CollisionTraceFlag == Flag)
				{
					continue;
				}

				Mesh->GetBodySetup()->Modify();
				Mesh->GetBodySetup()->CollisionTraceFlag = Flag;
				Mesh->GetBodySetup()->InvalidatePhysicsData();
				Mesh->GetBodySetup()->CreatePhysicsMeshes();
				Mesh->MarkPackageDirty();

				if (SaveDataAsset(Mesh, Mesh->GetPackage()->GetName(), OutErrorCount))
				{
					++Changed;
				}
			}
		}

		UE_LOG(LogTemp, Display,
			TEXT("GloamsteadImportDataAssets: collision policy applied to %d of %d static mesh(es)."),
			Changed, Inspected);
		return true;
	}

	static bool ImportNightCatalog(UNightConsequenceCatalog* Catalog, const TSharedPtr<FJsonObject>& Properties, int32& OutErrorCount)
	{
		if (!Catalog || !Properties.IsValid())
		{
			++OutErrorCount;
			return false;
		}

		const UEnum* NightEnum = StaticEnum<ENightConsequenceType>();

		FString FallbackName;
		if (Properties->TryGetStringField(TEXT("FallbackNightType"), FallbackName))
		{
			int64 EnumValue = INDEX_NONE;
			if (!ParseEnumByName(NightEnum, FallbackName, EnumValue))
			{
				++OutErrorCount;
				return false;
			}
			Catalog->FallbackNightType = static_cast<ENightConsequenceType>(EnumValue);
		}

		Properties->TryGetBoolField(TEXT("bForceTutorialOnFirstNight"), Catalog->bForceTutorialOnFirstNight);

		const TArray<TSharedPtr<FJsonValue>>* RulesArray = nullptr;
		if (!Properties->TryGetArrayField(TEXT("Rules"), RulesArray))
		{
			++OutErrorCount;
			return false;
		}

		Catalog->Rules.Reset();
		for (const TSharedPtr<FJsonValue>& RuleValue : *RulesArray)
		{
			const TSharedPtr<FJsonObject> RuleObj = RuleValue->AsObject();
			if (!RuleObj.IsValid())
			{
				++OutErrorCount;
				return false;
			}

			FNightConsequenceRule Rule;
			FString NightTypeName;
			if (!RuleObj->TryGetStringField(TEXT("NightType"), NightTypeName))
			{
				++OutErrorCount;
				return false;
			}
			int64 NightEnumValue = INDEX_NONE;
			if (!ParseEnumByName(NightEnum, NightTypeName, NightEnumValue))
			{
				++OutErrorCount;
				return false;
			}
			Rule.NightType = static_cast<ENightConsequenceType>(NightEnumValue);

			ReadNumberField(RuleObj, TEXT("Weight"), Rule.Weight);
			ReadNumberField(RuleObj, TEXT("MinAverageLight"), Rule.MinAverageLight);
			ReadNumberField(RuleObj, TEXT("MaxAverageLight"), Rule.MaxAverageLight);
			ReadNumberField(RuleObj, TEXT("MinAverageCorruption"), Rule.MinAverageCorruption);
			ReadNumberField(RuleObj, TEXT("MaxAverageCorruption"), Rule.MaxAverageCorruption);

			FString OmenTag;
			if (RuleObj->TryGetStringField(TEXT("OmenClueTag"), OmenTag) && !OmenTag.IsEmpty())
			{
				Rule.OmenClueTag = FName(*OmenTag);
			}

			const TArray<TSharedPtr<FJsonValue>>* FavoredArray = nullptr;
			if (RuleObj->TryGetArrayField(TEXT("FavoredRitualTypes"), FavoredArray) &&
				!ParseRitualTypeArray(*FavoredArray, Rule.FavoredRitualTypes))
			{
				++OutErrorCount;
				return false;
			}

			Catalog->Rules.Add(Rule);
		}

		return true;
	}

	static bool ImportWarningCatalog(UVeilHeartWarningCatalog* Catalog, const TSharedPtr<FJsonObject>& Properties, int32& OutErrorCount)
	{
		if (!Catalog || !Properties.IsValid())
		{
			++OutErrorCount;
			return false;
		}

		const UEnum* NightEnum = StaticEnum<ENightConsequenceType>();
		const TArray<TSharedPtr<FJsonValue>>* WarningsArray = nullptr;
		if (!Properties->TryGetArrayField(TEXT("Warnings"), WarningsArray))
		{
			++OutErrorCount;
			return false;
		}

		Catalog->Warnings.Reset();
		TSet<FString> ImportedWarningKeys;
		for (const TSharedPtr<FJsonValue>& WarningValue : *WarningsArray)
		{
			const TSharedPtr<FJsonObject> WarningObj = WarningValue->AsObject();
			if (!WarningObj.IsValid())
			{
				++OutErrorCount;
				return false;
			}

			FVeilHeartWarningFragment Fragment;
			FString WarningId;
			if (!WarningObj->TryGetStringField(TEXT("WarningId"), WarningId))
			{
				++OutErrorCount;
				return false;
			}
			Fragment.WarningId = FName(*WarningId);
			if (Fragment.WarningId == NAME_None)
			{
				++OutErrorCount;
				return false;
			}

			FString FragmentText;
			if (!WarningObj->TryGetStringField(TEXT("Fragment"), FragmentText))
			{
				++OutErrorCount;
				return false;
			}
			Fragment.Fragment = FText::FromString(FragmentText);

			FString NightTypeName;
			if (WarningObj->TryGetStringField(TEXT("AssociatedNightType"), NightTypeName))
			{
				int64 EnumValue = INDEX_NONE;
				if (!ParseEnumByName(NightEnum, NightTypeName, EnumValue))
				{
					++OutErrorCount;
					return false;
				}
				Fragment.AssociatedNightType = static_cast<ENightConsequenceType>(EnumValue);
			}

			const FString WarningKey = FString::Printf(TEXT("%s|%d"),
				*Fragment.WarningId.ToString(), static_cast<int32>(Fragment.AssociatedNightType));
			if (ImportedWarningKeys.Contains(WarningKey))
			{
				++OutErrorCount;
				return false;
			}
			ImportedWarningKeys.Add(WarningKey);

			ReadNumberField(WarningObj, TEXT("ClarityTier"), Fragment.ClarityTier);

			const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
			if (WarningObj->TryGetArrayField(TEXT("SatisfiableTags"), TagsArray) &&
				!ParseStringArray(*TagsArray, Fragment.SatisfiableTags))
			{
				++OutErrorCount;
				return false;
			}

			FString SemanticSubject;
			if (WarningObj->TryGetStringField(TEXT("SemanticSubject"), SemanticSubject) && !SemanticSubject.IsEmpty())
			{
				Fragment.SemanticSubject = FName(*SemanticSubject);
			}

			FString RequiredRitualTypeName;
			if (WarningObj->TryGetStringField(TEXT("RequiredRitualType"), RequiredRitualTypeName))
			{
				int64 EnumValue = INDEX_NONE;
				if (!ParseEnumByName(StaticEnum<ERitualType>(), RequiredRitualTypeName, EnumValue))
				{
					++OutErrorCount;
					return false;
				}
				Fragment.RequiredRitualType = static_cast<ERitualType>(EnumValue);
			}

			const TArray<TSharedPtr<FJsonValue>>* SupportsArray = nullptr;
			if (WarningObj->TryGetArrayField(TEXT("SupportChannels"), SupportsArray)
				&& !ParseSupportChannels(*SupportsArray, Fragment.SupportChannels))
			{
				++OutErrorCount;
				return false;
			}

			FString ReceiptId;
			if (WarningObj->TryGetStringField(TEXT("InterpretationReceiptId"), ReceiptId) && !ReceiptId.IsEmpty())
			{
				Fragment.InterpretationReceiptId = FName(*ReceiptId);
			}

			if (Fragment.WarningId == FName(TEXT("GardenRot")))
			{
				FString ContractError;
				if (!ValidateCanonicalGardenRotContract(Fragment, ContractError))
				{
					UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: GardenRot contract rejected: %s"), *ContractError);
					++OutErrorCount;
					return false;
				}
			}

			Catalog->Warnings.Add(Fragment);
		}

		return true;
	}

	static bool ImportRitualDefinition(URitualDefinition* Definition, const TSharedPtr<FJsonObject>& Properties, int32& OutErrorCount)
	{
		if (!Definition || !Properties.IsValid())
		{
			++OutErrorCount;
			return false;
		}

		const UEnum* RitualEnum = StaticEnum<ERitualType>();
		FString RitualTypeName;
		if (!Properties->TryGetStringField(TEXT("RitualType"), RitualTypeName))
		{
			++OutErrorCount;
			return false;
		}
		int64 EnumValue = INDEX_NONE;
		if (!ParseEnumByName(RitualEnum, RitualTypeName, EnumValue))
		{
			++OutErrorCount;
			return false;
		}
		Definition->RitualType = static_cast<ERitualType>(EnumValue);

		ReadNumberField(Properties, TEXT("DefaultLightContribution"), Definition->DefaultLightContribution);
		ReadNumberField(Properties, TEXT("DefaultCorruptionClearance"), Definition->DefaultCorruptionClearance);
		ReadNumberField(Properties, TEXT("RestorationRadius"), Definition->RestorationRadius);

		const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
		if (Properties->TryGetArrayField(TEXT("SatisfiableWarningTags"), TagsArray) &&
			!ParseStringArray(*TagsArray, Definition->SatisfiableWarningTags))
		{
			++OutErrorCount;
			return false;
		}

		return true;
	}
}

UGloamsteadImportDataAssetsCommandlet::UGloamsteadImportDataAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UGloamsteadImportDataAssetsCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Display, TEXT("GloamsteadImportDataAssets: Params=%s"), *Params);

	FString ManifestPath;
	if (!FParse::Value(*Params, TEXT("Manifest="), ManifestPath))
	{
		UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: missing -Manifest= path (use repo-relative e.g. specs/data/vs-polish-starter.json)"));
		return 1;
	}
	ManifestPath = ManifestPath.TrimQuotes();

	if (!FPaths::FileExists(ManifestPath))
	{
		const FString ProjectRelative = FPaths::Combine(FPaths::ProjectDir(), ManifestPath);
		if (FPaths::FileExists(ProjectRelative))
		{
			ManifestPath = ProjectRelative;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: manifest not found: %s (also tried %s)"), *ManifestPath, *ProjectRelative);
			return 1;
		}
	}

	UE_LOG(LogTemp, Display, TEXT("GloamsteadImportDataAssets: loading manifest %s"), *ManifestPath);

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
	{
		UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: failed to read manifest: %s"), *ManifestPath);
		return 1;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: invalid JSON in %s"), *ManifestPath);
		return 1;
	}

	FString ContentPath = TEXT("/Game/Data");
	Root->TryGetStringField(TEXT("content_path"), ContentPath);

	const TArray<TSharedPtr<FJsonValue>>* AssetsArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("assets"), AssetsArray))
	{
		UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: manifest missing assets[]"));
		return 1;
	}

	int32 ErrorCount = 0;
	for (const TSharedPtr<FJsonValue>& AssetValue : *AssetsArray)
	{
		const TSharedPtr<FJsonObject> AssetObj = AssetValue->AsObject();
		if (!AssetObj.IsValid())
		{
			++ErrorCount;
			continue;
		}

		FString AssetName;
		FString ClassName;
		if (!AssetObj->TryGetStringField(TEXT("asset_name"), AssetName) ||
			!AssetObj->TryGetStringField(TEXT("class"), ClassName))
		{
			++ErrorCount;
			continue;
		}

		const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
		if (!AssetObj->TryGetObjectField(TEXT("properties"), PropertiesPtr) || !PropertiesPtr->IsValid())
		{
			++ErrorCount;
			continue;
		}

		const FString PackageName = FString::Printf(TEXT("%s/%s"), *ContentPath, *AssetName);

		if (ClassName == TEXT("NightConsequenceCatalog"))
		{
			UNightConsequenceCatalog* Catalog = Cast<UNightConsequenceCatalog>(
				GloamsteadDataImport::LoadOrCreateAsset(PackageName, AssetName, UNightConsequenceCatalog::StaticClass()));
			if (!GloamsteadDataImport::ImportNightCatalog(Catalog, *PropertiesPtr, ErrorCount))
			{
				continue;
			}
			GloamsteadDataImport::SaveDataAsset(Catalog, PackageName, ErrorCount);
		}
		else if (ClassName == TEXT("VeilHeartWarningCatalog"))
		{
			UVeilHeartWarningCatalog* Catalog = Cast<UVeilHeartWarningCatalog>(
				GloamsteadDataImport::LoadOrCreateAsset(PackageName, AssetName, UVeilHeartWarningCatalog::StaticClass()));
			if (!GloamsteadDataImport::ImportWarningCatalog(Catalog, *PropertiesPtr, ErrorCount))
			{
				continue;
			}
			GloamsteadDataImport::SaveDataAsset(Catalog, PackageName, ErrorCount);
		}
		else if (ClassName == TEXT("ExperienceCycleCatalog"))
		{
			UExperienceCycleCatalog* Catalog = Cast<UExperienceCycleCatalog>(
				GloamsteadDataImport::LoadOrCreateAsset(PackageName, AssetName, UExperienceCycleCatalog::StaticClass()));
			if (!GloamsteadDataImport::ImportExperienceCycleCatalog(Catalog, *PropertiesPtr, ErrorCount))
			{
				continue;
			}
			GloamsteadDataImport::SaveDataAsset(Catalog, PackageName, ErrorCount);
		}
		else if (ClassName == TEXT("StaticMeshCollisionPolicy"))
		{
			GloamsteadDataImport::ImportStaticMeshCollisionPolicy(*PropertiesPtr, ErrorCount);
		}
		else if (ClassName == TEXT("RitualSiteCatalog"))
		{
			UGloamsteadRitualSiteCatalog* Catalog = Cast<UGloamsteadRitualSiteCatalog>(
				GloamsteadDataImport::LoadOrCreateAsset(PackageName, AssetName, UGloamsteadRitualSiteCatalog::StaticClass()));
			if (!GloamsteadDataImport::ImportRitualSiteCatalog(Catalog, *PropertiesPtr, ErrorCount))
			{
				continue;
			}
			GloamsteadDataImport::SaveDataAsset(Catalog, PackageName, ErrorCount);
		}
		else if (ClassName == TEXT("RitualDefinition"))
		{
			URitualDefinition* Definition = Cast<URitualDefinition>(
				GloamsteadDataImport::LoadOrCreateAsset(PackageName, AssetName, URitualDefinition::StaticClass()));
			if (!GloamsteadDataImport::ImportRitualDefinition(Definition, *PropertiesPtr, ErrorCount))
			{
				continue;
			}
			GloamsteadDataImport::SaveDataAsset(Definition, PackageName, ErrorCount);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: unknown class %s for %s"), *ClassName, *AssetName);
			++ErrorCount;
		}
	}

	if (ErrorCount > 0)
	{
		UE_LOG(LogTemp, Error, TEXT("GloamsteadImportDataAssets: completed with %d error(s)"), ErrorCount);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("GloamsteadImportDataAssets: success (%d assets)"), AssetsArray->Num());
	return 0;
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadImportGardenRotSparseSupportRejectedTest,
	"Gloamstead.Editor.Import.GardenRotRejectsSparseSupportFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadImportGardenRotSparseSupportRejectedTest::RunTest(const FString& /*Parameters*/)
{
	const FString SparseGardenJson = TEXT(R"JSON(
{
  "Warnings": [
    {
      "WarningId": "GardenRot",
      "Fragment": "What grows in darkness must be tended before the bell tolls.",
      "AssociatedNightType": "Corruption",
      "SatisfiableTags": ["GardenBed"],
      "SemanticSubject": "Cycle2_Garden",
      "RequiredRitualType": "GardenBed",
      "SupportChannels": [
        {
          "SupportId": "GardenRot.WitheredVines",
          "EvidenceText": "Grey leaves curl toward the eastern bed.",
          "ChannelType": "Environmental"
        }
      ],
      "InterpretationReceiptId": "GardenRot.Interpreted",
      "ClarityTier": 1
    }
  ]
}
)JSON");

	TSharedPtr<FJsonObject> Properties;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SparseGardenJson);
	if (!TestTrue(TEXT("negative import fixture parses as JSON"), FJsonSerializer::Deserialize(Reader, Properties) && Properties.IsValid()))
	{
		return false;
	}

	UVeilHeartWarningCatalog* Catalog = NewObject<UVeilHeartWarningCatalog>();
	int32 ErrorCount = 0;
	AddExpectedErrorPlain(TEXT("GloamsteadImportDataAssets: GardenRot contract rejected"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("import rejects a sparse GardenRot support array"),
		GloamsteadDataImport::ImportWarningCatalog(Catalog, Properties, ErrorCount));
	TestTrue(TEXT("import reports the rejected contract"), ErrorCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadImportGardenRotWrongMediumRejectedTest,
	"Gloamstead.Editor.Import.GardenRotRejectsWrongMediumFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadImportGardenRotWrongMediumRejectedTest::RunTest(const FString& /*Parameters*/)
{
	const FString WrongMediumGardenJson = TEXT(R"JSON(
{
  "Warnings": [
    {
      "WarningId": "GardenRot",
      "Fragment": "What grows in darkness must be tended before the bell tolls.",
      "AssociatedNightType": "Corruption",
      "SatisfiableTags": ["GardenBed"],
      "SemanticSubject": "Cycle2_Garden",
      "RequiredRitualType": "GardenBed",
      "SupportChannels": [
        { "SupportId": "GardenRot.WitheredVines", "EvidenceText": "Grey leaves curl toward the eastern bed.", "ChannelType": "Environmental" },
        { "SupportId": "GardenRot.ColdSoil", "EvidenceText": "A root-chime answers beside the cracked bed.", "ChannelType": "ObjectReaction" },
        { "SupportId": "GardenRot.BellMoths", "EvidenceText": "Moths gather where soil whispers beneath the bell.", "ChannelType": "Environmental" }
      ],
      "InterpretationReceiptId": "GardenRot.Interpreted",
      "ClarityTier": 1
    }
  ]
}
)JSON");

	TSharedPtr<FJsonObject> Properties;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(WrongMediumGardenJson);
	if (!TestTrue(TEXT("wrong-medium import fixture parses as JSON"), FJsonSerializer::Deserialize(Reader, Properties) && Properties.IsValid()))
	{
		return false;
	}

	UVeilHeartWarningCatalog* Catalog = NewObject<UVeilHeartWarningCatalog>();
	int32 ErrorCount = 0;
	AddExpectedErrorPlain(TEXT("GloamsteadImportDataAssets: GardenRot contract rejected"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("import rejects a wrong-medium GardenRot support"),
		GloamsteadDataImport::ImportWarningCatalog(Catalog, Properties, ErrorCount));
	TestTrue(TEXT("import reports the wrong-medium contract rejection"), ErrorCount > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
