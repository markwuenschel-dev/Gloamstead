#include "Data/GloamsteadGeneratedAssetCatalog.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
	bool IsSha256(const FString& Value)
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Ch : Value)
		{
			if (!FChar::IsHexDigit(Ch))
			{
				return false;
			}
		}
		return true;
	}

	bool IsStableId(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Ch : Value)
		{
			if (!(FChar::IsAlnum(Ch) || Ch == TEXT('.') || Ch == TEXT('-') || Ch == TEXT('_')))
			{
				return false;
			}
		}
		return true;
	}

	bool IsVersionRoot(const FString& Root)
	{
		static const FString Prefix = TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/");
		const FString Version = Root.Mid(Prefix.Len());
		return Root.StartsWith(Prefix)
			&& Root.Len() > Prefix.Len()
			&& !Root.EndsWith(TEXT("/"))
			&& !Version.Contains(TEXT("/"))
			&& IsStableId(Version);
	}

	void ValidateObjectPath(const FSoftObjectPath& ObjectPath, const FString& VersionRoot, TArray<FString>& Codes)
	{
		static const FString GeneratedRoot = TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/");
		const FString PackageName = ObjectPath.GetLongPackageName();
		if (PackageName.IsEmpty() || !PackageName.StartsWith(GeneratedRoot))
		{
			Codes.AddUnique(TEXT("GAC007"));
			return;
		}
		if (!PackageName.StartsWith(VersionRoot + TEXT("/")))
		{
			Codes.AddUnique(TEXT("GAC008"));
		}
	}

	bool IsMeshForgeClass(const TSoftClassPtr<UObject>& ExpectedClass)
	{
		const UClass* Class = ExpectedClass.Get();
		// A non-null but unloaded soft class is not evidence of an unsupported class. The provider
		// resolves it and performs the authoritative IsA check before spawning.
		return !Class
			|| Class->IsChildOf(UStaticMesh::StaticClass())
			|| Class->IsChildOf(UMaterialInterface::StaticClass());
	}
}

FString GACStateToken(EGloamsteadGeneratedAssetState State)
{
	switch (State)
	{
	case EGloamsteadGeneratedAssetState::Before: return TEXT("before");
	case EGloamsteadGeneratedAssetState::RestorationInProgress: return TEXT("restoration_in_progress");
	case EGloamsteadGeneratedAssetState::Restored: return TEXT("restored");
	case EGloamsteadGeneratedAssetState::Corrupted: return TEXT("corrupted");
	case EGloamsteadGeneratedAssetState::Unknown:
	default: return TEXT("unknown");
	}
}

const FGloamsteadGeneratedAssetEntry* UGloamsteadGeneratedAssetCatalog::FindExact(
	FName SemanticRole, EGloamsteadGeneratedAssetState State) const
{
	if (SemanticRole.IsNone() || State == EGloamsteadGeneratedAssetState::Unknown)
	{
		return nullptr;
	}

	const FGloamsteadGeneratedAssetEntry* Match = nullptr;
	for (const FGloamsteadGeneratedAssetEntry& Entry : Entries)
	{
		if (Entry.SemanticRole == SemanticRole && Entry.RestorationState == State)
		{
			if (Match)
			{
				return nullptr;
			}
			Match = &Entry;
		}
	}
	return Match;
}

