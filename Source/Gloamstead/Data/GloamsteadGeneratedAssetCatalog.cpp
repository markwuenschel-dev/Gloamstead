#include "Data/GloamsteadGeneratedAssetCatalog.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

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

	bool IsPackageUnderRoot(const FString& PackageName, const FString& Root)
	{
		return PackageName.Equals(Root, ESearchCase::IgnoreCase)
			|| PackageName.StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase);
	}

	bool IsValidPackageName(const FString& PackageName)
	{
		return FPackageName::IsValidLongPackageName(PackageName)
			&& !PackageName.EndsWith(TEXT("/"));
	}

	bool IsTerminalPlatformPackage(
		const FString& PackageName,
		const TArray<FString>& ExactPackages,
		const TArray<FString>& SafeRoots)
	{
		for (const FString& Exact : ExactPackages)
		{
			if (PackageName.Equals(Exact, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		for (const FString& Root : SafeRoots)
		{
			if (IsPackageUnderRoot(PackageName, Root))
			{
				return true;
			}
		}
		return false;
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

const FName GloamsteadGeneratedAssetProvenanceTags::ObjectSha256(TEXT("WorldForge.ObjectSha256"));
const FName GloamsteadGeneratedAssetProvenanceTags::ReceiptSha256(TEXT("WorldForge.ReceiptSha256"));
const FName GloamsteadGeneratedAssetProvenanceTags::BundleId(TEXT("WorldForge.BundleId"));
const FName GloamsteadGeneratedAssetProvenanceTags::PackageSha256(TEXT("WorldForge.PackageSha256"));

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
	const UEnum* StateEnum = StaticEnum<EGloamsteadGeneratedAssetState>();
	if (SemanticRole.IsNone() || !StateEnum
		|| !StateEnum->IsValidEnumValue(static_cast<int64>(State))
		|| State == EGloamsteadGeneratedAssetState::Unknown)
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
	if (!IsSha256(Catalog.TargetBuildIdentitySha256))
	{
		Codes.AddUnique(TEXT("GAC036"));
	}
	TSet<FString> SeenTerminalRoots;
	for (const FString& Root : Catalog.TerminalPlatformPackageRoots)
	{
		const FString Folded = Root.ToLower();
		if ((!Root.Equals(TEXT("/Engine"), ESearchCase::CaseSensitive)
				&& !Root.Equals(TEXT("/Script"), ESearchCase::CaseSensitive))
			|| SeenTerminalRoots.Contains(Folded))
		{
			Codes.AddUnique(TEXT("GAC034"));
		}
		SeenTerminalRoots.Add(Folded);
	}
	TSet<FString> SeenTerminalPackages;
	for (const FString& PackageName : Catalog.TerminalPlatformPackages)
	{
		const FString Folded = PackageName.ToLower();
		const bool bPlatformPackage = IsPackageUnderRoot(PackageName, TEXT("/Engine"))
			|| IsPackageUnderRoot(PackageName, TEXT("/Script"));
		if (!IsValidPackageName(PackageName) || !bPlatformPackage
			|| PackageName.Equals(TEXT("/Engine"), ESearchCase::IgnoreCase)
			|| PackageName.Equals(TEXT("/Script"), ESearchCase::IgnoreCase)
			|| SeenTerminalPackages.Contains(Folded)
			|| IsTerminalPlatformPackage(PackageName, {}, Catalog.TerminalPlatformPackageRoots))
		{
			Codes.AddUnique(TEXT("GAC034"));
		}
		SeenTerminalPackages.Add(Folded);
	}

	TSet<FString> SeenKeys;
	TSet<FString> CatalogAssets;
	TSet<FString> CatalogPackages;
	TMap<FString, FString> CatalogAssetByFoldedPackage;
	for (const FGloamsteadGeneratedAssetEntry& Entry : Catalog.Entries)
	{
		const FString AssetPath = Entry.Asset.ToSoftObjectPath().ToString();
		const FString FoldedAssetPath = AssetPath.ToLower();
		if (CatalogAssets.Contains(FoldedAssetPath))
		{
			Codes.AddUnique(TEXT("GAC022"));
		}
		CatalogAssets.Add(FoldedAssetPath);
		const FString PackageName = Entry.Asset.ToSoftObjectPath().GetLongPackageName();
		const FString FoldedPackageName = PackageName.ToLower();
		if (!PackageName.IsEmpty() && CatalogPackages.Contains(FoldedPackageName))
		{
			Codes.AddUnique(TEXT("GAC029"));
		}
		CatalogPackages.Add(FoldedPackageName);
		CatalogAssetByFoldedPackage.Add(FoldedPackageName, FoldedAssetPath);
	}
	TSet<FString> ExternalPackages;
	for (const FGloamsteadGeneratedExternalPackageRecord& Record : Catalog.ExternalPackageRecords)
	{
		const FString Folded = Record.PackageName.ToLower();
		const FString ProvenancePackage = Record.ProvenanceObject.ToSoftObjectPath().GetLongPackageName();
		if (!IsValidPackageName(Record.PackageName)
			|| IsPackageUnderRoot(Record.PackageName, TEXT("/Game/Gloamstead/Generated"))
			|| IsPackageUnderRoot(Record.PackageName, TEXT("/Engine"))
			|| IsPackageUnderRoot(Record.PackageName, TEXT("/Script"))
			|| ExternalPackages.Contains(Folded)
			|| ProvenancePackage.IsEmpty()
			|| !ProvenancePackage.Equals(Record.PackageName, ESearchCase::IgnoreCase))
		{
			Codes.AddUnique(TEXT("GAC034"));
		}
		ExternalPackages.Add(Folded);
		if (!IsSha256(Record.PackageSha256) || !IsSha256(Record.ReceiptSha256)
			|| !Record.ReceiptSha256.Equals(Catalog.ReceiptSha256, ESearchCase::IgnoreCase)
			|| Record.BundleId != Catalog.BundleId)
		{
			Codes.AddUnique(TEXT("GAC035"));
		}
		TSet<FString> DirectDependencies;
		for (const FString& DependencyPackage : Record.DirectPackageDependencies)
		{
			const FString DependencyFolded = DependencyPackage.ToLower();
			if (!IsValidPackageName(DependencyPackage) || DirectDependencies.Contains(DependencyFolded)
				|| DependencyPackage.Equals(Record.PackageName, ESearchCase::IgnoreCase))
			{
				Codes.AddUnique(TEXT("GAC034"));
			}
			DirectDependencies.Add(DependencyFolded);
		}
	}
	for (const FGloamsteadGeneratedAssetEntry& Entry : Catalog.Entries)
	{
		if (Entry.SemanticRole.IsNone() || !IsStableId(Entry.SemanticRole.ToString()))
		{
			Codes.AddUnique(TEXT("GAC004"));
		}
		const UEnum* StateEnum = StaticEnum<EGloamsteadGeneratedAssetState>();
		if (!StateEnum || !StateEnum->IsValidEnumValue(static_cast<int64>(Entry.RestorationState))
			|| Entry.RestorationState == EGloamsteadGeneratedAssetState::Unknown)
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
		TSet<FString> DirectPackageDependencies;
		for (const FString& DependencyPackage : Entry.DirectPackageDependencies)
		{
			const FString Folded = DependencyPackage.ToLower();
			if (!IsValidPackageName(DependencyPackage) || DirectPackageDependencies.Contains(Folded)
				|| DependencyPackage.Equals(Entry.Asset.ToSoftObjectPath().GetLongPackageName(), ESearchCase::IgnoreCase))
			{
				Codes.AddUnique(TEXT("GAC020"));
			}
			DirectPackageDependencies.Add(Folded);
		}
		TSet<FString> EntryDependencies;
		for (const TSoftObjectPtr<UObject>& Dependency : Entry.Dependencies)
		{
			ValidateObjectPath(Dependency.ToSoftObjectPath(), Catalog.VersionRoot, Codes);
			const FString DependencyPath = Dependency.ToSoftObjectPath().ToString();
			const FString FoldedDependencyPath = DependencyPath.ToLower();
			if (EntryDependencies.Contains(FoldedDependencyPath))
			{
				Codes.AddUnique(TEXT("GAC020"));
			}
			EntryDependencies.Add(FoldedDependencyPath);
			if (!CatalogAssets.Contains(FoldedDependencyPath)
				|| DependencyPath.Equals(Entry.Asset.ToSoftObjectPath().ToString(), ESearchCase::IgnoreCase))
			{
				Codes.AddUnique(TEXT("GAC021"));
			}
			if (!DirectPackageDependencies.Contains(
				Dependency.ToSoftObjectPath().GetLongPackageName().ToLower()))
			{
				Codes.AddUnique(TEXT("GAC021"));
			}
		}
		for (const FString& DependencyPackage : Entry.DirectPackageDependencies)
		{
			if (!IsPackageUnderRoot(DependencyPackage, Catalog.VersionRoot))
			{
				continue;
			}
			const FString* RequiredAssetPath = CatalogAssetByFoldedPackage.Find(DependencyPackage.ToLower());
			if (!RequiredAssetPath || !EntryDependencies.Contains(*RequiredAssetPath))
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

	// Every declared edge must resolve to a generated entry, a recursively declared external record,
	// or a narrowly scoped platform terminal. This pure pass catches incomplete authoring before I/O.
	auto ValidateDeclaredPackage = [&](const FString& DependencyPackage)
	{
		const FString Folded = DependencyPackage.ToLower();
		if (CatalogAssetByFoldedPackage.Contains(Folded))
		{
			return;
		}
		if (IsPackageUnderRoot(DependencyPackage, TEXT("/Game/Gloamstead/Generated")))
		{
			Codes.AddUnique(TEXT("GAC028"));
			return;
		}
		if (IsTerminalPlatformPackage(DependencyPackage,
			Catalog.TerminalPlatformPackages, Catalog.TerminalPlatformPackageRoots))
		{
			return;
		}
		if (!ExternalPackages.Contains(Folded))
		{
			Codes.AddUnique(TEXT("GAC033"));
		}
	};
	for (const FGloamsteadGeneratedAssetEntry& Entry : Catalog.Entries)
	{
		for (const FString& DependencyPackage : Entry.DirectPackageDependencies)
		{
			ValidateDeclaredPackage(DependencyPackage);
		}
	}
	for (const FGloamsteadGeneratedExternalPackageRecord& Record : Catalog.ExternalPackageRecords)
	{
		for (const FString& DependencyPackage : Record.DirectPackageDependencies)
		{
			ValidateDeclaredPackage(DependencyPackage);
		}
	}
	return Codes;
}

TArray<FString> GACValidateObservedProvenance(
	const FGloamsteadGeneratedAssetEntry& Entry,
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	const FGloamsteadGeneratedAssetObservedProvenance& Observed)
{
	TArray<FString> Codes;
	if (Observed.ObjectSha256.IsEmpty() || Observed.ReceiptSha256.IsEmpty() || Observed.BundleId.IsEmpty())
	{
		Codes.Add(TEXT("GAC023"));
		return Codes;
	}
	if (!Observed.ObjectSha256.Equals(Entry.ObjectSha256, ESearchCase::IgnoreCase)
		|| !Observed.ReceiptSha256.Equals(Catalog.ReceiptSha256, ESearchCase::IgnoreCase)
		|| Observed.BundleId != Catalog.BundleId)
	{
		Codes.Add(TEXT("GAC024"));
	}
	return Codes;
}

TArray<FString> GACValidateActiveBinding(
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	const FString& ExpectedBundleId,
	const FString& ExpectedReceiptSha256,
	const FString& ExpectedTargetBuildIdentitySha256)
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
	if (!IsSha256(ExpectedTargetBuildIdentitySha256)
		|| !Catalog.TargetBuildIdentitySha256.Equals(ExpectedTargetBuildIdentitySha256, ESearchCase::IgnoreCase))
	{
		Codes.Add(TEXT("GAC036"));
	}
	return Codes;
}

TArray<FString> GACValidateObservedProvenance(
	const FGloamsteadGeneratedExternalPackageRecord& Record,
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	const FGloamsteadGeneratedAssetObservedProvenance& Observed)
{
	TArray<FString> Codes;
	if (Observed.PackageSha256.IsEmpty() || Observed.ReceiptSha256.IsEmpty() || Observed.BundleId.IsEmpty())
	{
		Codes.Add(TEXT("GAC023"));
		return Codes;
	}
	if (!Observed.PackageSha256.Equals(Record.PackageSha256, ESearchCase::IgnoreCase)
		|| !Observed.ReceiptSha256.Equals(Catalog.ReceiptSha256, ESearchCase::IgnoreCase)
		|| Observed.BundleId != Catalog.BundleId)
	{
		Codes.Add(TEXT("GAC035"));
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
