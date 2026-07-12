#pragma once

#include "CoreMinimal.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "GloamsteadMeshForgeTypes.generated.h"

/**
 * Gloamstead MeshForge Adapter — data model (Corrected Wave 6A).
 *
 * The gameplay loop works but is invisible: the Heart is logic-only, ritual points are PCG data, night
 * feedback is numeric. This adapter makes the loop VISIBLE by binding proxy specs to real gameplay sources
 * and rendering them through a replaceable PROVIDER. The first provider uses engine-primitive meshes at
 * runtime (code-owned; no generated assets, no binary content). A future provider can emit generated_owned
 * MeshForge assets behind the same seam without changing the source-binding logic.
 *
 * HONESTY CONTRACT (enforced by the GMF validators): an engine-primitive provider is code_owned_runtime_proxy
 * and MUST NOT claim generated ownership or a generated asset path. The validators fail closed on overclaim.
 */

/** What a proxy makes visible. */
UENUM(BlueprintType)
enum class EGMFProxyType : uint8
{
	Heart              = 0,  // the rest point
	RitualPoint        = 1,  // a PCG ritual point marker
	LanternRestore     = 2,  // a restorable lantern target
	InteractionRadius  = 3,  // an affordance ring
	PathCue            = 4,  // an approach cue
	NightFeedback      = 5,  // phase/world readability cue
	CorruptionFeedback = 6,  // the night's corruption made visible
};

/** How a proxy's visual is produced. The seam that lets engine primitives be swapped for generated assets. */
UENUM(BlueprintType)
enum class EGMFProviderType : uint8
{
	EnginePrimitiveRuntimeProxy = 0, // engine basic-shape components spawned at runtime (this wave)
	GeneratedOwnedMeshForgeAsset = 1, // future: a generated .uasset proxy (editor/human gate required)
};

/** Who owns the proxy's visual. Kept explicit so a runtime proxy can never masquerade as a generated asset. */
UENUM(BlueprintType)
enum class EGMFOwnershipClass : uint8
{
	CodeOwnedRuntimeProxy = 0, // spawned by code from engine primitives; nothing authored/generated
	GeneratedOwned        = 1, // a generated asset owned by the MeshForge/WorldForge pipeline (future)
};

/** The gameplay system a proxy is bound to (read-only; the adapter never takes authority from it). */
UENUM(BlueprintType)
enum class EGMFSourceSystem : uint8
{
	None          = 0,
	VeilHeart     = 1,
	PCGSubsystem  = 2,
	RitualPlacement = 3,
	DayNight      = 4,
	NightRuntime  = 5,
};

/** A read-only binding from a proxy to the real gameplay source it visualizes. */
USTRUCT(BlueprintType)
struct FGloamsteadMeshForgeSourceBinding
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") EGMFSourceSystem SourceSystem = EGMFSourceSystem::None;
	UPROPERTY() TWeakObjectPtr<UObject> SourceObject;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") int32 SourcePointIndex = -1;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") ERitualType RitualType = ERitualType::Invalid;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") FName WarningTag;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") ENightConsequenceType NightType = ENightConsequenceType::Invalid;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") FVector WorldLocation = FVector::ZeroVector;
	/** True only when a real world location was resolved (a proxy is not spawned at a guessed location). */
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bLocationResolved = false;
};

/** What to make visible, independent of how (the provider decides the mesh). */
USTRUCT(BlueprintType)
struct FGloamsteadMeshForgeProxySpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") EGMFProxyType ProxyType = EGMFProxyType::Heart;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") FString ProxyId;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") FLinearColor Color = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") float Scale = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bInteractionRelevant = false;
};

/** A resolved proxy: spec + binding + the provider's honest provenance and spawn result. */
USTRUCT(BlueprintType)
struct FGloamsteadMeshForgeProxyInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") FGloamsteadMeshForgeProxySpec Spec;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") FGloamsteadMeshForgeSourceBinding Binding;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") EGMFProviderType ProviderType = EGMFProviderType::EnginePrimitiveRuntimeProxy;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") EGMFOwnershipClass OwnershipClass = EGMFOwnershipClass::CodeOwnedRuntimeProxy;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bRuntimeOnly = true;
	/** Empty for runtime proxies; a /Game/... path only when a real generated asset backs the proxy. */
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") FString GeneratedAssetPath;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bSpawned = false;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bVisibleProxyCreated = false;
	UPROPERTY() TWeakObjectPtr<AActor> SpawnedActor;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") TArray<FString> FailureCodes;
};

