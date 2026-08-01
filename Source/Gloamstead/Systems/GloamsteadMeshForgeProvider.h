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
struct FStreamableHandle;

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
	void PreloadCatalogAsync(FSimpleDelegate Completion = FSimpleDelegate());

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
		const FString& ExpectedBundleId, const FString& ExpectedReceiptSha256);
	/** Test-only deterministic stand-ins for cooked Asset Registry/object resolution. */
	void Test_SetResolvedObject(const FSoftObjectPath& ObjectPath, UObject* Object);
	void Test_SetObservedProvenance(const FSoftObjectPath& ObjectPath,
		const FGloamsteadGeneratedAssetObservedProvenance& Provenance);
	void Test_ForceSpawnFailure(bool bForce) { bTestForceSpawnFailure = bForce; }
	uint64 Test_BeginPendingCatalogLoad();
	void Test_CompleteCatalogLoad(uint64 LoadGeneration, const FSoftObjectPath& RequestedPath,
		UGloamsteadGeneratedAssetCatalog* Catalog, FSimpleDelegate Completion = FSimpleDelegate());
#endif

private:
	void CancelOutstandingPreload();
	void FinishCatalogLoad(uint64 LoadGeneration, FSoftObjectPath RequestedPath, FSimpleDelegate Completion);
	void AcceptCatalogLoad(uint64 LoadGeneration, const FSoftObjectPath& RequestedPath,
		UGloamsteadGeneratedAssetCatalog* Catalog, FSimpleDelegate Completion);
	void ValidateLoadedCatalog();
	void Fail(const TArray<FString>& Codes);
	UObject* ResolveEntryObject(const FSoftObjectPath& ObjectPath) const;
	FGloamsteadGeneratedAssetObservedProvenance ReadObservedProvenance(
		const FSoftObjectPath& ObjectPath) const;

	UPROPERTY() TSoftObjectPtr<UGloamsteadGeneratedAssetCatalog> CatalogPath;
	UPROPERTY() TObjectPtr<UGloamsteadGeneratedAssetCatalog> LoadedCatalog;
	UPROPERTY() EGMFGeneratedProviderState State = EGMFGeneratedProviderState::Uninitialized;
	UPROPERTY() TArray<FString> FailureCodes;
	FString ExpectedBundleId;
	FString ExpectedReceiptSha256;
	TSharedPtr<FStreamableHandle> PreloadHandle;
	uint64 LoadGeneration = 0;
#if WITH_DEV_AUTOMATION_TESTS
	TMap<FSoftObjectPath, TWeakObjectPtr<UObject>> TestResolvedObjects;
	TMap<FSoftObjectPath, FGloamsteadGeneratedAssetObservedProvenance> TestObservedProvenance;
	bool bTestForceSpawnFailure = false;
#endif
};
