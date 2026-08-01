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

/** Authority class for an exact terminal `/Script/<Module>` package. */
UENUM(BlueprintType)
enum class EGloamsteadGeneratedScriptPackageOwner : uint8
{
	Unknown = 0,
	Engine,
	GloamsteadProject,
	WorldForgePlugin,
	ExternalPlugin,
};

/** One independently enumerated enabled plugin and all of its script-module packages. */
struct FGloamsteadGeneratedEnabledPluginIdentity
{
	FString PluginName;
	FString PluginVersion;
	FString DescriptorSha256;
	/** Digest of the exact installed plugin tree: descriptor, binaries, content, config, and sources. */
	FString InstalledPluginTreeSha256;
	FString BuildIdentity;
	TArray<FString> ScriptPackages;
};

/** Exact catalog declaration derived from a trusted runtime inventory, never from a package-name prefix. */
USTRUCT(BlueprintType)
struct FGloamsteadGeneratedScriptPackageAuthority
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString PackageName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets")
	EGloamsteadGeneratedScriptPackageOwner OwnerClass = EGloamsteadGeneratedScriptPackageOwner::Unknown;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString OwnerId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") FString OwnerIdentitySha256;
};

/**
 * Independently observed runtime axes bound into a generated bundle.
 *
 * Canonical form is UTF-8, LF-only, with the contract line followed by scalar `key=value` lines,
 * `enabled_plugin_inventory_sha256`, then sorted `terminal_script_authority` lines and one final LF.
 * The inventory digest is recomputed from plugin-name-sorted records and package-name-sorted module
 * lists. Every record binds version, descriptor digest, full installed-tree digest (binaries, content,
 * config, and source), build identity, and zero or more exact `/Script/<Module>` packages. Values are
 * restricted to printable ASCII without CR/LF or '='. The production identity source may return
 * success only after independently enumerating every enabled plugin and verifying installed bytes
 * against the committed lock/build evidence. This serializer is shared with vendor sync and WorldForgeEd.
 */
struct FGloamsteadGeneratedAssetRuntimeIdentity
{
	FString EngineVersion;
	FString CompatibleEngineVersion;
	FString EngineBuildVersion;
	uint32 EngineChangelist = 0;
	uint32 CompatibleEngineChangelist = 0;
	FString GloamsteadCommit;
	FString PluginVersion;
	FString PluginEngineVersion;
	FString PluginDescriptorSha256;
	FString InstalledPluginTreeSha256;
	FString VendorLockSha256;
	FString DeclaredPluginPackageSha256;
	FString DeclaredPluginBuildIdentity;
	/** Exact script packages compiled into the independently identified UE build. */
	TArray<FString> EngineScriptPackages;
	/** Exact script packages compiled from the independently identified Gloamstead commit/build. */
	TArray<FString> GloamsteadScriptPackages;
	/** Complete enabled-plugin inventory, including every script module exported by each plugin. */
	TArray<FGloamsteadGeneratedEnabledPluginIdentity> EnabledPlugins;
	/** Independently observed digest of the canonical complete EnabledPlugins inventory. */
	FString EnabledPluginInventorySha256;
};

/** Fixed committed lock location consumed by both the runtime and the Task 4 sync tooling. */
GLOAMSTEAD_API const FString& GACWorldForgeVendorLockRelativePath();
/** Exact canonical bytes-as-text for runtime identity contract `gloamstead.worldforge.runtime-identity@1`. */
GLOAMSTEAD_API bool GACCanonicalRuntimeIdentity(
	const FGloamsteadGeneratedAssetRuntimeIdentity& Identity,
	FString& OutCanonical,
	TArray<FString>& OutFailureCodes);
/** SHA-256 of the canonical UTF-8 identity, or empty when any axis is absent/unverified. */
GLOAMSTEAD_API FString GACRuntimeIdentitySha256(
	const FGloamsteadGeneratedAssetRuntimeIdentity& Identity,
	TArray<FString>* OutFailureCodes = nullptr);
/** Canonical digest of sorted enabled-plugin/module records, or empty for an incomplete/invalid inventory. */
GLOAMSTEAD_API FString GACEnabledPluginInventorySha256(
	const TArray<FGloamsteadGeneratedEnabledPluginIdentity>& EnabledPlugins);
/** Derive the complete exact terminal script authority set from independently verified runtime axes. */
GLOAMSTEAD_API bool GACDeriveTerminalScriptPackageAuthorities(
	const FGloamsteadGeneratedAssetRuntimeIdentity& Identity,
	TArray<FGloamsteadGeneratedScriptPackageAuthority>& OutAuthorities,
	TArray<FString>& OutFailureCodes);

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
	 * Optional safe terminal platform roots. Only exact /Engine is valid. Script modules are never
	 * authorized by a root: they require the exact independently observed authority records below.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TArray<FString> TerminalPlatformPackageRoots;
	/** Exact /Engine packages treated as terminal when a broad safe root is not desired. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets") TArray<FString> TerminalPlatformPackages;
	/** Complete exact terminal script authority set; must equal the trusted runtime observation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Generated Assets")
	TArray<FGloamsteadGeneratedScriptPackageAuthority> TerminalScriptPackageAuthorities;
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
 * 035 external package provenance binding; 036 target UE/plugin build identity mismatch;
 * 037 independently observed runtime identity unavailable/incomplete;
 * 038 terminal script authority/inventory mismatch.
 */
GLOAMSTEAD_API TArray<FString> GACValidateCatalog(
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	bool bRequireMeshForgeCompatibleClasses = false);

/** Validate the catalog against the active settings receipt/bundle expectation. */
GLOAMSTEAD_API TArray<FString> GACValidateActiveBinding(
	const UGloamsteadGeneratedAssetCatalog& Catalog,
	const FString& ExpectedBundleId,
	const FString& ExpectedReceiptSha256,
	const FString& ExpectedTargetBuildIdentitySha256,
	const FGloamsteadGeneratedAssetRuntimeIdentity& ObservedRuntimeIdentity);

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
