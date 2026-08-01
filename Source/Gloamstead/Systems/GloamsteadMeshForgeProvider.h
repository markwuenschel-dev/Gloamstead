#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/GloamsteadMeshForgeTypes.h"
#include "GloamsteadMeshForgeProvider.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class UGloamsteadGeneratedAssetCatalog;
class UGloamsteadGeneratedAssetSettings;
struct FGloamsteadGeneratedAssetRuntimeIdentity;
struct FStreamableHandle;

/** Narrow runtime observation protocol. Implementations must inspect runtime state, never expected catalog data. */
class GLOAMSTEAD_API IGloamsteadGeneratedAssetRuntimeIdentitySource
{
public:
	virtual ~IGloamsteadGeneratedAssetRuntimeIdentitySource() = default;
	virtual bool Observe(
		FGloamsteadGeneratedAssetRuntimeIdentity& OutIdentity,
		TArray<FString>& OutFailureCodes) const = 0;
};

/** The visible body of one runtime proxy: a single primitive mesh, no collision (never blocks the player). */
UCLASS()
class GLOAMSTEAD_API AGloamsteadMeshForgeProxyActor : public AActor
{
	GENERATED_BODY()

public:
	AGloamsteadMeshForgeProxyActor();

	/** Set the mesh + (possibly non-uniform) scale, and best-effort colour/emissive via a dynamic material. */
	void ConfigureVisual(UStaticMesh* Mesh, const FLinearColor& Color, const FVector& Scale, bool bEmissive);
	/** Configure a generated mesh without substituting any engine material fallback. */
	void ConfigureGeneratedVisual(UStaticMesh* Mesh, const FLinearColor& Color, const FVector& Scale, bool bEmissive);
	void SetProjectedMaterialParameters(float Wetness, bool bWarningActive);

	/** Re-tint at runtime (e.g. a night-feedback proxy reacting to the phase). Best-effort. */
	void SetVisualColor(const FLinearColor& Color, bool bEmissive);

	/** True when a mesh is actually assigned — the testable meaning of "visible proxy created". */
	bool HasVisibleMesh() const;

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> MeshComponent;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DynMaterial;
	bool bAllowEngineTintMaterial = true;
};

/**
 * Provider seam: turns a proxy spec + source binding into a visible thing. Replaceable — an engine-primitive
 * provider now, a generated-owned MeshForge-asset provider later — without changing the adapter's source
 * binding logic. Abstract: concrete providers must declare an honest descriptor and produce instances.
 */
UCLASS(Abstract)
class GLOAMSTEAD_API UGloamsteadMeshForgeProvider : public UObject
{
	GENERATED_BODY()

public:
	virtual FGloamsteadMeshForgeProviderDescriptor GetDescriptor() const
		PURE_VIRTUAL(UGloamsteadMeshForgeProvider::GetDescriptor, return FGloamsteadMeshForgeProviderDescriptor(););

	virtual bool CanSpawn(EGMFProxyType /*Type*/) const { return false; }

	virtual FGloamsteadMeshForgeProxyInstance CreateProxy(const FGloamsteadMeshForgeProxySpec& /*Spec*/,
		const FGloamsteadMeshForgeSourceBinding& /*Binding*/, UWorld* /*World*/)
		PURE_VIRTUAL(UGloamsteadMeshForgeProvider::CreateProxy, return FGloamsteadMeshForgeProxyInstance(););
};

/**
 * First provider: engine basic-shape primitives spawned at runtime. It is honestly code_owned_runtime_proxy —
 * it creates no assets and touches no binary content, and the validators reject it if it ever claims otherwise.
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadEnginePrimitiveMeshForgeProvider : public UGloamsteadMeshForgeProvider
{
	GENERATED_BODY()

public:
	virtual FGloamsteadMeshForgeProviderDescriptor GetDescriptor() const override;
	virtual bool CanSpawn(EGMFProxyType Type) const override { return true; }
	virtual FGloamsteadMeshForgeProxyInstance CreateProxy(const FGloamsteadMeshForgeProxySpec& Spec,
		const FGloamsteadMeshForgeSourceBinding& Binding, UWorld* World) override;

	/** Engine basic-shape asset for a proxy type (or null if unavailable). */
	static UStaticMesh* MeshForType(EGMFProxyType Type);

	/** Per-type primitive scale (a tall pillar for the Heart, a flat disc for the interaction radius, etc.). */
	static FVector ScaleForType(EGMFProxyType Type, float SpecScale);
};