TArray<FString> GACValidateCatalog(
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	bool bRequireMeshForgeCompatibleClasses)
{
	TArray<FString> Codes;
	if (!IsStableId(Catalog.BundleId))
	{
		Codes.Add(TEXT("GAC001"));
	}
	if (!IsVersionRoot(Catalog.VersionRoot))
	{
		Codes.Add(TEXT("GAC002"));
	}
	if (!IsSha256(Catalog.ReceiptSha256))
	{
		Codes.Add(TEXT("GAC003"));
	}
	if (Catalog.Entries.Num() == 0)
	{
		Codes.Add(TEXT("GAC019"));
	}

	TSet<FString> SeenKeys;
	TSet<FString> CatalogAssets;
	for (const FGloamsteadGeneratedAssetEntry& Entry : Catalog.Entries)
	{
		const FString AssetPath = Entry.Asset.ToSoftObjectPath().ToString();
		if (CatalogAssets.Contains(AssetPath))
		{
			Codes.AddUnique(TEXT("GAC022"));
		}
		CatalogAssets.Add(AssetPath);
	}
	for (const FGloamsteadGeneratedAssetEntry& Entry : Catalog.Entries)
	{
		if (Entry.SemanticRole.IsNone() || !IsStableId(Entry.SemanticRole.ToString()))
		{
			Codes.AddUnique(TEXT("GAC004"));
		}
		if (Entry.RestorationState == EGloamsteadGeneratedAssetState::Unknown)
		{
			Codes.AddUnique(TEXT("GAC006"));
		}

		const FString Key = FString::Printf(TEXT("%s|%d"),
			*Entry.SemanticRole.ToString().ToLower(), static_cast<int32>(Entry.RestorationState));
		if (SeenKeys.Contains(Key))
		{
			Codes.AddUnique(TEXT("GAC005"));
		}
		SeenKeys.Add(Key);

		ValidateObjectPath(Entry.Asset.ToSoftObjectPath(), Catalog.VersionRoot, Codes);
		TSet<FString> EntryDependencies;
		for (const TSoftObjectPtr<UObject>& Dependency : Entry.Dependencies)
		{
			ValidateObjectPath(Dependency.ToSoftObjectPath(), Catalog.VersionRoot, Codes);
			const FString DependencyPath = Dependency.ToSoftObjectPath().ToString();
			if (EntryDependencies.Contains(DependencyPath))
			{
				Codes.AddUnique(TEXT("GAC020"));
			}
			EntryDependencies.Add(DependencyPath);
			if (!CatalogAssets.Contains(DependencyPath)
				|| DependencyPath == Entry.Asset.ToSoftObjectPath().ToString())
			{
				Codes.AddUnique(TEXT("GAC021"));
			}
		}

		if (!IsStableId(Entry.OwnershipId))
		{
			Codes.AddUnique(TEXT("GAC009"));
		}
		if (!IsStableId(Entry.LicenseId))
		{
			Codes.AddUnique(TEXT("GAC010"));
		}
		if (Entry.ExpectedClass.IsNull())
		{
			Codes.AddUnique(TEXT("GAC011"));
		}
		else if (bRequireMeshForgeCompatibleClasses && !IsMeshForgeClass(Entry.ExpectedClass))
		{
			Codes.AddUnique(TEXT("GAC013"));
		}
		if (!IsSha256(Entry.ObjectSha256))
		{
			Codes.AddUnique(TEXT("GAC012"));
		}
		if (!IsSha256(Entry.ReceiptSha256))
		{
			Codes.AddUnique(TEXT("GAC003"));
		}
		else if (!Entry.ReceiptSha256.Equals(Catalog.ReceiptSha256, ESearchCase::IgnoreCase))
		{
			Codes.AddUnique(TEXT("GAC015"));
		}
	}
	return Codes;
}

TArray<FString> GACValidateActiveBinding(
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	const FString& ExpectedBundleId,
	const FString& ExpectedReceiptSha256)
{
	TArray<FString> Codes;
	if (ExpectedBundleId.IsEmpty() || Catalog.BundleId != ExpectedBundleId)
	{
		Codes.Add(TEXT("GAC014"));
	}
	if (!IsSha256(ExpectedReceiptSha256) || !Catalog.ReceiptSha256.Equals(ExpectedReceiptSha256, ESearchCase::IgnoreCase))
	{
		Codes.Add(TEXT("GAC015"));
	}
	return Codes;
}

TArray<FString> GACValidateLoadedObject(
	const FGloamsteadGeneratedAssetEntry& Entry,
	const UObject* LoadedObject,
	bool bRequireStaticMeshForProvider)
{
	TArray<FString> Codes;
	if (!LoadedObject)
	{
		Codes.Add(TEXT("GAC017"));
		return Codes;
	}
	const UClass* ExpectedClass = Entry.ExpectedClass.Get();
	if (!ExpectedClass)
	{
		Codes.Add(TEXT("GAC017"));
		return Codes;
	}
	if (!LoadedObject->IsA(ExpectedClass))
	{
		Codes.Add(TEXT("GAC018"));
		return Codes;
	}
	if (bRequireStaticMeshForProvider && !LoadedObject->IsA(UStaticMesh::StaticClass()))
	{
		Codes.Add(TEXT("GAC013"));
	}
	return Codes;
}
