#include "Misc/AutomationTest.h"
#include "Data/GloamsteadGeneratedAssetCatalog.h"
#include "Systems/GloamsteadMeshForgeProvider.h"
#include "Settings/GloamsteadGeneratedAssetSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGloamsteadGeneratedAssetRuntimeIdentity MakeObservedRuntimeIdentity()
	{
		FGloamsteadGeneratedAssetRuntimeIdentity Identity;
		Identity.EngineVersion = TEXT("5.8.0-55116800+++UE5+Release-5.8");
		Identity.CompatibleEngineVersion = TEXT("5.8.0-55116800+++UE5+Release-5.8");
		Identity.EngineBuildVersion = TEXT("++UE5+Release-5.8-CL-55116800");
		Identity.EngineChangelist = 55116800;
		Identity.CompatibleEngineChangelist = 55116800;
		Identity.GloamsteadCommit = TEXT("0123456789abcdef0123456789abcdef01234567");
		Identity.PluginVersion = TEXT("0.2.0");
		Identity.PluginEngineVersion = TEXT("5.8.0");
		Identity.PluginDescriptorSha256 = TEXT("1111111111111111111111111111111111111111111111111111111111111111");
		Identity.InstalledPluginTreeSha256 = TEXT("2222222222222222222222222222222222222222222222222222222222222222");
		Identity.VendorLockSha256 = TEXT("3333333333333333333333333333333333333333333333333333333333333333");
		Identity.DeclaredPluginPackageSha256 = TEXT("4444444444444444444444444444444444444444444444444444444444444444");
		Identity.DeclaredPluginBuildIdentity = TEXT("wfplugin-5555555555555555555555555555555555555555555555555555555555555555");
		Identity.EngineScriptPackages = { TEXT("/Script/Engine") };
		Identity.GloamsteadScriptPackages = { TEXT("/Script/Gloamstead") };
		FGloamsteadGeneratedEnabledPluginIdentity WorldForge;
		WorldForge.PluginName = TEXT("WorldForge");
		WorldForge.PluginVersion = Identity.PluginVersion;
		WorldForge.DescriptorSha256 = Identity.PluginDescriptorSha256;
		WorldForge.InstalledPluginTreeSha256 = Identity.InstalledPluginTreeSha256;
		WorldForge.BuildIdentity = Identity.DeclaredPluginBuildIdentity;
		WorldForge.ScriptPackages = { TEXT("/Script/WorldForgeCore") };
		FGloamsteadGeneratedEnabledPluginIdentity ContentOnly;
		ContentOnly.PluginName = TEXT("ContentOnlyFX");
		ContentOnly.PluginVersion = TEXT("1.4.2");
		ContentOnly.DescriptorSha256 =
			TEXT("6666666666666666666666666666666666666666666666666666666666666666");
		ContentOnly.InstalledPluginTreeSha256 =
			TEXT("7777777777777777777777777777777777777777777777777777777777777777");
		ContentOnly.BuildIdentity = TEXT("content-only-1.4.2");
		Identity.EnabledPlugins = { WorldForge, ContentOnly };
		Identity.EnabledPluginInventorySha256 = GACEnabledPluginInventorySha256(Identity.EnabledPlugins);
		return Identity;
	}

	FGloamsteadGeneratedAssetEntry MakeValidMeshEntry(
		FName Role = TEXT("sanctuary.heart"),
		EGloamsteadGeneratedAssetState State = EGloamsteadGeneratedAssetState::Restored)
	{
		FGloamsteadGeneratedAssetEntry Entry;
		Entry.SemanticRole = Role;
		Entry.RestorationState = State;
		Entry.Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(
			TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/SM_Heart.SM_Heart")));
		Entry.ExpectedClass = UStaticMesh::StaticClass();
		Entry.ObjectSha256 = TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		Entry.ReceiptSha256 = TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
		Entry.OwnershipId = TEXT("gloamstead");
		Entry.LicenseId = TEXT("LicenseRef-001");
		return Entry;
	}

	UGloamsteadGeneratedAssetCatalog* MakeValidCatalog()
	{
		UGloamsteadGeneratedAssetCatalog* Catalog = NewObject<UGloamsteadGeneratedAssetCatalog>();
		Catalog->BundleId = TEXT("sanctuary-v1");
		Catalog->ReceiptSha256 = TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
		Catalog->VersionRoot = TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1");
		Catalog->TargetBuildIdentitySha256 = GACRuntimeIdentitySha256(MakeObservedRuntimeIdentity());
		TArray<FString> AuthorityFailures;
		GACDeriveTerminalScriptPackageAuthorities(
			MakeObservedRuntimeIdentity(), Catalog->TerminalScriptPackageAuthorities, AuthorityFailures);
		Catalog->Entries.Add(MakeValidMeshEntry());
		return Catalog;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamGeneratedAssetCatalogExactLookupTest,
	"Gloamstead.GeneratedAssets.ValidCatalogAndExactLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamGeneratedAssetCatalogExactLookupTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadGeneratedAssetCatalog* Catalog = MakeValidCatalog();
	FGloamsteadGeneratedAssetEntry Progress = MakeValidMeshEntry(
		TEXT("sanctuary.ritual_point"), EGloamsteadGeneratedAssetState::RestorationInProgress);
	Progress.Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/SM_RitualProgress.SM_RitualProgress")));
	Catalog->Entries.Add(Progress);
	TestEqual(TEXT("valid catalog has no failures"), GACValidateCatalog(*Catalog).Num(), 0);

	const FGloamsteadGeneratedAssetEntry* Exact = Catalog->FindExact(
		TEXT("sanctuary.heart"), EGloamsteadGeneratedAssetState::Restored);
	TestNotNull(TEXT("exact role/state resolves"), Exact);
	TestNull(TEXT("different state does not fall back"), Catalog->FindExact(
		TEXT("sanctuary.heart"), EGloamsteadGeneratedAssetState::Before));
	TestNull(TEXT("different role does not fuzzy-match"), Catalog->FindExact(
		TEXT("sanctuary.hear"), EGloamsteadGeneratedAssetState::Restored));
	TestNotNull(TEXT("restoration-in-progress is an exact supported state"), Catalog->FindExact(
		TEXT("sanctuary.ritual_point"), EGloamsteadGeneratedAssetState::RestorationInProgress));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamGeneratedAssetCatalogFailClosedTest,
	"Gloamstead.GeneratedAssets.CatalogValidationFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamGeneratedAssetCatalogFailClosedTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadGeneratedAssetCatalog* Catalog = MakeValidCatalog();
	Catalog->BundleId = TEXT("not a valid id");
	TestTrue(TEXT("invalid bundle id -> GAC001"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC001")));
	Catalog->BundleId = TEXT("sanctuary-v1");
	Catalog->VersionRoot = TEXT("/Game/Gloamstead/Generated/Biomes/Elsewhere/v1");
	TestTrue(TEXT("wrong immutable root -> GAC002"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC002")));
	Catalog->VersionRoot = TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1");

	const FGloamsteadGeneratedAssetEntry DuplicateKey = Catalog->Entries[0];
	Catalog->Entries.Add(DuplicateKey);
	TestTrue(TEXT("duplicate exact key -> GAC005"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC005")));
	Catalog->Entries.SetNum(1);

	Catalog->Entries[0].Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	TestTrue(TEXT("path outside generated root -> GAC007"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC007")));

	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v2/SM_Heart.SM_Heart")));
	TestTrue(TEXT("wrong catalog version -> GAC008"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC008")));

	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].SemanticRole = NAME_None;
	TestTrue(TEXT("empty role -> GAC004"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC004")));
	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].RestorationState = EGloamsteadGeneratedAssetState::Unknown;
	TestTrue(TEXT("unknown state -> GAC006"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC006")));
	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].RestorationState = static_cast<EGloamsteadGeneratedAssetState>(255);
	TestTrue(TEXT("out-of-range state -> GAC006"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC006")));
	TestNull(TEXT("out-of-range state never resolves"), Catalog->FindExact(
		TEXT("sanctuary.heart"), static_cast<EGloamsteadGeneratedAssetState>(255)));

	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].OwnershipId.Reset();
	TestTrue(TEXT("missing ownership -> GAC009"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC009")));
	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].LicenseId.Reset();
	TestTrue(TEXT("missing license -> GAC010"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC010")));
	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].ExpectedClass.Reset();
	TestTrue(TEXT("missing expected class -> GAC011"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC011")));
	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].ObjectSha256.Reset();
	TestTrue(TEXT("missing object hash -> GAC012"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC012")));
	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].ReceiptSha256.Reset();
	TestTrue(TEXT("missing entry receipt binding -> GAC003"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC003")));
	Catalog->Entries[0] = MakeValidMeshEntry();
	UTexture2D* WrongObject = NewObject<UTexture2D>();
	TestTrue(TEXT("loaded object wrong class -> GAC018"),
		GACValidateLoadedObject(Catalog->Entries[0], WrongObject, true).Contains(TEXT("GAC018")));

	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].ExpectedClass = UTexture2D::StaticClass();
	TestTrue(TEXT("unsupported MeshForge class -> GAC013"),
		GACValidateCatalog(*Catalog, true).Contains(TEXT("GAC013")));
	Catalog->Entries[0] = MakeValidMeshEntry();
	Catalog->Entries[0].ExpectedClass = TSoftClassPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/BP_Unloaded.BP_Unloaded_C")));
	TestFalse(TEXT("unloaded soft class is not falsely classified as unsupported"),
		GACValidateCatalog(*Catalog, true).Contains(TEXT("GAC013")));

	Catalog->Entries[0] = MakeValidMeshEntry();
	TestTrue(TEXT("stale bundle -> GAC014"),
		GACValidateActiveBinding(*Catalog, TEXT("sanctuary-v2"), Catalog->ReceiptSha256,
			Catalog->TargetBuildIdentitySha256, MakeObservedRuntimeIdentity()).Contains(TEXT("GAC014")));
	TestTrue(TEXT("stale receipt -> GAC015"),
		GACValidateActiveBinding(*Catalog, Catalog->BundleId,
			TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"),
			Catalog->TargetBuildIdentitySha256, MakeObservedRuntimeIdentity()).Contains(TEXT("GAC015")));
	TestTrue(TEXT("stale target build identity -> GAC036"),
		GACValidateActiveBinding(*Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
			TEXT("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"),
			MakeObservedRuntimeIdentity())
			.Contains(TEXT("GAC036")));
	Catalog->TerminalPlatformPackageRoots = { TEXT("/Game") };
	TestTrue(TEXT("terminal policy is limited to safe platform roots -> GAC034"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC034")));
	Catalog->TerminalPlatformPackageRoots.Reset();
	Catalog->TerminalPlatformPackageRoots = { TEXT("/Script") };
	TestTrue(TEXT("broad script terminal root is always rejected -> GAC034"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC034")));
	Catalog->TerminalPlatformPackageRoots.Reset();
	Catalog->TerminalPlatformPackages = { TEXT("/Game/Shared/Opaque") };
	TestTrue(TEXT("arbitrary game package cannot be terminal -> GAC034"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC034")));
	Catalog->TerminalPlatformPackages.Reset();
	Catalog->TerminalPlatformPackages = { TEXT("/Script/NeoStackAI") };
	TestTrue(TEXT("legacy exact script allowlists are rejected -> GAC034"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC034")));
	Catalog->TerminalPlatformPackages.Reset();
	FGloamsteadGeneratedScriptPackageAuthority ForgedNeoStack;
	ForgedNeoStack.PackageName = TEXT("/Script/NeoStackAI");
	ForgedNeoStack.OwnerClass = EGloamsteadGeneratedScriptPackageOwner::ExternalPlugin;
	ForgedNeoStack.OwnerId = TEXT("NeoStackAI");
	ForgedNeoStack.OwnerIdentitySha256 =
		TEXT("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
	Catalog->TerminalScriptPackageAuthorities.Add(ForgedNeoStack);
	TestTrue(TEXT("a catalog-authored NeoStackAI authority absent from trusted inventory -> GAC038"),
		GACValidateActiveBinding(*Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
			Catalog->TargetBuildIdentitySha256, MakeObservedRuntimeIdentity()).Contains(TEXT("GAC038")));
	Catalog->TerminalScriptPackageAuthorities.Pop();
	Catalog->TerminalScriptPackageAuthorities[0].OwnerId = TEXT("ForgedEngineOwner");
	TestTrue(TEXT("owner drift for an exact script package -> GAC038"),
		GACValidateActiveBinding(*Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
			Catalog->TargetBuildIdentitySha256, MakeObservedRuntimeIdentity()).Contains(TEXT("GAC038")));
	Catalog->TerminalScriptPackageAuthorities[0].OwnerId = TEXT("UnrealEngine");
	Catalog->TargetBuildIdentitySha256.Reset();
	TestTrue(TEXT("catalog target build identity is mandatory -> GAC036"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC036")));
	Catalog->TargetBuildIdentitySha256 = GACRuntimeIdentitySha256(MakeObservedRuntimeIdentity());
	Catalog->Entries[0].DirectPackageDependencies = { TEXT("/Game/Shared/Opaque") };
	TestTrue(TEXT("arbitrary game package requires a recursive record -> GAC033"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC033")));
	Catalog->Entries[0].DirectPackageDependencies.Reset();

	Catalog->Entries.Reset();
	TestTrue(TEXT("empty catalog -> GAC019"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC019")));

	Catalog->Entries.Add(MakeValidMeshEntry());
	FGloamsteadGeneratedAssetEntry DuplicateAsset = MakeValidMeshEntry(
		TEXT("sanctuary.heart.copy"), EGloamsteadGeneratedAssetState::Restored);
	Catalog->Entries.Add(DuplicateAsset);
	TestTrue(TEXT("duplicate asset path -> GAC022"), GACValidateCatalog(*Catalog).Contains(TEXT("GAC022")));
	Catalog->Entries.SetNum(1);
	Catalog->Entries[0].Dependencies.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/SM_Missing.SM_Missing"))));
	TestTrue(TEXT("dependency missing from catalog -> GAC021"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC021")));
	const TSoftObjectPtr<UObject> DuplicateDependency = Catalog->Entries[0].Dependencies[0];
	Catalog->Entries[0].Dependencies.Add(DuplicateDependency);
	TestTrue(TEXT("duplicate dependency -> GAC020"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC020")));

	Catalog->Entries.Reset();
	FGloamsteadGeneratedAssetEntry Heart = MakeValidMeshEntry();
	FGloamsteadGeneratedAssetEntry Material = MakeValidMeshEntry(
		TEXT("sanctuary.heart.material"), EGloamsteadGeneratedAssetState::Restored);
	Material.Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/M_Heart.M_Heart")));
	Material.ExpectedClass = UMaterialInterface::StaticClass();
	Heart.Dependencies.Add(Material.Asset);
	Heart.DirectPackageDependencies.Add(Material.Asset.ToSoftObjectPath().GetLongPackageName());
	Catalog->Entries = { Heart, Material };
	TestEqual(TEXT("unique same-catalog dependency closure is valid"), GACValidateCatalog(*Catalog).Num(), 0);

	const FGloamsteadGeneratedAssetObservedProvenance Matching{
		Heart.ObjectSha256, Catalog->ReceiptSha256, Catalog->BundleId };
	TestEqual(TEXT("matching registry provenance is accepted"),
		GACValidateObservedProvenance(Heart, *Catalog, Matching).Num(), 0);
	FGloamsteadGeneratedAssetObservedProvenance Stale = Matching;
	Stale.ObjectSha256 = TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
	TestTrue(TEXT("stale object digest -> GAC024"),
		GACValidateObservedProvenance(Heart, *Catalog, Stale).Contains(TEXT("GAC024")));
	TestTrue(TEXT("missing registry provenance -> GAC023"),
		GACValidateObservedProvenance(Heart, *Catalog, {}).Contains(TEXT("GAC023")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamGeneratedAssetRuntimeIdentityContractTest,
	"Gloamstead.GeneratedAssets.RuntimeIdentityContractIsCanonicalAndIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamGeneratedAssetRuntimeIdentityContractTest::RunTest(const FString& /*Parameters*/)
{
	const FGloamsteadGeneratedAssetRuntimeIdentity Observed = MakeObservedRuntimeIdentity();
	FString Canonical;
	TArray<FString> Failures;
	TestTrue(TEXT("complete independently observed identity canonicalizes"),
		GACCanonicalRuntimeIdentity(Observed, Canonical, Failures));
	const FString ExpectedCanonical =
		TEXT("gloamstead.worldforge.runtime-identity@1\n")
		TEXT("engine_version=5.8.0-55116800+++UE5+Release-5.8\n")
		TEXT("compatible_engine_version=5.8.0-55116800+++UE5+Release-5.8\n")
		TEXT("engine_build_version=++UE5+Release-5.8-CL-55116800\n")
		TEXT("engine_changelist=55116800\n")
		TEXT("compatible_engine_changelist=55116800\n")
		TEXT("gloamstead_commit=0123456789abcdef0123456789abcdef01234567\n")
		TEXT("plugin_version=0.2.0\n")
		TEXT("plugin_engine_version=5.8.0\n")
		TEXT("plugin_descriptor_sha256=1111111111111111111111111111111111111111111111111111111111111111\n")
		TEXT("installed_plugin_tree_sha256=2222222222222222222222222222222222222222222222222222222222222222\n")
		TEXT("vendor_lock_sha256=3333333333333333333333333333333333333333333333333333333333333333\n")
		TEXT("declared_plugin_package_sha256=4444444444444444444444444444444444444444444444444444444444444444\n")
		TEXT("declared_plugin_build_identity=wfplugin-5555555555555555555555555555555555555555555555555555555555555555\n")
		TEXT("enabled_plugin_inventory_sha256=cede1b002e019541ca5e9ae8eccb2e7f371826d7a33035146c3a624a1302c473\n")
		TEXT("terminal_script_authority=/Script/Engine|engine|UnrealEngine|9de66287bce661dabaaf4151c4986917a1afd5fb6b1af9a6c52d36e228e80660\n")
		TEXT("terminal_script_authority=/Script/Gloamstead|gloamstead_project|Gloamstead|0c437fb5f99bce6945dbc04fa1a0b0efa3905594dd792169653f1d0256a61bc7\n")
		TEXT("terminal_script_authority=/Script/WorldForgeCore|worldforge_plugin|WorldForge|6c89b010091aedeabe3647190d30bbd12c9d080fc3c2283653afe41390c31871\n");
	TestEqual(TEXT("canonical bytes contract is exact and ordered"), Canonical, ExpectedCanonical);
	TestEqual(TEXT("canonical identity matches independent Python SHA-256 vector"),
		GACRuntimeIdentitySha256(Observed),
		FString(TEXT("39cb37afe614bba860bc464318f9458a953c09a5412daf4ae61838e338122d87")));
	FGloamsteadGeneratedAssetRuntimeIdentity Reordered = Observed;
	Swap(Reordered.EnabledPlugins[0], Reordered.EnabledPlugins[1]);
	TestEqual(TEXT("enabled-plugin record order does not alter canonical identity"),
		GACRuntimeIdentitySha256(Reordered), GACRuntimeIdentitySha256(Observed));

	FGloamsteadGeneratedAssetRuntimeIdentity NoCommit = Observed;
	NoCommit.GloamsteadCommit = TEXT("unavailable");
	TestFalse(TEXT("packaged identity without an embedded Gloamstead commit fails closed"),
		GACCanonicalRuntimeIdentity(NoCommit, Canonical, Failures));
	TestTrue(TEXT("missing embedded commit -> GAC037"), Failures.Contains(TEXT("GAC037")));
	FGloamsteadGeneratedAssetRuntimeIdentity RawBuildHash = Observed;
	RawBuildHash.DeclaredPluginBuildIdentity =
		TEXT("5555555555555555555555555555555555555555555555555555555555555555");
	TestFalse(TEXT("raw hash cannot masquerade as WorldForge release build identity"),
		GACCanonicalRuntimeIdentity(RawBuildHash, Canonical, Failures));

	FGloamsteadGeneratedAssetRuntimeIdentity ForgedInventory = Observed;
	ForgedInventory.EnabledPlugins[0].ScriptPackages.Add(TEXT("/Script/NeoStackAI"));
	TestFalse(TEXT("plugin/module edits without an independently matching inventory digest fail closed"),
		GACCanonicalRuntimeIdentity(ForgedInventory, Canonical, Failures));
	TestTrue(TEXT("forged inventory -> GAC037"), Failures.Contains(TEXT("GAC037")));
	FGloamsteadGeneratedAssetRuntimeIdentity ContentOnlyDrift = Observed;
	ContentOnlyDrift.EnabledPlugins[1].InstalledPluginTreeSha256 =
		TEXT("8888888888888888888888888888888888888888888888888888888888888888");
	TestFalse(TEXT("content-only plugin tree drift is bound even without script packages"),
		GACCanonicalRuntimeIdentity(ContentOnlyDrift, Canonical, Failures));
	TestTrue(TEXT("content-only plugin drift -> GAC037"), Failures.Contains(TEXT("GAC037")));

	FGloamsteadGeneratedAssetRuntimeIdentity ExternalPluginIdentity = Observed;
	FGloamsteadGeneratedEnabledPluginIdentity NeoStack;
	NeoStack.PluginName = TEXT("NeoStackAI");
	NeoStack.PluginVersion = TEXT("3.0.0");
	NeoStack.DescriptorSha256 =
		TEXT("9999999999999999999999999999999999999999999999999999999999999999");
	NeoStack.InstalledPluginTreeSha256 =
		TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	NeoStack.BuildIdentity = TEXT("neostack-ue5.8-3.0.0");
	NeoStack.ScriptPackages = { TEXT("/Script/NeoStackAI") };
	ExternalPluginIdentity.EnabledPlugins.Add(NeoStack);
	ExternalPluginIdentity.EnabledPluginInventorySha256 =
		GACEnabledPluginInventorySha256(ExternalPluginIdentity.EnabledPlugins);
	UGloamsteadGeneratedAssetCatalog* ExternalCatalog = MakeValidCatalog();
	ExternalCatalog->TargetBuildIdentitySha256 = GACRuntimeIdentitySha256(ExternalPluginIdentity);
	GACDeriveTerminalScriptPackageAuthorities(ExternalPluginIdentity,
		ExternalCatalog->TerminalScriptPackageAuthorities, Failures);
	TestEqual(TEXT("an external plugin module succeeds only as part of the complete exact plugin inventory"),
		GACValidateActiveBinding(*ExternalCatalog, ExternalCatalog->BundleId,
			ExternalCatalog->ReceiptSha256, ExternalCatalog->TargetBuildIdentitySha256,
			ExternalPluginIdentity).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamGeneratedAssetSettingsDefaultTest,
	"Gloamstead.GeneratedAssets.ConfigAbsenceDefaultsToGeneratedMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamGeneratedAssetSettingsDefaultTest::RunTest(const FString& /*Parameters*/)
{
	TestTrue(TEXT("config absence cannot implicitly select primitive fallback"),
		UGloamsteadGeneratedAssetSettings::ProviderModeWhenConfigAbsent()
			== EGloamsteadMeshForgeProviderMode::GeneratedCatalog);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamGeneratedAssetProviderFailClosedTest,
	"Gloamstead.GeneratedAssets.ProviderNeverFallsBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamGeneratedAssetProviderFailClosedTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadGeneratedAssetMeshForgeProvider* Provider =
		NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	UGloamsteadGeneratedAssetCatalog* Catalog = MakeValidCatalog();
	Provider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
		MakeObservedRuntimeIdentity());
	TestTrue(TEXT("valid catalog reaches deterministic ready state"), Provider->IsReadyForBuild());
	TestFalse(TEXT("valid catalog is not failed"), Provider->HasFailed());
	TestTrue(TEXT("provider remains generated-owned"),
		Provider->GetDescriptor().ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset);

	FGloamsteadMeshForgeProxySpec Spec;
	Spec.ProxyId = TEXT("heart");
	Spec.GeneratedAssetRole = TEXT("sanctuary.heart");
	Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Before;
	FGloamsteadMeshForgeSourceBinding Binding;
	Binding.SourceSystem = EGMFSourceSystem::VeilHeart;
	Binding.bLocationResolved = true;

	FGloamsteadMeshForgeProxyInstance Missing = Provider->CreateProxy(Spec, Binding, nullptr);
	TestTrue(TEXT("missing exact role/state -> GAC016"), Missing.FailureCodes.Contains(TEXT("GAC016")));
	TestFalse(TEXT("missing entry never spawns"), Missing.bSpawned);

	Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Restored;
	FGloamsteadMeshForgeProxyInstance LoadFailure = Provider->CreateProxy(Spec, Binding, nullptr);
	TestTrue(TEXT("unresolved world fails loudly -> GAC025"), LoadFailure.FailureCodes.Contains(TEXT("GAC025")));
	TestFalse(TEXT("load failure never spawns"), LoadFailure.bSpawned);

	Provider->Test_SetLoadedCatalog(Catalog, TEXT("sanctuary-v2"), Catalog->ReceiptSha256,
		MakeObservedRuntimeIdentity());
	TestTrue(TEXT("stale expected bundle fails provider"), Provider->HasFailed());
	TestTrue(TEXT("stale bundle is reported"), Provider->GetFailureCodes().Contains(TEXT("GAC014")));
	TestTrue(TEXT("failed generated provider never changes type"),
		Provider->GetDescriptor().ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset);

	UGloamsteadGeneratedAssetCatalog* CoordinatedCatalog = MakeValidCatalog();
	const FString CoordinatedExpected =
		TEXT("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
	CoordinatedCatalog->TargetBuildIdentitySha256 = CoordinatedExpected;
	TestTrue(TEXT("coordinated catalog/settings edits cannot bless different observed runtime bytes"),
		GACValidateActiveBinding(*CoordinatedCatalog, CoordinatedCatalog->BundleId,
			CoordinatedCatalog->ReceiptSha256, CoordinatedExpected, MakeObservedRuntimeIdentity())
			.Contains(TEXT("GAC036")));

	UGloamsteadGeneratedAssetSettings* MissingSettings = NewObject<UGloamsteadGeneratedAssetSettings>();
	MissingSettings->ProviderMode = EGloamsteadMeshForgeProviderMode::GeneratedCatalog;
	MissingSettings->Catalog.Reset();
	MissingSettings->ExpectedActiveBundleId = TEXT("sanctuary-v1");
	MissingSettings->ExpectedReceiptSha256 = Catalog->ReceiptSha256;
	Provider->Configure(*MissingSettings);
	Provider->PreloadCatalogAsync();
	TestTrue(TEXT("missing generated catalog fails loudly"), Provider->HasFailed());
	TestTrue(TEXT("missing catalog reports load failure"), Provider->GetFailureCodes().Contains(TEXT("GAC017")));
	TestTrue(TEXT("missing catalog never activates primitive fallback"),
		Provider->GetDescriptor().ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset);

	UGloamsteadGeneratedAssetMeshForgeProvider* UnverifiedRuntimeProvider =
		NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	UGloamsteadGeneratedAssetSettings* UnverifiedRuntimeSettings =
		NewObject<UGloamsteadGeneratedAssetSettings>();
	UnverifiedRuntimeSettings->Catalog = TSoftObjectPtr<UGloamsteadGeneratedAssetCatalog>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/DA_Catalog.DA_Catalog")));
	UnverifiedRuntimeSettings->ExpectedActiveBundleId = Catalog->BundleId;
	UnverifiedRuntimeSettings->ExpectedReceiptSha256 = Catalog->ReceiptSha256;
	UnverifiedRuntimeSettings->ExpectedTargetBuildIdentitySha256 = Catalog->TargetBuildIdentitySha256;
	UnverifiedRuntimeProvider->Configure(*UnverifiedRuntimeSettings);
	const uint64 UnverifiedGeneration = UnverifiedRuntimeProvider->Test_BeginPendingCatalogLoad();
	UnverifiedRuntimeProvider->Test_CompleteCatalogLoad(
		UnverifiedGeneration, UnverifiedRuntimeSettings->Catalog.ToSoftObjectPath(), Catalog);
	TestTrue(TEXT("production source without verified plugin/lock evidence fails closed -> GAC037"),
		UnverifiedRuntimeProvider->GetFailureCodes().Contains(TEXT("GAC037")));

	UGloamsteadGeneratedAssetMeshForgeProvider* LifecycleProvider =
		NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	LifecycleProvider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
		MakeObservedRuntimeIdentity());
	TestTrue(TEXT("explicit test identity can qualify only its immediate test use"),
		LifecycleProvider->IsReadyForBuild());
	LifecycleProvider->Configure(*UnverifiedRuntimeSettings);
	const uint64 ReconfiguredGeneration = LifecycleProvider->Test_BeginPendingCatalogLoad();
	LifecycleProvider->Test_CompleteCatalogLoad(
		ReconfiguredGeneration, UnverifiedRuntimeSettings->Catalog.ToSoftObjectPath(), Catalog);
	TestTrue(TEXT("Configure discards a test identity and restores production fail-closed observation -> GAC037"),
		LifecycleProvider->GetFailureCodes().Contains(TEXT("GAC037")));
	TestFalse(TEXT("revalidation cannot recover the discarded test identity"),
		LifecycleProvider->RevalidateRuntimeIdentity());
	TestTrue(TEXT("revalidation remains fail-closed after Configure -> GAC037"),
		LifecycleProvider->GetFailureCodes().Contains(TEXT("GAC037")));

	LifecycleProvider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
		MakeObservedRuntimeIdentity());
	const uint64 PreDeactivateGeneration = LifecycleProvider->Test_BeginPendingCatalogLoad();
	const FSoftObjectPath PreDeactivatePath = UnverifiedRuntimeSettings->Catalog.ToSoftObjectPath();
	LifecycleProvider->Deactivate();
	bool bDeactivatedCompletionRan = false;
	LifecycleProvider->Test_CompleteCatalogLoad(
		PreDeactivateGeneration, PreDeactivatePath, Catalog,
		FSimpleDelegate::CreateLambda(
			[&bDeactivatedCompletionRan]() { bDeactivatedCompletionRan = true; }));
	TestFalse(TEXT("a completion from before Deactivate is ignored"), bDeactivatedCompletionRan);
	TestNull(TEXT("a completion from before Deactivate cannot restore the catalog"),
		LifecycleProvider->GetCatalog());
	const uint64 PostDeactivateGeneration = LifecycleProvider->Test_BeginPendingCatalogLoad();
	LifecycleProvider->Test_CompleteCatalogLoad(
		PostDeactivateGeneration, PreDeactivatePath, Catalog);
	TestTrue(TEXT("Deactivate itself discards a test identity -> GAC037"),
		LifecycleProvider->GetFailureCodes().Contains(TEXT("GAC037")));

	LifecycleProvider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
		MakeObservedRuntimeIdentity());
	LifecycleProvider->Deactivate();
	LifecycleProvider->Configure(*UnverifiedRuntimeSettings);
	const uint64 ReconfiguredAfterDeactivateGeneration = LifecycleProvider->Test_BeginPendingCatalogLoad();
	LifecycleProvider->Test_CompleteCatalogLoad(
		ReconfiguredAfterDeactivateGeneration, PreDeactivatePath, Catalog);
	TestTrue(TEXT("Deactivate followed by Configure cannot retain a test identity -> GAC037"),
		LifecycleProvider->GetFailureCodes().Contains(TEXT("GAC037")));

	UGloamsteadGeneratedAssetSettings* FirstSettings = NewObject<UGloamsteadGeneratedAssetSettings>();
	FirstSettings->Catalog = TSoftObjectPtr<UGloamsteadGeneratedAssetCatalog>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/DA_First.DA_First")));
	FirstSettings->ExpectedActiveBundleId = Catalog->BundleId;
	FirstSettings->ExpectedReceiptSha256 = Catalog->ReceiptSha256;
	Provider->Configure(*FirstSettings);
	const uint64 StaleGeneration = Provider->Test_BeginPendingCatalogLoad();
	const FSoftObjectPath StalePath = FirstSettings->Catalog.ToSoftObjectPath();

	UGloamsteadGeneratedAssetSettings* SecondSettings = NewObject<UGloamsteadGeneratedAssetSettings>();
	SecondSettings->Catalog = TSoftObjectPtr<UGloamsteadGeneratedAssetCatalog>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v2/DA_Second.DA_Second")));
	SecondSettings->ExpectedActiveBundleId = TEXT("sanctuary-v2");
	SecondSettings->ExpectedReceiptSha256 = Catalog->ReceiptSha256;
	Provider->Configure(*SecondSettings);
	bool bStaleCompletionRan = false;
	Provider->Test_CompleteCatalogLoad(StaleGeneration, StalePath, Catalog,
		FSimpleDelegate::CreateLambda([&bStaleCompletionRan]() { bStaleCompletionRan = true; }));
	TestFalse(TEXT("stale completion delegate is ignored"), bStaleCompletionRan);
	TestNull(TEXT("stale completion cannot install the prior catalog"), Provider->GetCatalog());
	TestTrue(TEXT("reconfigured provider remains uninitialized"),
		Provider->GetState() == EGMFGeneratedProviderState::Uninitialized);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamGeneratedAssetProviderProvenanceTest,
	"Gloamstead.GeneratedAssets.ProviderRequiresRegistryProvenanceBeforeSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamGeneratedAssetProviderProvenanceTest::RunTest(const FString& /*Parameters*/)
{
	UGloamsteadGeneratedAssetCatalog* Catalog = MakeValidCatalog();
	UGloamsteadGeneratedAssetMeshForgeProvider* Provider =
		NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	Provider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
		MakeObservedRuntimeIdentity());
	const FSoftObjectPath ObjectPath = Catalog->Entries[0].Asset.ToSoftObjectPath();
	Provider->Test_SetPackageDependencies(ObjectPath, {});
	UStaticMesh* TestMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Provider->Test_SetResolvedObject(ObjectPath, TestMesh);

	FGloamsteadGeneratedAssetObservedProvenance Observed{
		TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"),
		Catalog->ReceiptSha256,
		Catalog->BundleId };
	Provider->Test_SetObservedProvenance(ObjectPath, Observed);

	FGloamsteadMeshForgeProxySpec Spec;
	Spec.ProxyId = TEXT("heart");
	Spec.GeneratedAssetRole = TEXT("sanctuary.heart");
	Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Restored;
	FGloamsteadMeshForgeSourceBinding Binding;
	Binding.SourceSystem = EGMFSourceSystem::VeilHeart;
	Binding.bLocationResolved = true;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world created"), World) || !TestNotNull(TEXT("test mesh loaded"), TestMesh))
	{
		if (World) { World->DestroyWorld(false); }
		return false;
	}

	const FGloamsteadMeshForgeProxyInstance Stale = Provider->CreateProxy(Spec, Binding, World);
	TestTrue(TEXT("stale registry digest fails before spawn -> GAC024"),
		Stale.FailureCodes.Contains(TEXT("GAC024")));
	TestFalse(TEXT("stale registry digest never spawns"), Stale.bSpawned);

	Observed.ObjectSha256 = Catalog->Entries[0].ObjectSha256;
	Provider->Test_SetObservedProvenance(ObjectPath, Observed);
	Provider->Test_ForceSpawnFailure(true);
	const FGloamsteadMeshForgeProxyInstance SpawnFailure = Provider->CreateProxy(Spec, Binding, World);
	TestTrue(TEXT("actor spawn failure is explicit -> GAC026"),
		SpawnFailure.FailureCodes.Contains(TEXT("GAC026")));
	TestFalse(TEXT("failed actor spawn is not visibility"), SpawnFailure.bVisibleProxyCreated);

	Binding.bLocationResolved = false;
	const FGloamsteadMeshForgeProxyInstance MissingLocation = Provider->CreateProxy(Spec, Binding, World);
	TestTrue(TEXT("unresolved location is explicit -> GAC025"),
		MissingLocation.FailureCodes.Contains(TEXT("GAC025")));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamGeneratedAssetProviderImmutableCatalogTest,
	"Gloamstead.GeneratedAssets.ProviderRejectsPostAcceptanceCatalogMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamGeneratedAssetProviderImmutableCatalogTest::RunTest(const FString& /*Parameters*/)
{
	auto MakeContractCatalog = []()
	{
		UGloamsteadGeneratedAssetCatalog* Catalog = MakeValidCatalog();
		Catalog->TerminalPlatformPackages = {
			TEXT("/Engine/EngineMaterials/DefaultMaterial") };
		FGloamsteadGeneratedExternalPackageRecord External;
		External.PackageName = TEXT("/Game/Shared/M_Master");
		External.ProvenanceObject = TSoftObjectPtr<UObject>(FSoftObjectPath(
			TEXT("/Game/Shared/M_Master.M_Master")));
		External.PackageSha256 =
			TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
		External.ReceiptSha256 = Catalog->ReceiptSha256;
		External.BundleId = Catalog->BundleId;
		Catalog->ExternalPackageRecords.Add(External);
		return Catalog;
	};

	UGloamsteadGeneratedAssetCatalog* OrderedCatalog = MakeContractCatalog();
	OrderedCatalog->TerminalPlatformPackages.Add(
		TEXT("/Engine/EngineMaterials/WorldGridMaterial"));
	OrderedCatalog->Entries[0].DirectPackageDependencies = {
		TEXT("/Script/Engine"), TEXT("/Script/Gloamstead") };
	OrderedCatalog->ExternalPackageRecords[0].DirectPackageDependencies = {
		TEXT("/Script/Engine"), TEXT("/Script/Gloamstead") };
	UGloamsteadGeneratedAssetCatalog* ReorderedCatalog =
		DuplicateObject<UGloamsteadGeneratedAssetCatalog>(OrderedCatalog, GetTransientPackage());
	Algo::Reverse(ReorderedCatalog->TerminalPlatformPackages);
	Algo::Reverse(ReorderedCatalog->TerminalScriptPackageAuthorities);
	Algo::Reverse(ReorderedCatalog->Entries[0].DirectPackageDependencies);
	Algo::Reverse(ReorderedCatalog->ExternalPackageRecords[0].DirectPackageDependencies);
	TestEqual(TEXT("canonical catalog digest is independent of set-like array order and UObject address"),
		GACCatalogContractSha256(*ReorderedCatalog), GACCatalogContractSha256(*OrderedCatalog));

	enum class EMutationTrigger : uint8
	{
		Revalidate,
		CanSpawn,
		Closure,
		CreateProxy,
	};

	auto ExpectRejected = [this, &MakeContractCatalog](
		const TCHAR* Label,
		TFunction<void(UGloamsteadGeneratedAssetCatalog&)> Mutate,
		EMutationTrigger Trigger,
		const TCHAR* StructuralCode = nullptr)
	{
		UGloamsteadGeneratedAssetCatalog* Catalog = MakeContractCatalog();
		UGloamsteadGeneratedAssetMeshForgeProvider* Provider =
			NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
		Provider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
			MakeObservedRuntimeIdentity());
		TestTrue(FString::Printf(TEXT("%s: baseline ready"), Label), Provider->IsReadyForBuild());
		const FString AcceptedFingerprint = GACCatalogContractSha256(*Catalog);
		Mutate(*Catalog);
		TestNotEqual(FString::Printf(TEXT("%s: contract fingerprint changes"), Label),
			GACCatalogContractSha256(*Catalog), AcceptedFingerprint);

		switch (Trigger)
		{
		case EMutationTrigger::Revalidate:
			Provider->RevalidateRuntimeIdentity();
			break;
		case EMutationTrigger::CanSpawn:
			Provider->CanSpawn(EGMFProxyType::Heart);
			break;
		case EMutationTrigger::Closure:
			Provider->Test_ValidateDependencyClosure();
			break;
		case EMutationTrigger::CreateProxy:
		{
			FGloamsteadMeshForgeProxySpec Spec;
			Spec.GeneratedAssetRole = TEXT("sanctuary.heart");
			Spec.GeneratedAssetState = EGloamsteadGeneratedAssetState::Restored;
			FGloamsteadMeshForgeSourceBinding Binding;
			Binding.bLocationResolved = true;
			Provider->CreateProxy(Spec, Binding, nullptr);
			break;
		}
		}

		TestTrue(FString::Printf(TEXT("%s: mutation fails provider"), Label), Provider->HasFailed());
		TestTrue(FString::Printf(TEXT("%s: mutation reports GAC039"), Label),
			Provider->GetFailureCodes().Contains(TEXT("GAC039")));
		if (StructuralCode)
		{
			TestTrue(FString::Printf(TEXT("%s: structural failure retained"), Label),
				Provider->GetFailureCodes().Contains(StructuralCode));
		}
		TestNull(FString::Printf(TEXT("%s: rejected catalog is released"), Label),
			Provider->GetCatalog());
		TestFalse(FString::Printf(TEXT("%s: revalidation cannot recover without Configure"), Label),
			Provider->RevalidateRuntimeIdentity());
		TestTrue(FString::Printf(TEXT("%s: revalidation remains latched at GAC039"), Label),
			Provider->GetFailureCodes().Contains(TEXT("GAC039")));
		UGloamsteadGeneratedAssetCatalog* Replacement = MakeContractCatalog();
		Provider->Test_SetLoadedCatalog(Replacement, Replacement->BundleId,
			Replacement->ReceiptSha256, MakeObservedRuntimeIdentity());
		TestTrue(FString::Printf(TEXT("%s: direct reload cannot bypass Configure latch"), Label),
			Provider->HasFailed() && Provider->GetFailureCodes().Contains(TEXT("GAC039")));
	};

	ExpectRejected(TEXT("terminal root"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog)
		{
			Catalog.TerminalPlatformPackageRoots = { TEXT("/Game") };
		}, EMutationTrigger::Revalidate, TEXT("GAC034"));
	ExpectRejected(TEXT("exact engine terminal"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog)
		{
			Catalog.TerminalPlatformPackages.Add(TEXT("/Engine/EngineMaterials/WorldGridMaterial"));
		}, EMutationTrigger::CanSpawn);
	ExpectRejected(TEXT("script authority"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog)
		{
			Catalog.TerminalScriptPackageAuthorities[0].OwnerIdentitySha256 =
				TEXT("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
		}, EMutationTrigger::Closure);
	ExpectRejected(TEXT("entry object path"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog)
		{
			Catalog.Entries[0].Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(
				TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/SM_Other.SM_Other")));
		}, EMutationTrigger::CreateProxy);
	ExpectRejected(TEXT("entry object hash"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog)
		{
			Catalog.Entries[0].ObjectSha256 =
				TEXT("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
		}, EMutationTrigger::Revalidate);
	ExpectRejected(TEXT("entry dependency"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog)
		{
			Catalog.Entries[0].DirectPackageDependencies.Add(TEXT("/Script/Engine"));
		}, EMutationTrigger::Closure);
	ExpectRejected(TEXT("external record"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog)
		{
			Catalog.ExternalPackageRecords[0].PackageSha256 =
				TEXT("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
		}, EMutationTrigger::CreateProxy);
	ExpectRejected(TEXT("bundle identity"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog) { Catalog.BundleId = TEXT("sanctuary-v2"); },
		EMutationTrigger::Revalidate);
	ExpectRejected(TEXT("receipt identity"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog)
		{
			Catalog.ReceiptSha256 =
				TEXT("1111111111111111111111111111111111111111111111111111111111111111");
		}, EMutationTrigger::CanSpawn, TEXT("GAC015"));
	ExpectRejected(TEXT("build identity"),
		[](UGloamsteadGeneratedAssetCatalog& Catalog)
		{
			Catalog.TargetBuildIdentitySha256 =
				TEXT("2222222222222222222222222222222222222222222222222222222222222222");
		}, EMutationTrigger::Closure);

	UGloamsteadGeneratedAssetCatalog* StaleCatalog = MakeContractCatalog();
	UGloamsteadGeneratedAssetMeshForgeProvider* StaleProvider =
		NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	StaleProvider->Test_SetLoadedCatalog(StaleCatalog, StaleCatalog->BundleId,
		StaleCatalog->ReceiptSha256, MakeObservedRuntimeIdentity());
	const uint64 StaleGeneration = StaleProvider->Test_BeginPendingCatalogLoad();
	StaleCatalog->Entries[0].LicenseId = TEXT("LicenseRef-002");
	TestFalse(TEXT("mutation while reload is pending is detected before build"),
		StaleProvider->CanSpawn(EGMFProxyType::Heart));
	bool bStaleCompletionRan = false;
	StaleProvider->Test_CompleteCatalogLoad(StaleGeneration, FSoftObjectPath(), StaleCatalog,
		FSimpleDelegate::CreateLambda([&bStaleCompletionRan]() { bStaleCompletionRan = true; }));
	TestFalse(TEXT("callback invalidated by catalog mutation is ignored"), bStaleCompletionRan);
	TestTrue(TEXT("stale callback cannot recover failed provider"), StaleProvider->HasFailed());
	TestNull(TEXT("stale callback cannot reinstall catalog"), StaleProvider->GetCatalog());

	UGloamsteadGeneratedAssetCatalog* FreshCatalog = MakeContractCatalog();
	StaleProvider->Deactivate();
	StaleProvider->Test_SetLoadedCatalog(FreshCatalog, FreshCatalog->BundleId,
		FreshCatalog->ReceiptSha256, MakeObservedRuntimeIdentity());
	TestTrue(TEXT("Deactivate cannot clear a mutation latch without Configure"),
		StaleProvider->HasFailed() && StaleProvider->GetFailureCodes().Contains(TEXT("GAC039")));
	UGloamsteadGeneratedAssetSettings* FreshSettings = NewObject<UGloamsteadGeneratedAssetSettings>();
	FreshSettings->Catalog = TSoftObjectPtr<UGloamsteadGeneratedAssetCatalog>(FSoftObjectPath(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/DA_Catalog.DA_Catalog")));
	FreshSettings->ExpectedActiveBundleId = FreshCatalog->BundleId;
	FreshSettings->ExpectedReceiptSha256 = FreshCatalog->ReceiptSha256;
	FreshSettings->ExpectedTargetBuildIdentitySha256 = FreshCatalog->TargetBuildIdentitySha256;
	StaleProvider->Configure(*FreshSettings);
	StaleProvider->Test_SetObservedRuntimeIdentity(MakeObservedRuntimeIdentity());
	const uint64 FreshGeneration = StaleProvider->Test_BeginPendingCatalogLoad();
	StaleProvider->Test_CompleteCatalogLoad(FreshGeneration, FreshSettings->Catalog.ToSoftObjectPath(),
		FreshCatalog);
	TestTrue(TEXT("explicit Configure plus fresh async load can accept a fresh immutable catalog"),
		StaleProvider->IsReadyForBuild());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamGeneratedAssetDependencyClosureTest,
	"Gloamstead.GeneratedAssets.AssetRegistryDependencyClosureIsExact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamGeneratedAssetDependencyClosureTest::RunTest(const FString& /*Parameters*/)
{
	auto MakeClosedCatalog = []()
	{
		UGloamsteadGeneratedAssetCatalog* Catalog = MakeValidCatalog();
		FGloamsteadGeneratedAssetEntry Material = MakeValidMeshEntry(
			TEXT("sanctuary.heart.material"), EGloamsteadGeneratedAssetState::Restored);
		Material.Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(
			TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/M_Heart.M_Heart")));
		Material.ExpectedClass = UMaterialInterface::StaticClass();
		Material.ObjectSha256 = TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
		FGloamsteadGeneratedAssetEntry Texture = MakeValidMeshEntry(
			TEXT("sanctuary.heart.texture"), EGloamsteadGeneratedAssetState::Restored);
		Texture.Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(
			TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/T_Heart.T_Heart")));
		Texture.ExpectedClass = UTexture2D::StaticClass();
		Texture.ObjectSha256 = TEXT("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
		Catalog->Entries[0].Dependencies.Add(Material.Asset);
		Catalog->Entries[0].DirectPackageDependencies = {
			TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/M_Heart"),
			TEXT("/Game/Shared/M_Master"),
			TEXT("/Engine/EngineMaterials/DefaultMaterial") };
		Material.Dependencies.Add(Texture.Asset);
		Material.DirectPackageDependencies = {
			TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/T_Heart"),
			TEXT("/Script/Engine") };
		Catalog->Entries.Add(Material);
		Catalog->Entries.Add(Texture);
		Catalog->TerminalPlatformPackages = { TEXT("/Engine/EngineMaterials/DefaultMaterial") };

		FGloamsteadGeneratedExternalPackageRecord SharedMaterial;
		SharedMaterial.PackageName = TEXT("/Game/Shared/M_Master");
		SharedMaterial.ProvenanceObject = TSoftObjectPtr<UObject>(FSoftObjectPath(
			TEXT("/Game/Shared/M_Master.M_Master")));
		SharedMaterial.PackageSha256 = TEXT("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
		SharedMaterial.ReceiptSha256 = Catalog->ReceiptSha256;
		SharedMaterial.BundleId = Catalog->BundleId;
		SharedMaterial.DirectPackageDependencies = {
			TEXT("/Game/Shared/T_Shared"), TEXT("/Script/Engine") };

		FGloamsteadGeneratedExternalPackageRecord SharedTexture;
		SharedTexture.PackageName = TEXT("/Game/Shared/T_Shared");
		SharedTexture.ProvenanceObject = TSoftObjectPtr<UObject>(FSoftObjectPath(
			TEXT("/Game/Shared/T_Shared.T_Shared")));
		SharedTexture.PackageSha256 = TEXT("1111111111111111111111111111111111111111111111111111111111111111");
		SharedTexture.ReceiptSha256 = Catalog->ReceiptSha256;
		SharedTexture.BundleId = Catalog->BundleId;
		Catalog->ExternalPackageRecords = { SharedMaterial, SharedTexture };
		return Catalog;
	};

	auto ConfigureEvidence = [](UGloamsteadGeneratedAssetMeshForgeProvider* Provider,
		UGloamsteadGeneratedAssetCatalog* Catalog)
	{
		for (const FGloamsteadGeneratedAssetEntry& Entry : Catalog->Entries)
		{
			Provider->Test_SetObservedProvenance(Entry.Asset.ToSoftObjectPath(), {
				Entry.ObjectSha256, Catalog->ReceiptSha256, Catalog->BundleId });
		}
		for (const FGloamsteadGeneratedExternalPackageRecord& Record : Catalog->ExternalPackageRecords)
		{
			Provider->Test_SetObservedProvenance(Record.ProvenanceObject.ToSoftObjectPath(), {
				FString(), Catalog->ReceiptSha256, Catalog->BundleId, Record.PackageSha256 });
		}
	};

	auto ConfigureGraph = [&](UGloamsteadGeneratedAssetMeshForgeProvider* Provider,
		UGloamsteadGeneratedAssetCatalog* Catalog)
	{
		Provider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
			MakeObservedRuntimeIdentity());
		ConfigureEvidence(Provider, Catalog);
		Provider->Test_SetPackageDependencies(Catalog->Entries[0].Asset.ToSoftObjectPath(), {
			FName(TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/M_Heart")),
			FName(TEXT("/Game/Shared/M_Master")),
			FName(TEXT("/Engine/EngineMaterials/DefaultMaterial")) });
		Provider->Test_SetPackageDependencies(Catalog->Entries[1].Asset.ToSoftObjectPath(), {
			FName(TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/T_Heart")),
			FName(TEXT("/Script/Engine")) });
		Provider->Test_SetPackageDependencies(Catalog->Entries[2].Asset.ToSoftObjectPath(), {});
		Provider->Test_SetPackageDependencies(
			Catalog->ExternalPackageRecords[0].ProvenanceObject.ToSoftObjectPath(), {
				FName(TEXT("/Game/Shared/T_Shared")), FName(TEXT("/Script/Engine")) });
		Provider->Test_SetPackageDependencies(
			Catalog->ExternalPackageRecords[1].ProvenanceObject.ToSoftObjectPath(), {});
	};

	UGloamsteadGeneratedAssetCatalog* Catalog = MakeClosedCatalog();
	UGloamsteadGeneratedAssetMeshForgeProvider* Provider =
		NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	ConfigureGraph(Provider, Catalog);
	const TArray<FString> BaselineClosureCodes = Provider->Test_ValidateDependencyClosure();
	if (BaselineClosureCodes.Num() > 0)
	{
		AddInfo(FString::Printf(TEXT("baseline closure codes: %s"),
			*FString::Join(BaselineClosureCodes, TEXT(","))));
	}
	TestEqual(TEXT("full generated, shared external, and platform-terminal closure passes"),
		BaselineClosureCodes.Num(), 0);
	Provider->Test_SetObservedProvenance(
		Catalog->ExternalPackageRecords[1].ProvenanceObject.ToSoftObjectPath(), {
			FString(), Catalog->ReceiptSha256, Catalog->BundleId,
			TEXT("2222222222222222222222222222222222222222222222222222222222222222") });
	TestTrue(TEXT("stale external package digest -> GAC035"),
		Provider->Test_ValidateDependencyClosure().Contains(TEXT("GAC035")));
	ConfigureEvidence(Provider, Catalog);

	Provider->Test_SetPackageDependencies(
		Catalog->ExternalPackageRecords[0].ProvenanceObject.ToSoftObjectPath(), {
			FName(TEXT("/Game/Shared/T_Shared")), FName(TEXT("/Script/Engine")),
			FName(TEXT("/Game/Shared/T_Drift")) });
	const TArray<FString> ExternalDriftCodes = Provider->Test_ValidateDependencyClosure();
	TestTrue(TEXT("external direct edge drift -> GAC030"), ExternalDriftCodes.Contains(TEXT("GAC030")));
	TestTrue(TEXT("undeclared external transitive -> GAC033"), ExternalDriftCodes.Contains(TEXT("GAC033")));

	Provider = NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	ConfigureGraph(Provider, Catalog);
	Catalog->ExternalPackageRecords[0].DirectPackageDependencies.Add(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v0/SM_Prior"));
	Provider->Test_SetPackageDependencies(
		Catalog->ExternalPackageRecords[0].ProvenanceObject.ToSoftObjectPath(), {
			FName(TEXT("/Game/Shared/T_Shared")), FName(TEXT("/Script/Engine")),
			FName(TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v0/SM_Prior")) });
	TestTrue(TEXT("declared shared package cannot reenter a prior generated version -> GAC028"),
		Provider->Test_ValidateDependencyClosure().Contains(TEXT("GAC028")));
	Catalog->ExternalPackageRecords[0].DirectPackageDependencies.Pop();

	Provider = NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	ConfigureGraph(Provider, Catalog);
	Catalog->ExternalPackageRecords[0].DirectPackageDependencies.Add(
		TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/SM_Undeclared"));
	Provider->Test_SetPackageDependencies(
		Catalog->ExternalPackageRecords[0].ProvenanceObject.ToSoftObjectPath(), {
			FName(TEXT("/Game/Shared/T_Shared")), FName(TEXT("/Script/Engine")),
			FName(TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/SM_Undeclared")) });
	TestTrue(TEXT("post-acceptance current-version edge mutation invalidates the contract -> GAC039"),
		Provider->Test_ValidateDependencyClosure().Contains(TEXT("GAC039")));
	Catalog->ExternalPackageRecords[0].DirectPackageDependencies.Pop();

	Provider = NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	ConfigureGraph(Provider, Catalog);
	Provider->Test_SetPackageDependencies(Catalog->Entries[0].Asset.ToSoftObjectPath(), {
		FName(TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/M_Heart")),
		FName(TEXT("/Game/Shared/M_Master")),
		FName(TEXT("/Engine/EngineMaterials/WorldGridMaterial")) });
	const TArray<FString> TerminalDriftCodes = Provider->Test_ValidateDependencyClosure();
	TestTrue(TEXT("terminal package substitution is an observed omission -> GAC030"),
		TerminalDriftCodes.Contains(TEXT("GAC030")));
	TestTrue(TEXT("the declared terminal package is unused -> GAC031"),
		TerminalDriftCodes.Contains(TEXT("GAC031")));
	TestTrue(TEXT("a non-policy platform substitution is not implicitly terminal -> GAC033"),
		TerminalDriftCodes.Contains(TEXT("GAC033")));

	Provider = NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	ConfigureGraph(Provider, Catalog);
	Provider->Test_MarkPackageDependencyQueryUnavailable(TEXT("/Game/Shared/T_Shared"));
	TestTrue(TEXT("external query failure remains fail-closed -> GAC027"),
		Provider->Test_ValidateDependencyClosure().Contains(TEXT("GAC027")));

	Provider = NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	ConfigureGraph(Provider, Catalog);
	Provider->Test_SetPackageDependencies(
		Catalog->ExternalPackageRecords[1].ProvenanceObject.ToSoftObjectPath(), {
			FName(TEXT("/Game/Shared/M_Master")) });
	TestTrue(TEXT("external package cycle -> GAC032"),
		Provider->Test_ValidateDependencyClosure().Contains(TEXT("GAC032")));

	Provider = NewObject<UGloamsteadGeneratedAssetMeshForgeProvider>();
	ConfigureGraph(Provider, Catalog);
	FGloamsteadGeneratedScriptPackageAuthority ForgedNeoStack;
	ForgedNeoStack.PackageName = TEXT("/Script/NeoStackAI");
	ForgedNeoStack.OwnerClass = EGloamsteadGeneratedScriptPackageOwner::ExternalPlugin;
	ForgedNeoStack.OwnerId = TEXT("NeoStackAI");
	ForgedNeoStack.OwnerIdentitySha256 =
		TEXT("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
	Catalog->TerminalScriptPackageAuthorities.Add(ForgedNeoStack);
	Catalog->Entries[0].DirectPackageDependencies.Add(TEXT("/Script/NeoStackAI"));
	Provider->Test_SetPackageDependencies(Catalog->Entries[0].Asset.ToSoftObjectPath(), {
		FName(TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/v1/M_Heart")),
		FName(TEXT("/Game/Shared/M_Master")),
		FName(TEXT("/Engine/EngineMaterials/DefaultMaterial")),
		FName(TEXT("/Script/NeoStackAI")) });
	TestTrue(TEXT("a post-validation free NeoStackAI authority mutates the accepted contract -> GAC039"),
		Provider->Test_ValidateDependencyClosure().Contains(TEXT("GAC039")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