UENUM(BlueprintType)
enum class EGMFGeneratedProviderState : uint8
{
	Uninitialized = 0,
	Loading,
	Ready,
	Failed,
};

/** Receipt-closed generated catalog provider. It never falls back to engine primitives. */
UCLASS()
class GLOAMSTEAD_API UGloamsteadGeneratedAssetMeshForgeProvider : public UGloamsteadMeshForgeProvider
{
	GENERATED_BODY()

public:
	void Configure(const UGloamsteadGeneratedAssetSettings& Settings);
	/** Cancel async work and invalidate this provider before adapter replacement/shutdown. */
	void Deactivate();
	void PreloadCatalogAsync(FSimpleDelegate Completion = FSimpleDelegate());
	/** Re-observe and bind the runtime identity; called on every adapter build/rebuild. */
	bool RevalidateRuntimeIdentity();

	EGMFGeneratedProviderState GetState() const { return State; }
	bool IsReadyForBuild() const { return State == EGMFGeneratedProviderState::Ready; }
	bool HasFailed() const { return State == EGMFGeneratedProviderState::Failed; }
	const TArray<FString>& GetFailureCodes() const { return FailureCodes; }
	const UGloamsteadGeneratedAssetCatalog* GetCatalog() const { return LoadedCatalog; }

	virtual FGloamsteadMeshForgeProviderDescriptor GetDescriptor() const override;
	virtual bool CanSpawn(EGMFProxyType Type) const override;
	virtual FGloamsteadMeshForgeProxyInstance CreateProxy(const FGloamsteadMeshForgeProxySpec& Spec,
		const FGloamsteadMeshForgeSourceBinding& Binding, UWorld* World) override;

#if WITH_DEV_AUTOMATION_TESTS
	/** Test seam: exercises the same validation transition without requiring an authored .uasset. */
	void Test_SetLoadedCatalog(UGloamsteadGeneratedAssetCatalog* Catalog,
		const FString& ExpectedBundleId, const FString& ExpectedReceiptSha256,
		const FGloamsteadGeneratedAssetRuntimeIdentity& ObservedRuntimeIdentity,
		FSimpleDelegate ObservationCallback = FSimpleDelegate());
	void Test_SetObservedRuntimeIdentity(
		const FGloamsteadGeneratedAssetRuntimeIdentity& ObservedRuntimeIdentity,
		FSimpleDelegate ObservationCallback = FSimpleDelegate());
	/** Test-only deterministic stand-ins for cooked Asset Registry/object resolution. */
	void Test_SetResolvedObject(const FSoftObjectPath& ObjectPath, UObject* Object);
	void Test_SetObservedProvenance(const FSoftObjectPath& ObjectPath,
		const FGloamsteadGeneratedAssetObservedProvenance& Provenance);
	/** Inject the exact direct hard+soft package dependencies returned by the cooked Asset Registry. */
	void Test_SetPackageDependencies(const FSoftObjectPath& ObjectPath,
		const TArray<FName>& DependencyPackages);
	void Test_MarkPackageDependencyQueryUnavailable(const FString& PackageName);
	void Test_SetObjectResolutionCallback(FSimpleDelegate Callback);
	void Test_SetActorSpawnCallback(
		TFunction<void(AGloamsteadMeshForgeProxyActor*)> Callback);
	TArray<FString> Test_ValidateDependencyClosure();
	void Test_ForceSpawnFailure(bool bForce) { bTestForceSpawnFailure = bForce; }
	uint64 Test_BeginPendingCatalogLoad();
	void Test_CompleteCatalogLoad(uint64 LoadGeneration, const FSoftObjectPath& RequestedPath,
		UGloamsteadGeneratedAssetCatalog* Catalog, FSimpleDelegate Completion = FSimpleDelegate());
	uint64 Test_GetProviderEpoch() const { return ProviderEpoch; }
#endif

private:
	struct FProviderOperationSnapshot
	{
		uint64 ProviderEpoch = 0;
		uint64 LoadGeneration = 0;
		EGMFGeneratedProviderState State = EGMFGeneratedProviderState::Uninitialized;
		TWeakObjectPtr<UGloamsteadGeneratedAssetCatalog> CatalogObjectGeneration;
		FString AcceptedCatalogContractSha256;
	};

