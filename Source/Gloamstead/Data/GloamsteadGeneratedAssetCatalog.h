#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GloamsteadGeneratedAssetCatalog.generated.h"

/** Exact caller-owned state used as part of a generated-asset catalog key. */
UENUM(BlueprintType)
enum class EGloamsteadGeneratedAssetState : uint8
{
	Unknown = 0,
	Before,
	RestorationInProgress,
	Restored,
	Corrupted,
};

GLOAMSTEAD_API FString GACStateToken(EGloamsteadGeneratedAssetState State);

/** Import-authored values read from the cooked/on-disk Asset Registry for one exact object path. */
struct FGloamsteadGeneratedAssetObservedProvenance
{
	FString ObjectSha256;
	FString ReceiptSha256;
	FString BundleId;
	FString PackageSha256;
};

/**
 * One receipt-bound, non-generated package in the dependency closure.
 * DirectPackageDependencies is the complete direct package edge set observed by the Asset Registry.
 * ProvenanceObject must live in PackageName and carry the same installer-authored evidence tags as
 * generated objects. This lets shared /Game or plugin content participate without becoming opaque.
 */
USTRUCT(BlueprintType)
struct FGloamsteadGeneratedExternalPackageRecord
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString PackageName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TSoftObjectPtr<UObject> ProvenanceObject;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString PackageSha256;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString ReceiptSha256;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString BundleId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TArray<FString> DirectPackageDependencies;
};

/** Generic Asset Registry tag names authored by the asset installer; they carry no Gloamstead semantics. */
namespace GloamsteadGeneratedAssetProvenanceTags
{
	GLOAMSTEAD_API extern const FName ObjectSha256;
	GLOAMSTEAD_API extern const FName ReceiptSha256;
	GLOAMSTEAD_API extern const FName BundleId;
	GLOAMSTEAD_API extern const FName PackageSha256;
}

/** One immutable receipt-bound generated asset selection. */
USTRUCT(BlueprintType)
struct FGloamsteadGeneratedAssetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FName SemanticRole;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") EGloamsteadGeneratedAssetState RestorationState = EGloamsteadGeneratedAssetState::Unknown;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TSoftObjectPtr<UObject> Asset;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TSoftClassPtr<UObject> ExpectedClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString ObjectSha256;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString ReceiptSha256;
	/** Complete exact direct package dependency set, including generated and external packages. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TArray<FString> DirectPackageDependencies;
	/** Generated-object mappings retained for class, load, and per-object provenance validation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TArray<TSoftObjectPtr<UObject>> Dependencies;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString OwnershipId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString LicenseId;
};

/** Gloamstead-owned immutable selection catalog for one generated Sanctuary kit/version. */
UCLASS(BlueprintType)
class GLOAMSTEAD_API UGloamsteadGeneratedAssetCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString BundleId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString ReceiptSha256;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString VersionRoot;
	/** Canonical hash of the target UE build, Gloamstead base commit, and vendored plugin lock. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString TargetBuildIdentitySha256;
	/**
	 * Optional safe terminal platform roots. Only exact /Engine and /Script roots are valid; packages below
	 * them are terminal and therefore bound by TargetBuildIdentitySha256 instead of recursively queried.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TArray<FString> TerminalPlatformPackageRoots;
	/** Exact /Engine or /Script package names treated as terminal when a broad safe root is not desired. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TArray<FString> TerminalPlatformPackages;
	/** Every non-terminal external package, including /Game shared or plugin content, is recursively declared here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TArray<FGloamsteadGeneratedExternalPackageRecord> ExternalPackageRecords;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TArray<FGloamsteadGeneratedAssetEntry> Entries;

	/** Exact role/state lookup. Invalid or duplicate catalogs never use first-match fallback. */
	const FGloamsteadGeneratedAssetEntry* FindExact(FName SemanticRole, EGloamsteadGeneratedAssetState State) const;
};

/**
 * Pure fail-closed catalog validation. Empty means valid.
 * GAC001 invalid bundle id; 002 invalid version root; 003 receipt hash; 004 role id; 005 duplicate key;
 * 006 unknown state; 007 outside generated root; 008 wrong version; 009 ownership; 010 license;
 * 011 expected class; 012 object hash; 013 provider-incompatible loaded class; 015 receipt binding;
 * 016 exact entry missing; 017 soft load failure; 018 loaded class mismatch; 019 empty catalog;
 * 020 duplicate dependency; 021 dependency closure; 022 duplicate asset path; 023 provenance absent;
 * 024 provenance mismatch; 025 unresolved spawn target; 026 actor spawn failure;
 * 027 dependency query unavailable; 028 generated dependency root escape; 029 ambiguous package mapping;
 * 030 observed direct dependency omitted; 031 declared direct dependency unused; 032 dependency cycle;
 * 033 undeclared non-terminal dependency; 034 invalid external package/terminal policy declaration;
 * 035 external package provenance binding; 036 target UE/plugin build identity binding.
 */
GLOAMSTEAD_API TArray<FString> GACValidateCatalog(
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	bool bRequireMeshForgeCompatibleClasses = false);

/** Validate the catalog against the active settings receipt/bundle expectation. */
GLOAMSTEAD_API TArray<FString> GACValidateActiveBinding(
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	const FString& ExpectedBundleId,
	const FString& ExpectedReceiptSha256,
	const FString& ExpectedTargetBuildIdentitySha256);

/** Validate a loaded object against one entry after its expected class has been resolved. */
GLOAMSTEAD_API TArray<FString> GACValidateLoadedObject(
	const FGloamsteadGeneratedAssetEntry& Entry,
	const UObject* LoadedObject,
	bool bRequireStaticMeshForProvider = false);

/** Bind the catalog declaration to independently stored Asset Registry evidence. */
GLOAMSTEAD_API TArray<FString> GACValidateObservedProvenance(
	const FGloamsteadGeneratedAssetEntry& Entry,
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	const FGloamsteadGeneratedAssetObservedProvenance& Observed);

/** Bind a recursively declared external package to independently stored Asset Registry evidence. */
GLOAMSTEAD_API TArray<FString> GACValidateObservedProvenance(
	const FGloamsteadGeneratedExternalPackageRecord& Record,
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	const FGloamsteadGeneratedAssetObservedProvenance& Observed);
