#include "Misc/AutomationTest.h"
#include "Data/GloamsteadGeneratedAssetCatalog.h"
#include "Systems/GloamsteadMeshForgeProvider.h"
#include "Settings/GloamsteadGeneratedAssetSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
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
		GACValidateActiveBinding(*Catalog, TEXT("sanctuary-v2"), Catalog->ReceiptSha256).Contains(TEXT("GAC014")));
	TestTrue(TEXT("stale receipt -> GAC015"),
		GACValidateActiveBinding(*Catalog, Catalog->BundleId,
			TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc")).Contains(TEXT("GAC015")));

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
	Catalog->Entries = { Heart, Material };
	TestEqual(TEXT("unique same-catalog dependency closure is valid"), GACValidateCatalog(*Catalog).Num(), 0);
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
	TestTrue(TEXT("unloadable soft reference -> GAC017"), LoadFailure.FailureCodes.Contains(TEXT("GAC017")));
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
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
