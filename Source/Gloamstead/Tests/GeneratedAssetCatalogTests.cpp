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
		Catalog->TargetBuildIdentitySha256 = TEXT("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
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
			Catalog->TargetBuildIdentitySha256).Contains(TEXT("GAC014")));
	TestTrue(TEXT("stale receipt -> GAC015"),
		GACValidateActiveBinding(*Catalog, Catalog->BundleId,
			TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"),
			Catalog->TargetBuildIdentitySha256).Contains(TEXT("GAC015")));
	TestTrue(TEXT("stale target build identity -> GAC036"),
		GACValidateActiveBinding(*Catalog, Catalog->BundleId, Catalog->ReceiptSha256,
			TEXT("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"))
			.Contains(TEXT("GAC036")));
	Catalog->TerminalPlatformPackageRoots = { TEXT("/Game") };
	TestTrue(TEXT("terminal policy is limited to safe platform roots -> GAC034"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC034")));
	Catalog->TerminalPlatformPackageRoots.Reset();
	Catalog->TerminalPlatformPackages = { TEXT("/Game/Shared/Opaque") };
	TestTrue(TEXT("arbitrary game package cannot be terminal -> GAC034"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC034")));
	Catalog->TerminalPlatformPackages.Reset();
	Catalog->TargetBuildIdentitySha256.Reset();
	TestTrue(TEXT("catalog target build identity is mandatory -> GAC036"),
		GACValidateCatalog(*Catalog).Contains(TEXT("GAC036")));
	Catalog->TargetBuildIdentitySha256 =
		TEXT("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
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
	Provider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256);
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

	Provider->Test_SetLoadedCatalog(Catalog, TEXT("sanctuary-v2"), Catalog->ReceiptSha256);
	TestTrue(TEXT("stale expected bundle fails provider"), Provider->HasFailed());
	TestTrue(TEXT("stale bundle is reported"), Provider->GetFailureCodes().Contains(TEXT("GAC014")));
	TestTrue(TEXT("failed generated provider never changes type"),
		Provider->GetDescriptor().ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset);

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
	Provider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256);
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
		Catalog->TerminalPlatformPackageRoots = { TEXT("/Script") };
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
		Provider->Test_SetLoadedCatalog(Catalog, Catalog->BundleId, Catalog->ReceiptSha256);
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
	TestEqual(TEXT("full generated, shared external, and platform-terminal closure passes"),
		Provider->Test_ValidateDependencyClosure().Num(), 0);
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
	TestTrue(TEXT("declared shared package cannot reenter an unmapped current-version package -> GAC029"),
		Provider->Test_ValidateDependencyClosure().Contains(TEXT("GAC029")));
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
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
