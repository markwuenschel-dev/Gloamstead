#include "Commandlets/GloamsteadImportDataAssetsCommandlet.h"

#include "Data/NightConsequenceTypes.h"
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

			ReadNumberField(WarningObj, TEXT("ClarityTier"), Fragment.ClarityTier);

			const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
			if (WarningObj->TryGetArrayField(TEXT("SatisfiableTags"), TagsArray) &&
				!ParseStringArray(*TagsArray, Fragment.SatisfiableTags))
			{
				++OutErrorCount;
				return false;
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
