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
};

/** Generic Asset Registry tag names authored by the asset installer; they carry no Gloamstead semantics. */
namespace GloamsteadGeneratedAssetProvenanceTags
{
	GLOAMSTEAD_API extern const FName ObjectSha256;
	GLOAMSTEAD_API extern const FName ReceiptSha256;
	GLOAMSTEAD_API extern const FName BundleId;
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
 * 024 provenance mismatch; 025 unresolved spawn target; 026 actor spawn failure.
 */
GLOAMSTEAD_API TArray<FString> GACValidateCatalog(
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	bool bRequireMeshForgeCompatibleClasses = false);

/** Validate the catalog against the active settings receipt/bundle expectation. */
GLOAMSTEAD_API TArray<FString> GACValidateActiveBinding(
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	const FString& ExpectedBundleId,
	const FString& ExpectedReceiptSha256);

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