/** Provider self-description, validated so a provider cannot claim capabilities/ownership it does not have. */
USTRUCT(BlueprintType)
struct FGloamsteadMeshForgeProviderDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") FString ProviderId;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") EGMFProviderType ProviderType = EGMFProviderType::EnginePrimitiveRuntimeProxy;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") EGMFOwnershipClass OwnershipClass = EGMFOwnershipClass::CodeOwnedRuntimeProxy;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bSupportsRuntimePrimitives = false;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bSupportsGeneratedAssets = false;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bCanSpawnHeartProxy = false;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bCanSpawnRitualPointProxy = false;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bCanSpawnInteractionRadiusProxy = false;
	UPROPERTY(BlueprintReadOnly, Category = "MeshForge") bool bCanSpawnNightFeedbackProxy = false;
};

/** Aggregate visibility/provenance report — the auditable artifact for the wave. */
USTRUCT(BlueprintType)
struct FGloamsteadMeshForgeVisibilityReport
{
	GENERATED_BODY()

	UPROPERTY() FString ReportId;
	UPROPERTY() FString CreatedAt;
	UPROPERTY() FString GitSha;
	UPROPERTY() EGMFProviderType ProviderType = EGMFProviderType::EnginePrimitiveRuntimeProxy;
	UPROPERTY() EGMFOwnershipClass OwnershipClass = EGMFOwnershipClass::CodeOwnedRuntimeProxy;
	UPROPERTY() int32 ProxyCount = 0;
	UPROPERTY() int32 HeartProxyCount = 0;
	UPROPERTY() int32 RitualPointProxyCount = 0;
	UPROPERTY() int32 LanternProxyCount = 0;
	UPROPERTY() int32 InteractionRadiusProxyCount = 0;
	UPROPERTY() int32 NightFeedbackProxyCount = 0;
	UPROPERTY() int32 GeneratedAssetCount = 0;
	UPROPERTY() int32 RuntimeOnlyProxyCount = 0;
	UPROPERTY() bool bBinaryContentTouched = false;
	UPROPERTY() TArray<FString> FailureCodes;
	UPROPERTY() TArray<FGloamsteadMeshForgeProxyInstance> Proxies;
};

// ===== Token / display helpers =====
GLOAMSTEAD_API FString GMFProxyTypeToken(EGMFProxyType Type);
GLOAMSTEAD_API FString GMFProviderTypeToken(EGMFProviderType Type);
GLOAMSTEAD_API FString GMFOwnershipClassToken(EGMFOwnershipClass Ownership);
GLOAMSTEAD_API FString GMFSourceSystemToken(EGMFSourceSystem System);

// ===== Fail-closed validation (returns GMF codes; empty = valid) =====
/** Validate a provider descriptor's internal consistency (capabilities vs. type/ownership). */
GLOAMSTEAD_API TArray<FString> GMFValidateDescriptor(const FGloamsteadMeshForgeProviderDescriptor& Descriptor);
/** Validate a single proxy instance's provenance honesty (no generated-ownership overclaim). */
GLOAMSTEAD_API TArray<FString> GMFValidateInstance(const FGloamsteadMeshForgeProxyInstance& Instance);
/** Validate an aggregate report (coverage + honesty + no binary content touched). */
GLOAMSTEAD_API TArray<FString> GMFValidateReport(const FGloamsteadMeshForgeVisibilityReport& Report);

namespace GloamsteadMeshForgeReport
{
	/** Default report dir: <ProjectDir>/procedural/reports/gloamstead_meshforge (git-ignored; regen by gate). */
	GLOAMSTEAD_API FString DefaultReportDir();
	/** Serialize the visibility report (+ provider + source-binding views) to JSON files under OutDir. */
	GLOAMSTEAD_API bool WriteReports(const FGloamsteadMeshForgeVisibilityReport& Report,
		const FGloamsteadMeshForgeProviderDescriptor& Descriptor, const FString& OutDir, FString& OutPrimaryPath);
}
