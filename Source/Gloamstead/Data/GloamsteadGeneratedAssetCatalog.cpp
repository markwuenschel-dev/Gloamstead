#include "Data/GloamsteadGeneratedAssetCatalog.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Containers/StringConv.h"

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

	bool IsCanonicalIdentityValue(const FString& Value)
	{
		if (Value.IsEmpty() || Value == TEXT("unavailable"))
		{
			return false;
		}
		for (const TCHAR Ch : Value)
		{
			if (Ch < 0x20 || Ch > 0x7e || Ch == TEXT('=') || Ch == TEXT('\r') || Ch == TEXT('\n'))
			{
				return false;
			}
		}
		return true;
	}

	bool IsGitCommit(const FString& Value)
	{
		if (Value.Len() != 40 && Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Ch : Value)
		{
			if (!FChar::IsHexDigit(Ch) || FChar::IsUpper(Ch))
			{
				return false;
			}
		}
		return true;
	}

	bool IsWorldForgeBuildIdentity(const FString& Value)
	{
		static const FString Prefix = TEXT("wfplugin-");
		return Value.StartsWith(Prefix, ESearchCase::CaseSensitive)
			&& IsSha256(Value.Mid(Prefix.Len()))
			&& Value.Equals(Value.ToLower(), ESearchCase::CaseSensitive);
	}

	uint32 Sha256RotateRight(uint32 Value, uint32 Count)
	{
		return (Value >> Count) | (Value << (32 - Count));
	}

	FString Sha256Utf8(const FString& Value)
	{
		static constexpr uint32 K[64] = {
			0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
			0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
			0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
			0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
			0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
			0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
			0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
			0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
		uint32 H[8] = {
			0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
			0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };

		const FTCHARToUTF8 Utf8(*Value);
		const uint64 ByteLength = static_cast<uint64>(Utf8.Length());
		const uint64 PaddedLength = ((ByteLength + 9 + 63) / 64) * 64;
		if (PaddedLength > static_cast<uint64>(MAX_int32))
		{
			return FString();
		}
		TArray<uint8> Message;
		Message.SetNumZeroed(static_cast<int32>(PaddedLength));
		if (ByteLength > 0)
		{
			FMemory::Memcpy(Message.GetData(), Utf8.Get(), ByteLength);
		}
		Message[ByteLength] = 0x80;
		const uint64 BitLength = ByteLength * 8;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Message[PaddedLength - 1 - Index] = static_cast<uint8>(BitLength >> (Index * 8));
		}

		for (uint64 Offset = 0; Offset < PaddedLength; Offset += 64)
		{
			uint32 W[64]{};
			for (int32 Index = 0; Index < 16; ++Index)
			{
				const uint64 Base = Offset + static_cast<uint64>(Index * 4);
				W[Index] = (static_cast<uint32>(Message[Base]) << 24)
					| (static_cast<uint32>(Message[Base + 1]) << 16)
					| (static_cast<uint32>(Message[Base + 2]) << 8)
					| static_cast<uint32>(Message[Base + 3]);
			}
			for (int32 Index = 16; Index < 64; ++Index)
			{
				const uint32 S0 = Sha256RotateRight(W[Index - 15], 7)
					^ Sha256RotateRight(W[Index - 15], 18) ^ (W[Index - 15] >> 3);
				const uint32 S1 = Sha256RotateRight(W[Index - 2], 17)
					^ Sha256RotateRight(W[Index - 2], 19) ^ (W[Index - 2] >> 10);
				W[Index] = W[Index - 16] + S0 + W[Index - 7] + S1;
			}

			uint32 A=H[0], B=H[1], C=H[2], D=H[3], E=H[4], F=H[5], G=H[6], HH=H[7];
			for (int32 Index = 0; Index < 64; ++Index)
			{
				const uint32 S1 = Sha256RotateRight(E, 6) ^ Sha256RotateRight(E, 11) ^ Sha256RotateRight(E, 25);
				const uint32 Choice = (E & F) ^ ((~E) & G);
				const uint32 Temp1 = HH + S1 + Choice + K[Index] + W[Index];
				const uint32 S0 = Sha256RotateRight(A, 2) ^ Sha256RotateRight(A, 13) ^ Sha256RotateRight(A, 22);
				const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
				const uint32 Temp2 = S0 + Majority;
				HH=G; G=F; F=E; E=D+Temp1; D=C; C=B; B=A; A=Temp1+Temp2;
			}
			H[0]+=A; H[1]+=B; H[2]+=C; H[3]+=D; H[4]+=E; H[5]+=F; H[6]+=G; H[7]+=HH;
		}
		return FString::Printf(TEXT("%08x%08x%08x%08x%08x%08x%08x%08x"),
			H[0],H[1],H[2],H[3],H[4],H[5],H[6],H[7]);
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

	bool IsCanonicalScriptPackage(const FString& PackageName)
	{
		static const FString Prefix = TEXT("/Script/");
		if (!PackageName.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			return false;
		}
		const FString ModuleName = PackageName.Mid(Prefix.Len());
		if (ModuleName.IsEmpty()
			|| !(FChar::IsAlpha(ModuleName[0]) || ModuleName[0] == TEXT('_')))
		{
			return false;
		}
		for (const TCHAR Ch : ModuleName)
		{
			if (!(FChar::IsAlnum(Ch) || Ch == TEXT('_')))
			{
				return false;
			}
		}
		return true;
	}

	const TCHAR* ScriptOwnerToken(EGloamsteadGeneratedScriptPackageOwner OwnerClass)
	{
		switch (OwnerClass)
		{
		case EGloamsteadGeneratedScriptPackageOwner::Engine: return TEXT("engine");
		case EGloamsteadGeneratedScriptPackageOwner::GloamsteadProject: return TEXT("gloamstead_project");
		case EGloamsteadGeneratedScriptPackageOwner::WorldForgePlugin: return TEXT("worldforge_plugin");
		case EGloamsteadGeneratedScriptPackageOwner::ExternalPlugin: return TEXT("external_plugin");
		case EGloamsteadGeneratedScriptPackageOwner::Unknown:
		default: return TEXT("unknown");
		}
	}

	bool CanonicalPluginRecord(
		const FGloamsteadGeneratedEnabledPluginIdentity& Plugin,
		FString& OutCanonical)
	{
		OutCanonical.Reset();
		if (!IsStableId(Plugin.PluginName)
			|| !IsCanonicalIdentityValue(Plugin.PluginVersion)
			|| !IsSha256(Plugin.DescriptorSha256)
			|| !IsSha256(Plugin.InstalledPluginTreeSha256)
			|| !IsCanonicalIdentityValue(Plugin.BuildIdentity))
		{
			return false;
		}
		TArray<FString> Packages = Plugin.ScriptPackages;
		Packages.Sort([](const FString& A, const FString& B) { return A < B; });
		TSet<FString> Seen;
		OutCanonical = FString::Printf(
			TEXT("plugin=%s\nplugin_version=%s\nplugin_descriptor_sha256=%s\n")
			TEXT("plugin_installed_tree_sha256=%s\nplugin_build_identity=%s\n"),
			*Plugin.PluginName, *Plugin.PluginVersion,
			*Plugin.DescriptorSha256.ToLower(), *Plugin.InstalledPluginTreeSha256.ToLower(),
			*Plugin.BuildIdentity);
		for (const FString& PackageName : Packages)
		{
			const FString Folded = PackageName.ToLower();
			if (!IsCanonicalScriptPackage(PackageName) || Seen.Contains(Folded))
			{
				OutCanonical.Reset();
				return false;
			}
			Seen.Add(Folded);
			OutCanonical += FString::Printf(TEXT("plugin_script_package=%s\n"), *PackageName);
		}
		OutCanonical += FString::Printf(TEXT("plugin_end=%s\n"), *Plugin.PluginName);
		return true;
	}

	bool CanonicalEnabledPluginInventory(
		const TArray<FGloamsteadGeneratedEnabledPluginIdentity>& EnabledPlugins,
		FString& OutCanonical)
	{
		OutCanonical.Reset();
		if (EnabledPlugins.Num() == 0)
		{
			return false;
		}
		TArray<const FGloamsteadGeneratedEnabledPluginIdentity*> Sorted;
		for (const FGloamsteadGeneratedEnabledPluginIdentity& Plugin : EnabledPlugins)
		{
			Sorted.Add(&Plugin);
		}
		Sorted.Sort([](const FGloamsteadGeneratedEnabledPluginIdentity& A,
			const FGloamsteadGeneratedEnabledPluginIdentity& B)
		{
			return A.PluginName < B.PluginName;
		});
		TSet<FString> SeenPlugins;
		TSet<FString> SeenPackages;
		OutCanonical = TEXT("gloamstead.enabled-plugin-inventory@1\n");
		for (const FGloamsteadGeneratedEnabledPluginIdentity* Plugin : Sorted)
		{
			const FString FoldedPlugin = Plugin->PluginName.ToLower();
			if (SeenPlugins.Contains(FoldedPlugin))
			{
				OutCanonical.Reset();
				return false;
			}
			SeenPlugins.Add(FoldedPlugin);
			FString Record;
			if (!CanonicalPluginRecord(*Plugin, Record))
			{
				OutCanonical.Reset();
				return false;
			}
			for (const FString& PackageName : Plugin->ScriptPackages)
			{
				const FString FoldedPackage = PackageName.ToLower();
				if (SeenPackages.Contains(FoldedPackage))
				{
					OutCanonical.Reset();
					return false;
				}
				SeenPackages.Add(FoldedPackage);
			}
			OutCanonical += Record;
		}
		return true;
	}

	FString CoreOwnerIdentitySha256(
		EGloamsteadGeneratedScriptPackageOwner OwnerClass,
		const FGloamsteadGeneratedAssetRuntimeIdentity& Identity)
	{
		if (OwnerClass == EGloamsteadGeneratedScriptPackageOwner::Engine)
		{
			return Sha256Utf8(FString::Printf(
				TEXT("gloamstead.script-owner.engine@1\nengine_version=%s\n")
				TEXT("compatible_engine_version=%s\nengine_build_version=%s\n")
				TEXT("engine_changelist=%u\ncompatible_engine_changelist=%u\n"),
				*Identity.EngineVersion, *Identity.CompatibleEngineVersion,
				*Identity.EngineBuildVersion, Identity.EngineChangelist,
				Identity.CompatibleEngineChangelist));
		}
		if (OwnerClass == EGloamsteadGeneratedScriptPackageOwner::GloamsteadProject)
		{
			return Sha256Utf8(FString::Printf(
				TEXT("gloamstead.script-owner.project@1\ngloamstead_commit=%s\nengine_build_version=%s\n"),
				*Identity.GloamsteadCommit, *Identity.EngineBuildVersion));
		}
		return FString();
	}

	bool DeriveTerminalAuthoritiesInternal(
		const FGloamsteadGeneratedAssetRuntimeIdentity& Identity,
		TArray<FGloamsteadGeneratedScriptPackageAuthority>& OutAuthorities)
	{
		OutAuthorities.Reset();
		if (Identity.EngineScriptPackages.Num() == 0 || Identity.GloamsteadScriptPackages.Num() == 0)
		{
			return false;
		}
		TSet<FString> SeenPackages;
		auto AddAuthority = [&](const FString& PackageName,
			EGloamsteadGeneratedScriptPackageOwner OwnerClass,
			const FString& OwnerId,
			const FString& OwnerIdentitySha256)
		{
			const FString Folded = PackageName.ToLower();
			if (!IsCanonicalScriptPackage(PackageName) || SeenPackages.Contains(Folded)
				|| !IsStableId(OwnerId) || !IsSha256(OwnerIdentitySha256))
			{
				return false;
			}
			SeenPackages.Add(Folded);
			FGloamsteadGeneratedScriptPackageAuthority Authority;
			Authority.PackageName = PackageName;
			Authority.OwnerClass = OwnerClass;
			Authority.OwnerId = OwnerId;
			Authority.OwnerIdentitySha256 = OwnerIdentitySha256.ToLower();
			OutAuthorities.Add(MoveTemp(Authority));
			return true;
		};

		const FString EngineIdentity = CoreOwnerIdentitySha256(
			EGloamsteadGeneratedScriptPackageOwner::Engine, Identity);
		for (const FString& PackageName : Identity.EngineScriptPackages)
		{
			if (!AddAuthority(PackageName, EGloamsteadGeneratedScriptPackageOwner::Engine,
				TEXT("UnrealEngine"), EngineIdentity))
			{
				OutAuthorities.Reset();
				return false;
			}
		}
		const FString ProjectIdentity = CoreOwnerIdentitySha256(
			EGloamsteadGeneratedScriptPackageOwner::GloamsteadProject, Identity);
		for (const FString& PackageName : Identity.GloamsteadScriptPackages)
		{
			if (!AddAuthority(PackageName, EGloamsteadGeneratedScriptPackageOwner::GloamsteadProject,
				TEXT("Gloamstead"), ProjectIdentity))
			{
				OutAuthorities.Reset();
				return false;
			}
		}

		bool bFoundWorldForge = false;
		for (const FGloamsteadGeneratedEnabledPluginIdentity& Plugin : Identity.EnabledPlugins)
		{
			FString PluginCanonical;
			if (!CanonicalPluginRecord(Plugin, PluginCanonical))
			{
				OutAuthorities.Reset();
				return false;
			}
			const bool bWorldForge = Plugin.PluginName.Equals(TEXT("WorldForge"), ESearchCase::CaseSensitive);
			if (bWorldForge)
			{
				bFoundWorldForge = true;
				if (Plugin.PluginVersion != Identity.PluginVersion
					|| !Plugin.DescriptorSha256.Equals(Identity.PluginDescriptorSha256, ESearchCase::IgnoreCase)
					|| !Plugin.InstalledPluginTreeSha256.Equals(Identity.InstalledPluginTreeSha256, ESearchCase::IgnoreCase)
					|| Plugin.BuildIdentity != Identity.DeclaredPluginBuildIdentity)
				{
					OutAuthorities.Reset();
					return false;
				}
			}
			const FString PluginIdentity = Sha256Utf8(PluginCanonical);
			for (const FString& PackageName : Plugin.ScriptPackages)
			{
				if (!AddAuthority(PackageName,
					bWorldForge
						? EGloamsteadGeneratedScriptPackageOwner::WorldForgePlugin
						: EGloamsteadGeneratedScriptPackageOwner::ExternalPlugin,
					Plugin.PluginName, PluginIdentity))
				{
					OutAuthorities.Reset();
					return false;
				}
			}
		}
		if (!bFoundWorldForge)
		{
			OutAuthorities.Reset();
			return false;
		}
		OutAuthorities.Sort([](const auto& A, const auto& B) { return A.PackageName < B.PackageName; });
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

const FString& GACWorldForgeVendorLockRelativePath()
{
	static const FString Path = TEXT("specs/worldforge_asset_forge/worldforge-plugin.lock.json");
	return Path;
}

bool GACCanonicalRuntimeIdentity(
	const FGloamsteadGeneratedAssetRuntimeIdentity& Identity,
	FString& OutCanonical,
	TArray<FString>& OutFailureCodes)
{
	OutCanonical.Reset();
	OutFailureCodes.Reset();
	const bool bTextAxesValid =
		IsCanonicalIdentityValue(Identity.EngineVersion)
		&& IsCanonicalIdentityValue(Identity.CompatibleEngineVersion)
		&& IsCanonicalIdentityValue(Identity.EngineBuildVersion)
		&& IsGitCommit(Identity.GloamsteadCommit)
		&& IsCanonicalIdentityValue(Identity.PluginVersion)
		&& IsCanonicalIdentityValue(Identity.PluginEngineVersion)
		&& Identity.EngineChangelist > 0
		&& Identity.CompatibleEngineChangelist > 0;
	const bool bHashAxesValid =
		IsSha256(Identity.PluginDescriptorSha256)
		&& IsSha256(Identity.InstalledPluginTreeSha256)
		&& IsSha256(Identity.VendorLockSha256)
		&& IsSha256(Identity.DeclaredPluginPackageSha256)
		&& IsWorldForgeBuildIdentity(Identity.DeclaredPluginBuildIdentity)
		&& IsSha256(Identity.EnabledPluginInventorySha256);
	if (!bTextAxesValid || !bHashAxesValid)
	{
		OutFailureCodes.Add(TEXT("GAC037"));
		return false;
	}
	FString PluginInventoryCanonical;
	if (!CanonicalEnabledPluginInventory(Identity.EnabledPlugins, PluginInventoryCanonical)
		|| !Sha256Utf8(PluginInventoryCanonical).Equals(
			Identity.EnabledPluginInventorySha256, ESearchCase::IgnoreCase))
	{
		OutFailureCodes.Add(TEXT("GAC037"));
		return false;
	}
	TArray<FGloamsteadGeneratedScriptPackageAuthority> Authorities;
	if (!DeriveTerminalAuthoritiesInternal(Identity, Authorities))
	{
		OutFailureCodes.Add(TEXT("GAC037"));
		return false;
	}

	OutCanonical = FString::Printf(
		TEXT("gloamstead.worldforge.runtime-identity@1\n")
		TEXT("engine_version=%s\n")
		TEXT("compatible_engine_version=%s\n")
		TEXT("engine_build_version=%s\n")
		TEXT("engine_changelist=%u\n")
		TEXT("compatible_engine_changelist=%u\n")
		TEXT("gloamstead_commit=%s\n")
		TEXT("plugin_version=%s\n")
		TEXT("plugin_engine_version=%s\n")
		TEXT("plugin_descriptor_sha256=%s\n")
		TEXT("installed_plugin_tree_sha256=%s\n")
		TEXT("vendor_lock_sha256=%s\n")
		TEXT("declared_plugin_package_sha256=%s\n")
		TEXT("declared_plugin_build_identity=%s\n")
		TEXT("enabled_plugin_inventory_sha256=%s\n"),
		*Identity.EngineVersion,
		*Identity.CompatibleEngineVersion,
		*Identity.EngineBuildVersion,
		Identity.EngineChangelist,
		Identity.CompatibleEngineChangelist,
		*Identity.GloamsteadCommit,
		*Identity.PluginVersion,
		*Identity.PluginEngineVersion,
		*Identity.PluginDescriptorSha256.ToLower(),
		*Identity.InstalledPluginTreeSha256.ToLower(),
		*Identity.VendorLockSha256.ToLower(),
		*Identity.DeclaredPluginPackageSha256.ToLower(),
		*Identity.DeclaredPluginBuildIdentity.ToLower(),
		*Identity.EnabledPluginInventorySha256.ToLower());
	for (const FGloamsteadGeneratedScriptPackageAuthority& Authority : Authorities)
	{
		OutCanonical += FString::Printf(TEXT("terminal_script_authority=%s|%s|%s|%s\n"),
			*Authority.PackageName, ScriptOwnerToken(Authority.OwnerClass), *Authority.OwnerId,
			*Authority.OwnerIdentitySha256.ToLower());
	}
	return true;
}

FString GACEnabledPluginInventorySha256(
	const TArray<FGloamsteadGeneratedEnabledPluginIdentity>& EnabledPlugins)
{
	FString Canonical;
	return CanonicalEnabledPluginInventory(EnabledPlugins, Canonical)
		? Sha256Utf8(Canonical)
		: FString();
}

bool GACDeriveTerminalScriptPackageAuthorities(
	const FGloamsteadGeneratedAssetRuntimeIdentity& Identity,
	TArray<FGloamsteadGeneratedScriptPackageAuthority>& OutAuthorities,
	TArray<FString>& OutFailureCodes)
{
	OutFailureCodes.Reset();
	FString Canonical;
	if (!GACCanonicalRuntimeIdentity(Identity, Canonical, OutFailureCodes))
	{
		OutAuthorities.Reset();
		return false;
	}
	if (!DeriveTerminalAuthoritiesInternal(Identity, OutAuthorities))
	{
		OutFailureCodes.AddUnique(TEXT("GAC037"));
		return false;
	}
	return true;
}

FString GACRuntimeIdentitySha256(
	const FGloamsteadGeneratedAssetRuntimeIdentity& Identity,
	TArray<FString>* OutFailureCodes)
{
	FString Canonical;
	TArray<FString> FailureCodes;
	if (!GACCanonicalRuntimeIdentity(Identity, Canonical, FailureCodes))
	{
		if (OutFailureCodes)
		{
			*OutFailureCodes = MoveTemp(FailureCodes);
		}
		return FString();
	}
	const FString Hash = Sha256Utf8(Canonical);
	if (Hash.IsEmpty())
	{
		FailureCodes.AddUnique(TEXT("GAC037"));
	}
	if (OutFailureCodes)
	{
		*OutFailureCodes = MoveTemp(FailureCodes);
	}
	return Hash;
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

FString GACCanonicalCatalogContract(const UGloamsteadGeneratedAssetCatalog& Catalog)
{
	auto AppendField = [](FString& Out, const TCHAR* Name, const FString& Value)
	{
		const FTCHARToUTF8 Utf8(*Value);
		Out += FString::Printf(TEXT("%s=%d:"), Name, Utf8.Length());
		Out += Value;
		Out += TEXT("\n");
	};
	auto AppendSortedValues = [&AppendField](
		FString& Out, const TCHAR* CountName, const TCHAR* ItemName, TArray<FString> Values)
	{
		Values.Sort([](const FString& A, const FString& B) { return A < B; });
		AppendField(Out, CountName, FString::FromInt(Values.Num()));
		for (const FString& Value : Values)
		{
			AppendField(Out, ItemName, Value);
		}
	};
	auto CanonicalAuthority = [&AppendField](
		const FGloamsteadGeneratedScriptPackageAuthority& Authority)
	{
		FString Out;
		AppendField(Out, TEXT("package"), Authority.PackageName);
		AppendField(Out, TEXT("owner_class"),
			FString::FromInt(static_cast<int32>(Authority.OwnerClass)));
		AppendField(Out, TEXT("owner_id"), Authority.OwnerId);
		AppendField(Out, TEXT("owner_identity_sha256"), Authority.OwnerIdentitySha256);
		return Out;
	};
	auto CanonicalExternal = [&AppendField, &AppendSortedValues](
		const FGloamsteadGeneratedExternalPackageRecord& Record)
	{
		FString Out;
		AppendField(Out, TEXT("package"), Record.PackageName);
		AppendField(Out, TEXT("provenance_object"),
			Record.ProvenanceObject.ToSoftObjectPath().ToString());
		AppendField(Out, TEXT("package_sha256"), Record.PackageSha256);
		AppendField(Out, TEXT("receipt_sha256"), Record.ReceiptSha256);
		AppendField(Out, TEXT("bundle_id"), Record.BundleId);
		AppendSortedValues(Out, TEXT("direct_dependency_count"), TEXT("direct_dependency"),
			Record.DirectPackageDependencies);
		return Out;
	};
	auto CanonicalEntry = [&AppendField, &AppendSortedValues](
		const FGloamsteadGeneratedAssetEntry& Entry)
	{
		FString Out;
		AppendField(Out, TEXT("semantic_role"), Entry.SemanticRole.ToString());
		AppendField(Out, TEXT("restoration_state"),
			FString::FromInt(static_cast<int32>(Entry.RestorationState)));
		AppendField(Out, TEXT("asset"), Entry.Asset.ToSoftObjectPath().ToString());
		AppendField(Out, TEXT("expected_class"), Entry.ExpectedClass.ToSoftObjectPath().ToString());
		AppendField(Out, TEXT("object_sha256"), Entry.ObjectSha256);
		AppendField(Out, TEXT("receipt_sha256"), Entry.ReceiptSha256);
		AppendSortedValues(Out, TEXT("direct_dependency_count"), TEXT("direct_dependency"),
			Entry.DirectPackageDependencies);
		TArray<FString> DependencyPaths;
		DependencyPaths.Reserve(Entry.Dependencies.Num());
		for (const TSoftObjectPtr<UObject>& Dependency : Entry.Dependencies)
		{
			DependencyPaths.Add(Dependency.ToSoftObjectPath().ToString());
		}
		AppendSortedValues(Out, TEXT("object_dependency_count"), TEXT("object_dependency"),
			MoveTemp(DependencyPaths));
		AppendField(Out, TEXT("ownership_id"), Entry.OwnershipId);
		AppendField(Out, TEXT("license_id"), Entry.LicenseId);
		return Out;
	};

	FString Canonical = TEXT("gloamstead.generated-asset-catalog-contract@1\n");
	AppendField(Canonical, TEXT("bundle_id"), Catalog.BundleId);
	AppendField(Canonical, TEXT("receipt_sha256"), Catalog.ReceiptSha256);
	AppendField(Canonical, TEXT("version_root"), Catalog.VersionRoot);
	AppendField(Canonical, TEXT("target_build_identity_sha256"),
		Catalog.TargetBuildIdentitySha256);
	AppendSortedValues(Canonical, TEXT("terminal_root_count"), TEXT("terminal_root"),
		Catalog.TerminalPlatformPackageRoots);
	AppendSortedValues(Canonical, TEXT("terminal_package_count"), TEXT("terminal_package"),
		Catalog.TerminalPlatformPackages);

	TArray<FString> Authorities;
	Authorities.Reserve(Catalog.TerminalScriptPackageAuthorities.Num());
	for (const FGloamsteadGeneratedScriptPackageAuthority& Authority
		: Catalog.TerminalScriptPackageAuthorities)
	{
		Authorities.Add(CanonicalAuthority(Authority));
	}
	AppendSortedValues(Canonical, TEXT("script_authority_count"), TEXT("script_authority"),
		MoveTemp(Authorities));

	TArray<FString> ExternalRecords;
	ExternalRecords.Reserve(Catalog.ExternalPackageRecords.Num());
	for (const FGloamsteadGeneratedExternalPackageRecord& Record : Catalog.ExternalPackageRecords)
	{
		ExternalRecords.Add(CanonicalExternal(Record));
	}
	AppendSortedValues(Canonical, TEXT("external_record_count"), TEXT("external_record"),
		MoveTemp(ExternalRecords));

	TArray<FString> Entries;
	Entries.Reserve(Catalog.Entries.Num());
	for (const FGloamsteadGeneratedAssetEntry& Entry : Catalog.Entries)
	{
		Entries.Add(CanonicalEntry(Entry));
	}
	AppendSortedValues(Canonical, TEXT("entry_count"), TEXT("entry"), MoveTemp(Entries));
	return Canonical;
}

FString GACCatalogContractSha256(const UGloamsteadGeneratedAssetCatalog& Catalog)
{
	return Sha256Utf8(GACCanonicalCatalogContract(Catalog));
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
		if (!Root.Equals(TEXT("/Engine"), ESearchCase::CaseSensitive)
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
		const bool bPlatformPackage = IsPackageUnderRoot(PackageName, TEXT("/Engine"));
		if (!IsValidPackageName(PackageName) || !bPlatformPackage
			|| PackageName.Equals(TEXT("/Engine"), ESearchCase::IgnoreCase)
			|| SeenTerminalPackages.Contains(Folded)
			|| IsTerminalPlatformPackage(PackageName, {}, Catalog.TerminalPlatformPackageRoots))
		{
			Codes.AddUnique(TEXT("GAC034"));
		}
		SeenTerminalPackages.Add(Folded);
	}
	TSet<FString> TerminalScriptPackages;
	for (const FGloamsteadGeneratedScriptPackageAuthority& Authority
		: Catalog.TerminalScriptPackageAuthorities)
	{
		const FString Folded = Authority.PackageName.ToLower();
		const UEnum* OwnerEnum = StaticEnum<EGloamsteadGeneratedScriptPackageOwner>();
		const bool bOwnerValid = OwnerEnum
			&& OwnerEnum->IsValidEnumValue(static_cast<int64>(Authority.OwnerClass))
			&& Authority.OwnerClass != EGloamsteadGeneratedScriptPackageOwner::Unknown;
		bool bOwnerIdValid = IsStableId(Authority.OwnerId);
		if (Authority.OwnerClass == EGloamsteadGeneratedScriptPackageOwner::Engine)
		{
			bOwnerIdValid = Authority.OwnerId == TEXT("UnrealEngine");
		}
		else if (Authority.OwnerClass == EGloamsteadGeneratedScriptPackageOwner::GloamsteadProject)
		{
			bOwnerIdValid = Authority.OwnerId == TEXT("Gloamstead");
		}
		else if (Authority.OwnerClass == EGloamsteadGeneratedScriptPackageOwner::WorldForgePlugin)
		{
			bOwnerIdValid = Authority.OwnerId == TEXT("WorldForge");
		}
		if (!IsCanonicalScriptPackage(Authority.PackageName)
			|| TerminalScriptPackages.Contains(Folded)
			|| !bOwnerValid || !bOwnerIdValid || !IsSha256(Authority.OwnerIdentitySha256))
		{
			Codes.AddUnique(TEXT("GAC034"));
		}
		TerminalScriptPackages.Add(Folded);
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
			if ((!IsValidPackageName(DependencyPackage) && !IsCanonicalScriptPackage(DependencyPackage))
				|| DirectDependencies.Contains(DependencyFolded)
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
			if ((!IsValidPackageName(DependencyPackage) && !IsCanonicalScriptPackage(DependencyPackage))
				|| DirectPackageDependencies.Contains(Folded)
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
		if (TerminalScriptPackages.Contains(Folded))
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
	const FString& ExpectedTargetBuildIdentitySha256,
	const FGloamsteadGeneratedAssetRuntimeIdentity& ObservedRuntimeIdentity)
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
	TArray<FString> IdentityFailures;
	const FString ObservedIdentitySha256 = GACRuntimeIdentitySha256(ObservedRuntimeIdentity, &IdentityFailures);
	if (IdentityFailures.Num() > 0)
	{
		Codes.AddUnique(TEXT("GAC037"));
	}
	if (!IsSha256(ExpectedTargetBuildIdentitySha256) || !IsSha256(ObservedIdentitySha256)
		|| !Catalog.TargetBuildIdentitySha256.Equals(ExpectedTargetBuildIdentitySha256, ESearchCase::IgnoreCase)
		|| !Catalog.TargetBuildIdentitySha256.Equals(ObservedIdentitySha256, ESearchCase::IgnoreCase))
	{
		Codes.Add(TEXT("GAC036"));
	}
	TArray<FGloamsteadGeneratedScriptPackageAuthority> ObservedAuthorities;
	TArray<FString> AuthorityFailures;
	if (!GACDeriveTerminalScriptPackageAuthorities(
		ObservedRuntimeIdentity, ObservedAuthorities, AuthorityFailures))
	{
		Codes.AddUnique(TEXT("GAC037"));
	}
	else
	{
		TMap<FString, const FGloamsteadGeneratedScriptPackageAuthority*> ExpectedByPackage;
		TMap<FString, const FGloamsteadGeneratedScriptPackageAuthority*> ObservedByPackage;
		for (const FGloamsteadGeneratedScriptPackageAuthority& Authority
			: Catalog.TerminalScriptPackageAuthorities)
		{
			ExpectedByPackage.Add(Authority.PackageName.ToLower(), &Authority);
		}
		for (const FGloamsteadGeneratedScriptPackageAuthority& Authority : ObservedAuthorities)
		{
			ObservedByPackage.Add(Authority.PackageName.ToLower(), &Authority);
		}
		bool bAuthorityMismatch = ExpectedByPackage.Num() != ObservedByPackage.Num();
		for (const TPair<FString, const FGloamsteadGeneratedScriptPackageAuthority*>& Pair
			: ExpectedByPackage)
		{
			const FGloamsteadGeneratedScriptPackageAuthority* const* Observed =
				ObservedByPackage.Find(Pair.Key);
			if (!Observed
				|| (*Observed)->PackageName != Pair.Value->PackageName
				|| (*Observed)->OwnerClass != Pair.Value->OwnerClass
				|| (*Observed)->OwnerId != Pair.Value->OwnerId
				|| !(*Observed)->OwnerIdentitySha256.Equals(
					Pair.Value->OwnerIdentitySha256, ESearchCase::IgnoreCase))
			{
				bAuthorityMismatch = true;
			}
		}
		if (bAuthorityMismatch)
		{
			Codes.AddUnique(TEXT("GAC038"));
		}
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