	void AdvanceProviderEpoch();
	FProviderOperationSnapshot CaptureOperationSnapshot() const;
	bool IsOperationSnapshotCurrent(const FProviderOperationSnapshot& Snapshot) const;
	bool ValidateCurrentOperationAfterBoundary(const FProviderOperationSnapshot& Snapshot);
	void CancelOutstandingPreload();
	void FinishCatalogLoad(uint64 LoadGeneration, FSoftObjectPath RequestedPath, FSimpleDelegate Completion);
	void AcceptCatalogLoad(uint64 LoadGeneration, const FSoftObjectPath& RequestedPath,
		UGloamsteadGeneratedAssetCatalog* Catalog, FSimpleDelegate Completion);
	void ValidateLoadedCatalog();
	/** Re-run structural validation and compare every canonical contract byte to the accepted digest. */
	bool EnsureAcceptedCatalogContractCurrent();
	/** Irreversibly reject this configuration; only Configure followed by a fresh load clears the latch. */
	void InvalidateAcceptedCatalog(const TArray<FString>& Codes);
	bool IsCatalogObjectGenerationQuarantined(
		const UGloamsteadGeneratedAssetCatalog* Catalog) const;
	void QuarantineCatalogObjectGeneration(UGloamsteadGeneratedAssetCatalog* Catalog);
	void Fail(const TArray<FString>& Codes);
	UObject* ResolveEntryObject(const FSoftObjectPath& ObjectPath);
	FGloamsteadGeneratedAssetObservedProvenance ReadObservedProvenance(
		const FSoftObjectPath& ObjectPath) const;
	bool ReadPackageDependencies(FName PackageName, TArray<FName>& OutDependencies) const;
	TArray<FString> ValidateCatalogDependencyClosure();

	UPROPERTY() TSoftObjectPtr<UGloamsteadGeneratedAssetCatalog> CatalogPath;
	UPROPERTY() TObjectPtr<UGloamsteadGeneratedAssetCatalog> LoadedCatalog;
	UPROPERTY() EGMFGeneratedProviderState State = EGMFGeneratedProviderState::Uninitialized;
	UPROPERTY() TArray<FString> FailureCodes;
	FString ExpectedBundleId;
	FString ExpectedReceiptSha256;
	FString ExpectedTargetBuildIdentitySha256;
	TSharedPtr<FStreamableHandle> PreloadHandle;
	TSharedPtr<const IGloamsteadGeneratedAssetRuntimeIdentitySource> RuntimeIdentitySource;
	/** Exact authority set retained only after trusted runtime observation and catalog equality succeed. */
	TSet<FName> VerifiedTerminalScriptPackages;
	/** Canonical digest of every catalog contract field at the moment the provider entered Ready. */
	FString AcceptedCatalogContractSha256;
	/** Set by an integrity mutation. Preload/revalidation cannot clear it; Configure must. */
	bool bRequiresFreshConfiguration = false;
	/** Mutated UObject generations remain quarantined across Configure; only a new serialized load may replace one. */
	TArray<TWeakObjectPtr<UGloamsteadGeneratedAssetCatalog>> RejectedCatalogObjectGenerations;
	/** Monotonic token invalidating every outer frame when configuration or an operation is replaced. */
	uint64 ProviderEpoch = 0;
	uint64 LoadGeneration = 0;
#if WITH_DEV_AUTOMATION_TESTS
	TMap<FSoftObjectPath, TWeakObjectPtr<UObject>> TestResolvedObjects;
	TMap<FSoftObjectPath, FGloamsteadGeneratedAssetObservedProvenance> TestObservedProvenance;
	TMap<FName, TArray<FName>> TestPackageDependencies;
	TSet<FName> TestUnavailablePackageDependencyQueries;
	FSimpleDelegate TestObjectResolutionCallback;
	TFunction<void(AGloamsteadMeshForgeProxyActor*)> TestActorSpawnCallback;
	bool bTestForceSpawnFailure = false;
#endif
};
