#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/GloamsteadMeshForgeTypes.h"
#include "GloamsteadMeshForgeProvider.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;

/** The visible body of one runtime proxy: a single primitive mesh, no collision (never blocks the player). */
UCLASS()
class GLOAMSTEAD_API AGloamsteadMeshForgeProxyActor : public AActor
{
	GENERATED_BODY()

public:
	AGloamsteadMeshForgeProxyActor();

	/** Set the mesh + (possibly non-uniform) scale, and best-effort colour/emissive via a dynamic material. */
	void ConfigureVisual(UStaticMesh* Mesh, const FLinearColor& Color, const FVector& Scale, bool bEmissive);

	/** Re-tint at runtime (e.g. a night-feedback proxy reacting to the phase). Best-effort. */
	void SetVisualColor(const FLinearColor& Color, bool bEmissive);

	/** True when a mesh is actually assigned — the testable meaning of "visible proxy created". */
	bool HasVisibleMesh() const;

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> MeshComponent;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DynMaterial;
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
