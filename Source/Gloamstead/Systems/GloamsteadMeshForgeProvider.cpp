#include "Systems/GloamsteadMeshForgeProvider.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "Data/GloamsteadGeneratedAssetCatalog.h"
#include "Settings/GloamsteadGeneratedAssetSettings.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	class FUnavailableRuntimeIdentitySource final : public IGloamsteadGeneratedAssetRuntimeIdentitySource
	{
	public:
		virtual bool Observe(
			FGloamsteadGeneratedAssetRuntimeIdentity& OutIdentity,
			TArray<FString>& OutFailureCodes) const override
		{
			OutIdentity = {};
			OutFailureCodes = { TEXT("GAC037") };
			return false;
		}
	};

#if WITH_DEV_AUTOMATION_TESTS
	class FFixedRuntimeIdentitySource final : public IGloamsteadGeneratedAssetRuntimeIdentitySource
	{
	public:
		explicit FFixedRuntimeIdentitySource(
			const FGloamsteadGeneratedAssetRuntimeIdentity& InIdentity,
			FSimpleDelegate InObservationCallback = FSimpleDelegate())
			: Identity(InIdentity)
			, ObservationCallback(MoveTemp(InObservationCallback)) {}

		virtual bool Observe(
			FGloamsteadGeneratedAssetRuntimeIdentity& OutIdentity,
			TArray<FString>& OutFailureCodes) const override
		{
			ObservationCallback.ExecuteIfBound();
			OutIdentity = Identity;
			OutFailureCodes.Reset();
			return true;
		}

	private:
		FGloamsteadGeneratedAssetRuntimeIdentity Identity;
		mutable FSimpleDelegate ObservationCallback;
	};
#endif

	const FString GloamGeneratedPackageRoot = TEXT("/Game/Gloamstead/Generated");

	bool IsPackageUnderRoot(const FString& PackageName, const FString& Root)
	{
		return PackageName.Equals(Root, ESearchCase::IgnoreCase)
			|| PackageName.StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase);
	}

	bool IsTerminalPlatformPackage(
		const FString& PackageName,
		const TArray<FString>& ExactPackages,
		const TArray<FString>& SafeRoots)
	{
		for (const FString& ExactPackage : ExactPackages)
		{
			if (PackageName.Equals(ExactPackage, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		for (const FString& Root : SafeRoots)
		{
			if (IsPackageUnderRoot(PackageName, Root))
			{
				return true;
			}
		}
		return false;
	}
}

// ===== AGloamsteadMeshForgeProxyActor =====

AGloamsteadMeshForgeProxyActor::AGloamsteadMeshForgeProxyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProxyMesh"));
	SetRootComponent(MeshComponent);
	// A readability proxy must never block movement or interaction traces.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCastShadow(false);
}

void AGloamsteadMeshForgeProxyActor::ConfigureVisual(UStaticMesh* Mesh, const FLinearColor& Color, const FVector& Scale, bool bEmissive)
{
	bAllowEngineTintMaterial = true;
	if (!MeshComponent)
	{
		return;
	}
	if (Mesh)
	{
		MeshComponent->SetStaticMesh(Mesh);
	}
	const FVector Safe(FMath::Max(Scale.X, 0.01f), FMath::Max(Scale.Y, 0.01f), FMath::Max(Scale.Z, 0.01f));
	MeshComponent->SetWorldScale3D(Safe);
	SetVisualColor(Color, bEmissive);
}

void AGloamsteadMeshForgeProxyActor::ConfigureGeneratedVisual(
	UStaticMesh* Mesh, const FLinearColor& Color, const FVector& Scale, bool bEmissive)
{
	bAllowEngineTintMaterial = false;
	DynMaterial = nullptr;
	if (!MeshComponent || !Mesh)
	{
		return;
	}
	MeshComponent->SetStaticMesh(Mesh);
	const FVector Safe(FMath::Max(Scale.X, 0.01f), FMath::Max(Scale.Y, 0.01f), FMath::Max(Scale.Z, 0.01f));
	MeshComponent->SetWorldScale3D(Safe);
	SetVisualColor(Color, bEmissive);
}

void AGloamsteadMeshForgeProxyActor::SetVisualColor(const FLinearColor& Color, bool bEmissive)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}
	if (!DynMaterial)
	{
		// The /Engine/BasicShapes meshes ship with BasicShapeMaterial — a constant grey material that
		// exposes NO parameters. Tinting a dynamic instance of it silently no-ops, so every proxy renders
		// identical grey and the readability colour language is invisible (confirmed in PIE, W6a). Base the
		// tint on an engine material that is genuinely parameter-driven and emissive so the colours render.
		// Still code-only: an engine material, no authored/binary content.
		UMaterialInterface* Base = nullptr;
		if (bAllowEngineTintMaterial)
		{
			Base = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
		}
		// Generated visuals use only the material carried by their generated mesh; primitives retain
		// their existing best-effort engine material behavior.
		if (!Base) { Base = MeshComponent->GetMaterial(0); }
		if (Base)
		{
			DynMaterial = UMaterialInstanceDynamic::Create(Base, this);
			if (DynMaterial)
			{
				MeshComponent->SetMaterial(0, DynMaterial);
			}
		}
	}
	if (DynMaterial)
	{
		// Emissive proxies read as gently glowing beacons in daylight without blooming to white — a
		// modest boost keeps the hue readable (Color*4 blew out to near-white in PIE, W6a).
		const FLinearColor Emissive = bEmissive ? (Color * 1.25f) : Color;
		// Engine emissive/tint materials name their colour param differently across versions; set several
		// harmless aliases so whichever one exists takes effect (unknown params are ignored by the DMI).
		DynMaterial->SetVectorParameterValue(TEXT("Color"), Emissive);
		DynMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
		DynMaterial->SetVectorParameterValue(TEXT("Emissive"), Emissive);
		DynMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
		DynMaterial->SetVectorParameterValue(TEXT("Tint"), Color);
		DynMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), bEmissive ? 1.25f : 1.0f);
	}
}

void AGloamsteadMeshForgeProxyActor::SetProjectedMaterialParameters(float Wetness, bool bWarningActive)
{
	if (DynMaterial)
	{
		DynMaterial->SetScalarParameterValue(TEXT("Wetness"), FMath::Clamp(Wetness, 0.f, 1.f));
		DynMaterial->SetScalarParameterValue(TEXT("WarningActive"), bWarningActive ? 1.f : 0.f);
	}
}

bool AGloamsteadMeshForgeProxyActor::HasVisibleMesh() const
{
	return MeshComponent && MeshComponent->GetStaticMesh() != nullptr;
}

// ===== UGloamsteadEnginePrimitiveMeshForgeProvider =====

UStaticMesh* UGloamsteadEnginePrimitiveMeshForgeProvider::MeshForType(EGMFProxyType Type)
{
	const TCHAR* Path = TEXT("/Engine/BasicShapes/Cube.Cube");
	switch (Type)
	{
	case EGMFProxyType::Heart:              Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); break; // a standing pillar
	case EGMFProxyType::RitualPoint:        Path = TEXT("/Engine/BasicShapes/Cube.Cube"); break;
	case EGMFProxyType::LanternRestore:     Path = TEXT("/Engine/BasicShapes/Cone.Cone"); break;        // a beacon
	case EGMFProxyType::InteractionRadius:  Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); break; // a flat disc (scaled)
	case EGMFProxyType::PathCue:            Path = TEXT("/Engine/BasicShapes/Cone.Cone"); break;
	case EGMFProxyType::NightFeedback:      Path = TEXT("/Engine/BasicShapes/Sphere.Sphere"); break;
	case EGMFProxyType::CorruptionFeedback: Path = TEXT("/Engine/BasicShapes/Sphere.Sphere"); break;
	default:                                Path = TEXT("/Engine/BasicShapes/Cube.Cube"); break;
	}
	return LoadObject<UStaticMesh>(nullptr, Path);
}

FVector UGloamsteadEnginePrimitiveMeshForgeProvider::ScaleForType(EGMFProxyType Type, float SpecScale)
{
	const float S = FMath::Max(SpecScale, 0.05f);
	switch (Type)
	{
	case EGMFProxyType::Heart:              return FVector(1.5f * S, 1.5f * S, 2.5f * S); // a standing pillar
	case EGMFProxyType::RitualPoint:        return FVector(0.6f * S, 0.6f * S, 0.6f * S);
	case EGMFProxyType::LanternRestore:     return FVector(0.6f * S, 0.6f * S, 1.2f * S); // a beacon
	case EGMFProxyType::InteractionRadius:  return FVector(S, S, 0.05f);                   // a flat disc
	case EGMFProxyType::PathCue:            return FVector(0.4f * S, 0.4f * S, 0.4f * S);
	case EGMFProxyType::NightFeedback:      return FVector(1.2f * S, 1.2f * S, 1.2f * S);
	case EGMFProxyType::CorruptionFeedback: return FVector(0.8f * S, 0.8f * S, 0.8f * S);
	default:                                return FVector(S, S, S);
	}
}

FGloamsteadMeshForgeProviderDescriptor UGloamsteadEnginePrimitiveMeshForgeProvider::GetDescriptor() const
{
	FGloamsteadMeshForgeProviderDescriptor D;
	D.ProviderId = TEXT("gmf.engine_primitive.v1");
	D.ProviderType = EGMFProviderType::EnginePrimitiveRuntimeProxy;
	D.OwnershipClass = EGMFOwnershipClass::CodeOwnedRuntimeProxy;
	D.bSupportsRuntimePrimitives = true;
	D.bSupportsGeneratedAssets = false; // honest: this provider generates nothing
	D.bCanSpawnHeartProxy = true;
	D.bCanSpawnRitualPointProxy = true;
	D.bCanSpawnInteractionRadiusProxy = true;
	D.bCanSpawnNightFeedbackProxy = true;
	return D;
}

FGloamsteadMeshForgeProxyInstance UGloamsteadEnginePrimitiveMeshForgeProvider::CreateProxy(
	const FGloamsteadMeshForgeProxySpec& Spec, const FGloamsteadMeshForgeSourceBinding& Binding, UWorld* World)
{
	FGloamsteadMeshForgeProxyInstance Inst;
	Inst.Spec = Spec;
	Inst.Binding = Binding;
	// Honest provenance — a runtime proxy, never a generated asset.
	Inst.ProviderType = EGMFProviderType::EnginePrimitiveRuntimeProxy;
	Inst.OwnershipClass = EGMFOwnershipClass::CodeOwnedRuntimeProxy;
	Inst.bRuntimeOnly = true;
	Inst.GeneratedAssetPath = FString(); // null in the report

	// No world or no resolved location -> record the spec/binding but do NOT spawn at a guessed place.
	if (!World || !Binding.bLocationResolved)
	{
		Inst.bSpawned = false;
		Inst.bVisibleProxyCreated = false;
		return Inst;
	}

	UStaticMesh* Mesh = MeshForType(Spec.ProxyType);
	if (!Mesh)
	{
		Inst.bSpawned = false;
		Inst.bVisibleProxyCreated = false;
		return Inst;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGloamsteadMeshForgeProxyActor* Actor = World->SpawnActor<AGloamsteadMeshForgeProxyActor>(
		AGloamsteadMeshForgeProxyActor::StaticClass(), Binding.WorldLocation, FRotator::ZeroRotator, Params);

	if (Actor)
	{
		Actor->ConfigureVisual(Mesh, Spec.Color, ScaleForType(Spec.ProxyType, Spec.Scale), /*bEmissive*/ true);
		Inst.SpawnedActor = Actor;
		Inst.bSpawned = true;
		Inst.bVisibleProxyCreated = Actor->HasVisibleMesh();
	}
	return Inst;
}

// ===== UGloamsteadGeneratedAssetMeshForgeProvider =====

void UGloamsteadGeneratedAssetMeshForgeProvider::AdvanceProviderEpoch()
{
	checkf(ProviderEpoch != MAX_uint64, TEXT("Generated asset provider epoch exhausted"));
	++ProviderEpoch;
}

UGloamsteadGeneratedAssetMeshForgeProvider::FProviderOperationSnapshot
UGloamsteadGeneratedAssetMeshForgeProvider::CaptureOperationSnapshot() const
{
	FProviderOperationSnapshot Snapshot;
	Snapshot.ProviderEpoch = ProviderEpoch;
	Snapshot.LoadGeneration = LoadGeneration;
	Snapshot.State = State;
	Snapshot.CatalogObjectGeneration = LoadedCatalog.Get();
	Snapshot.AcceptedCatalogContractSha256 = AcceptedCatalogContractSha256;
	return Snapshot;
}

bool UGloamsteadGeneratedAssetMeshForgeProvider::IsOperationSnapshotCurrent(
	const FProviderOperationSnapshot& Snapshot) const
{
	return Snapshot.ProviderEpoch == ProviderEpoch
		&& Snapshot.LoadGeneration == LoadGeneration
		&& Snapshot.State == State
		&& Snapshot.CatalogObjectGeneration
			== TWeakObjectPtr<UGloamsteadGeneratedAssetCatalog>(LoadedCatalog.Get())
		&& Snapshot.AcceptedCatalogContractSha256.Equals(
			AcceptedCatalogContractSha256, ESearchCase::CaseSensitive);
}

bool UGloamsteadGeneratedAssetMeshForgeProvider::ValidateCurrentOperationAfterBoundary(
	const FProviderOperationSnapshot& Snapshot)
{
	// Snapshot identity is checked first: validating a stale outer catalog would mutate the new generation.
	return IsOperationSnapshotCurrent(Snapshot) && EnsureAcceptedCatalogContractCurrent();
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Configure(const UGloamsteadGeneratedAssetSettings& Settings)
{
	AdvanceProviderEpoch();
	const uint64 ConfigurationEpoch = ProviderEpoch;
	const uint64 PriorLoadGeneration = LoadGeneration;
	CancelOutstandingPreload();
	if (ProviderEpoch != ConfigurationEpoch || LoadGeneration != PriorLoadGeneration + 1)
	{
		// Cancellation is a user-code boundary for typed consumers. A nested lifecycle call owns the UObject.
		return;
	}
	// Configuration is a production lifecycle boundary. A development automation observer is valid
	// only for the explicit test operation that installed it and must never survive reuse of this UObject.
	RuntimeIdentitySource = MakeShared<FUnavailableRuntimeIdentitySource>();
	CatalogPath = Settings.Catalog;
	ExpectedBundleId = Settings.ExpectedActiveBundleId;
	ExpectedReceiptSha256 = Settings.ExpectedReceiptSha256;
	ExpectedTargetBuildIdentitySha256 = Settings.ExpectedTargetBuildIdentitySha256;
	VerifiedTerminalScriptPackages.Reset();
	AcceptedCatalogContractSha256.Reset();
	bRequiresFreshConfiguration = false;
	LoadedCatalog = nullptr;
	FailureCodes.Reset();
	State = EGMFGeneratedProviderState::Uninitialized;
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Deactivate()
{
	AdvanceProviderEpoch();
	const uint64 DeactivationEpoch = ProviderEpoch;
	const uint64 PriorLoadGeneration = LoadGeneration;
	const bool bPreserveMutationLatch = bRequiresFreshConfiguration;
	CancelOutstandingPreload();
	if (ProviderEpoch != DeactivationEpoch || LoadGeneration != PriorLoadGeneration + 1)
	{
		return;
	}
	RuntimeIdentitySource = MakeShared<FUnavailableRuntimeIdentitySource>();
	VerifiedTerminalScriptPackages.Reset();
	AcceptedCatalogContractSha256.Reset();
	bRequiresFreshConfiguration = bPreserveMutationLatch;
	LoadedCatalog = nullptr;
	FailureCodes.Reset();
	State = EGMFGeneratedProviderState::Uninitialized;
}

void UGloamsteadGeneratedAssetMeshForgeProvider::PreloadCatalogAsync(FSimpleDelegate Completion)
{
	PreloadCatalogAsyncWithResult(FGloamsteadGeneratedCatalogLoadCompletion::CreateLambda(
		[Completion = MoveTemp(Completion)](
			const FGloamsteadGeneratedCatalogLoadResult& Result) mutable
		{
			// Preserve the legacy public contract: current terminal paths notify, superseded/cancelled
			// requests remain silent. Generation-aware consumers use the typed overload below.
			if (Result.Terminal == EGMFGeneratedCatalogLoadTerminal::Accepted
				|| Result.Terminal == EGMFGeneratedCatalogLoadTerminal::Rejected)
			{
				Completion.ExecuteIfBound();
			}
		}));
}

void UGloamsteadGeneratedAssetMeshForgeProvider::PreloadCatalogAsyncWithResult(
	FGloamsteadGeneratedCatalogLoadCompletion Completion)
{
	AdvanceProviderEpoch();
	const uint64 RequestEpoch = ProviderEpoch;
	const uint64 PriorLoadGeneration = LoadGeneration;
	CancelOutstandingPreload();
	const uint64 RequestLoadGeneration = PriorLoadGeneration + 1;
	auto CompleteLocal = [this, &Completion, RequestEpoch, RequestLoadGeneration](
		EGMFGeneratedCatalogLoadTerminal Terminal)
	{
		if (Completion.IsBound())
		{
			const FGloamsteadGeneratedCatalogLoadResult Result = MakeCatalogLoadResult(
				Terminal, RequestEpoch, RequestLoadGeneration, nullptr);
			Completion.Execute(Result);
			Completion.Unbind();
		}
	};
	if (ProviderEpoch != RequestEpoch || LoadGeneration != RequestLoadGeneration)
	{
		CompleteLocal(EGMFGeneratedCatalogLoadTerminal::Stale);
		return;
	}
	if (bRequiresFreshConfiguration)
	{
		TArray<FString> Codes = FailureCodes;
		Codes.AddUnique(TEXT("GAC039"));
		Fail(Codes);
		CompleteLocal(EGMFGeneratedCatalogLoadTerminal::Rejected);
		return;
	}
	if (!AcceptedCatalogContractSha256.IsEmpty() && !EnsureAcceptedCatalogContractCurrent())
	{
		CompleteLocal(EGMFGeneratedCatalogLoadTerminal::Rejected);
		return;
	}
	FailureCodes.Reset();
	if (CatalogPath.IsNull())
	{
		Fail({ TEXT("GAC017") });
		CompleteLocal(EGMFGeneratedCatalogLoadTerminal::Rejected);
		return;
	}

	PendingCompletionLoadGeneration = LoadGeneration;
	PendingCompletionProviderEpoch = ProviderEpoch;
	PendingCatalogLoadCompletion = MoveTemp(Completion);
	State = EGMFGeneratedProviderState::Loading;
	const uint64 RequestGeneration = LoadGeneration;
	const FSoftObjectPath RequestedPath = CatalogPath.ToSoftObjectPath();
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestDeferNextCatalogLoad)
	{
		bTestDeferNextCatalogLoad = false;
		return;
	}
#endif
	PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		RequestedPath,
		FStreamableDelegate::CreateWeakLambda(this,
			[this, RequestGeneration, RequestedPath]()
			{
				FinishCatalogLoad(RequestGeneration, RequestedPath);
			}));
	if (!PreloadHandle.IsValid())
	{
		Fail({ TEXT("GAC017") });
		CompletePendingCatalogLoad(EGMFGeneratedCatalogLoadTerminal::Rejected);
	}
}

bool UGloamsteadGeneratedAssetMeshForgeProvider::IsCatalogLoadResultCurrent(
	const FGloamsteadGeneratedCatalogLoadResult& Result) const
{
	return Result.Terminal == EGMFGeneratedCatalogLoadTerminal::Accepted
		&& Result.ProviderEpoch == ProviderEpoch
		&& Result.LoadGeneration == LoadGeneration
		&& Result.ProviderState == EGMFGeneratedProviderState::Ready
		&& State == EGMFGeneratedProviderState::Ready
		&& Result.CatalogObjectGeneration
			== TWeakObjectPtr<UGloamsteadGeneratedAssetCatalog>(LoadedCatalog.Get())
		&& !Result.AcceptedCatalogContractSha256.IsEmpty()
		&& Result.AcceptedCatalogContractSha256.Equals(
			AcceptedCatalogContractSha256, ESearchCase::CaseSensitive);
}

FGloamsteadGeneratedCatalogLoadResult
UGloamsteadGeneratedAssetMeshForgeProvider::MakeCatalogLoadResult(
	EGMFGeneratedCatalogLoadTerminal Terminal,
	uint64 ResultProviderEpoch,
	uint64 ResultLoadGeneration,
	UGloamsteadGeneratedAssetCatalog* ResultCatalog) const
{
	FGloamsteadGeneratedCatalogLoadResult Result;
	Result.Terminal = Terminal;
	Result.ProviderEpoch = ResultProviderEpoch;
	Result.LoadGeneration = ResultLoadGeneration;
	Result.ProviderState = State;
	Result.CatalogObjectGeneration = ResultCatalog;
	Result.AcceptedCatalogContractSha256 = AcceptedCatalogContractSha256;
	return Result;
}

void UGloamsteadGeneratedAssetMeshForgeProvider::CompletePendingCatalogLoad(
	EGMFGeneratedCatalogLoadTerminal Terminal)
{
	FGloamsteadGeneratedCatalogLoadCompletion Completion = MoveTemp(PendingCatalogLoadCompletion);
	PendingCatalogLoadCompletion.Unbind();
	const uint64 ResultProviderEpoch = PendingCompletionProviderEpoch;
	const uint64 ResultLoadGeneration = PendingCompletionLoadGeneration;
	PendingCompletionProviderEpoch = 0;
	PendingCompletionLoadGeneration = 0;
	if (Completion.IsBound())
	{
		const FGloamsteadGeneratedCatalogLoadResult Result = MakeCatalogLoadResult(
			Terminal, ResultProviderEpoch, ResultLoadGeneration, nullptr);
		Completion.Execute(Result);
	}
}

void UGloamsteadGeneratedAssetMeshForgeProvider::CancelOutstandingPreload()
{
	checkf(LoadGeneration != MAX_uint64, TEXT("Generated asset provider load generation exhausted"));
	++LoadGeneration;
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle->ReleaseHandle();
		PreloadHandle.Reset();
	}
	CompletePendingCatalogLoad(EGMFGeneratedCatalogLoadTerminal::Cancelled);
}

void UGloamsteadGeneratedAssetMeshForgeProvider::FinishCatalogLoad(
	uint64 InLoadGeneration, FSoftObjectPath RequestedPath)
{
	UGloamsteadGeneratedAssetCatalog* Catalog = Cast<UGloamsteadGeneratedAssetCatalog>(RequestedPath.ResolveObject());
	AcceptCatalogLoad(InLoadGeneration, RequestedPath, Catalog);
}

void UGloamsteadGeneratedAssetMeshForgeProvider::AcceptCatalogLoad(
	uint64 InLoadGeneration,
	const FSoftObjectPath& RequestedPath,
	UGloamsteadGeneratedAssetCatalog* Catalog,
	FGloamsteadGeneratedCatalogLoadCompletion CompletionOverride)
{
	if (InLoadGeneration != LoadGeneration)
	{
		// The generation was already retired by cancellation. A late platform callback owns no completion.
		return;
	}
	FGloamsteadGeneratedCatalogLoadCompletion Completion;
	uint64 CompletionProviderEpoch = ProviderEpoch;
	if (CompletionOverride.IsBound())
	{
		Completion = MoveTemp(CompletionOverride);
	}
	else if (PendingCompletionLoadGeneration == InLoadGeneration)
	{
		Completion = MoveTemp(PendingCatalogLoadCompletion);
		PendingCatalogLoadCompletion.Unbind();
		CompletionProviderEpoch = PendingCompletionProviderEpoch;
		PendingCompletionProviderEpoch = 0;
		PendingCompletionLoadGeneration = 0;
	}
	AdvanceProviderEpoch();
	const uint64 AcceptanceEpoch = ProviderEpoch;
	auto Complete = [this, &Completion, CompletionProviderEpoch, AcceptanceEpoch,
		InLoadGeneration, Catalog](
		EGMFGeneratedCatalogLoadTerminal Terminal)
	{
		if (Completion.IsBound())
		{
			const uint64 ResultEpoch = Terminal == EGMFGeneratedCatalogLoadTerminal::Accepted
				? AcceptanceEpoch
				: CompletionProviderEpoch;
			const FGloamsteadGeneratedCatalogLoadResult Result = MakeCatalogLoadResult(
				Terminal, ResultEpoch, InLoadGeneration, Catalog);
			Completion.Execute(Result);
			Completion.Unbind();
		}
	};
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->ReleaseHandle();
		PreloadHandle.Reset();
	}
	if (RequestedPath != CatalogPath.ToSoftObjectPath())
	{
		Fail({ TEXT("GAC017") });
		Complete(EGMFGeneratedCatalogLoadTerminal::Rejected);
		return;
	}
	if (IsCatalogObjectGenerationQuarantined(Catalog))
	{
		LoadedCatalog = Catalog;
		InvalidateAcceptedCatalog({ TEXT("GAC039") });
		Complete(EGMFGeneratedCatalogLoadTerminal::Rejected);
		return;
	}
	if (!AcceptedCatalogContractSha256.IsEmpty())
	{
		if (!EnsureAcceptedCatalogContractCurrent())
		{
			Complete(EGMFGeneratedCatalogLoadTerminal::Rejected);
			return;
		}
		if (!Catalog || !GACCatalogContractSha256(*Catalog).Equals(
			AcceptedCatalogContractSha256, ESearchCase::CaseSensitive))
		{
			InvalidateAcceptedCatalog({ TEXT("GAC039") });
			Complete(EGMFGeneratedCatalogLoadTerminal::Rejected);
			return;
		}
	}
	LoadedCatalog = Catalog;
	if (!LoadedCatalog) { Fail({ TEXT("GAC017") }); }
	else { ValidateLoadedCatalog(); }
	const bool bGenerationStillOwned = ProviderEpoch == AcceptanceEpoch
		&& LoadGeneration == InLoadGeneration
		&& RequestedPath == CatalogPath.ToSoftObjectPath();
	const bool bAccepted = bGenerationStillOwned
		&& State == EGMFGeneratedProviderState::Ready
		&& LoadedCatalog == Catalog
		&& !AcceptedCatalogContractSha256.IsEmpty();
	Complete(bAccepted
		? EGMFGeneratedCatalogLoadTerminal::Accepted
		: (bGenerationStillOwned
			? EGMFGeneratedCatalogLoadTerminal::Rejected
			: EGMFGeneratedCatalogLoadTerminal::Stale));
}

void UGloamsteadGeneratedAssetMeshForgeProvider::ValidateLoadedCatalog()
{
	VerifiedTerminalScriptPackages.Reset();
	AcceptedCatalogContractSha256.Reset();
	if (bRequiresFreshConfiguration)
	{
		TArray<FString> Codes = FailureCodes;
		Codes.AddUnique(TEXT("GAC039"));
		Fail(Codes);
		return;
	}
	if (!LoadedCatalog)
	{
		Fail({ TEXT("GAC017") });
		return;
	}
	TStrongObjectPtr<UGloamsteadGeneratedAssetCatalog> CatalogGuard(LoadedCatalog.Get());
	UGloamsteadGeneratedAssetCatalog* const Catalog = CatalogGuard.Get();
	if (IsCatalogObjectGenerationQuarantined(Catalog))
	{
		InvalidateAcceptedCatalog({ TEXT("GAC039") });
		return;
	}
	const FString ValidationStartSha256 = GACCatalogContractSha256(*Catalog);
	if (ValidationStartSha256.IsEmpty())
	{
		InvalidateAcceptedCatalog({ TEXT("GAC039") });
		return;
	}

	// The catalog is intentionally generic and may also carry placement/material/VFX entries. Selection
	// enforces the provider's narrower static-mesh support on the exact chosen entry before spawning.
	TArray<FString> Codes = GACValidateCatalog(*Catalog, /*bRequireMeshForgeCompatibleClasses*/ false);
	FGloamsteadGeneratedAssetRuntimeIdentity ObservedRuntimeIdentity;
	TArray<FString> ObservationFailures;
	if (!RuntimeIdentitySource.IsValid())
	{
		RuntimeIdentitySource = MakeShared<FUnavailableRuntimeIdentitySource>();
	}
	const TSharedPtr<const IGloamsteadGeneratedAssetRuntimeIdentitySource> IdentitySource =
		RuntimeIdentitySource;
	const FProviderOperationSnapshot ObservationSnapshot = CaptureOperationSnapshot();
	const bool bObserved = IdentitySource->Observe(ObservedRuntimeIdentity, ObservationFailures);
	if (!IsOperationSnapshotCurrent(ObservationSnapshot))
	{
		// A nested Configure/Deactivate/accept owns the provider now. The stale frame must be inert.
		return;
	}
	if (!bObserved)
	{
		ObservationFailures.AddUnique(TEXT("GAC037"));
	}
	for (const FString& Code : ObservationFailures)
	{
		Codes.AddUnique(Code);
	}
	for (const FString& Code : GACValidateActiveBinding(*Catalog, ExpectedBundleId,
		ExpectedReceiptSha256, ExpectedTargetBuildIdentitySha256, ObservedRuntimeIdentity))
	{
		Codes.AddUnique(Code);
	}
	const FString PostObservationSha256 = GACCatalogContractSha256(*Catalog);
	if (PostObservationSha256.IsEmpty()
		|| !PostObservationSha256.Equals(ValidationStartSha256, ESearchCase::CaseSensitive))
	{
		Codes.AddUnique(TEXT("GAC039"));
		InvalidateAcceptedCatalog(Codes);
		return;
	}
	if (Codes.Num() > 0)
	{
		Fail(Codes);
		return;
	}

	FailureCodes.Reset();
	TArray<FGloamsteadGeneratedScriptPackageAuthority> ObservedAuthorities;
	TArray<FString> AuthorityFailures;
	if (!GACDeriveTerminalScriptPackageAuthorities(
		ObservedRuntimeIdentity, ObservedAuthorities, AuthorityFailures))
	{
		Fail({ TEXT("GAC037") });
		return;
	}
	const FString ValidationEndSha256 = GACCatalogContractSha256(*Catalog);
	if (ValidationEndSha256.IsEmpty()
		|| !ValidationEndSha256.Equals(ValidationStartSha256, ESearchCase::CaseSensitive))
	{
		InvalidateAcceptedCatalog({ TEXT("GAC039") });
		return;
	}
	if (!IsOperationSnapshotCurrent(ObservationSnapshot))
	{
		return;
	}
	for (const FGloamsteadGeneratedScriptPackageAuthority& Authority : ObservedAuthorities)
	{
		VerifiedTerminalScriptPackages.Add(FName(*Authority.PackageName));
	}
	AcceptedCatalogContractSha256 = ValidationStartSha256;
	if (AcceptedCatalogContractSha256.IsEmpty())
	{
		Fail({ TEXT("GAC039") });
		return;
	}
	State = EGMFGeneratedProviderState::Ready;
}

bool UGloamsteadGeneratedAssetMeshForgeProvider::RevalidateRuntimeIdentity()
{
	if (State != EGMFGeneratedProviderState::Ready
		|| AcceptedCatalogContractSha256.IsEmpty()
		|| !LoadedCatalog
		|| !EnsureAcceptedCatalogContractCurrent())
	{
		return false;
	}
	TStrongObjectPtr<UGloamsteadGeneratedAssetCatalog> CatalogGuard(LoadedCatalog.Get());
	UGloamsteadGeneratedAssetCatalog* const Catalog = CatalogGuard.Get();
	FGloamsteadGeneratedAssetRuntimeIdentity ObservedRuntimeIdentity;
	TArray<FString> Codes;
	if (!RuntimeIdentitySource.IsValid())
	{
		RuntimeIdentitySource = MakeShared<FUnavailableRuntimeIdentitySource>();
	}
	const TSharedPtr<const IGloamsteadGeneratedAssetRuntimeIdentitySource> IdentitySource =
		RuntimeIdentitySource;
	const FProviderOperationSnapshot ObservationSnapshot = CaptureOperationSnapshot();
	const bool bObserved = IdentitySource->Observe(ObservedRuntimeIdentity, Codes);
	if (!IsOperationSnapshotCurrent(ObservationSnapshot))
	{
		return false;
	}
	if (!bObserved)
	{
		Codes.AddUnique(TEXT("GAC037"));
	}
	if (!ValidateCurrentOperationAfterBoundary(ObservationSnapshot))
	{
		return false;
	}
	for (const FString& Code : GACValidateActiveBinding(*Catalog, ExpectedBundleId,
		ExpectedReceiptSha256, ExpectedTargetBuildIdentitySha256, ObservedRuntimeIdentity))
	{
		Codes.AddUnique(Code);
	}
	if (Codes.Num() > 0)
	{
		Fail(Codes);
		return false;
	}
	TArray<FGloamsteadGeneratedScriptPackageAuthority> ObservedAuthorities;
	TArray<FString> AuthorityFailures;
	if (!GACDeriveTerminalScriptPackageAuthorities(
		ObservedRuntimeIdentity, ObservedAuthorities, AuthorityFailures))
	{
		Fail({ TEXT("GAC037") });
		return false;
	}
	if (!ValidateCurrentOperationAfterBoundary(ObservationSnapshot))
	{
		return false;
	}
	VerifiedTerminalScriptPackages.Reset();
	for (const FGloamsteadGeneratedScriptPackageAuthority& Authority : ObservedAuthorities)
	{
		VerifiedTerminalScriptPackages.Add(FName(*Authority.PackageName));
	}
	return true;
}

bool UGloamsteadGeneratedAssetMeshForgeProvider::EnsureAcceptedCatalogContractCurrent()
{
	if (bRequiresFreshConfiguration)
	{
		TArray<FString> Codes = FailureCodes;
		Codes.AddUnique(TEXT("GAC039"));
		Fail(Codes);
		return false;
	}
	if (AcceptedCatalogContractSha256.IsEmpty())
	{
		if (State == EGMFGeneratedProviderState::Ready)
		{
			InvalidateAcceptedCatalog({ TEXT("GAC039") });
			return false;
		}
		// Initial loading and ordinary pre-acceptance failures have no accepted contract to compare.
		return true;
	}
	if (!LoadedCatalog)
	{
		InvalidateAcceptedCatalog({ TEXT("GAC039") });
		return false;
	}

	TArray<FString> Codes = GACValidateCatalog(
		*LoadedCatalog, /*bRequireMeshForgeCompatibleClasses*/ false);
	const FString CurrentFingerprint = GACCatalogContractSha256(*LoadedCatalog);
	if (CurrentFingerprint.IsEmpty()
		|| !CurrentFingerprint.Equals(AcceptedCatalogContractSha256, ESearchCase::CaseSensitive))
	{
		Codes.AddUnique(TEXT("GAC039"));
	}
	if (Codes.Num() > 0)
	{
		InvalidateAcceptedCatalog(Codes);
		return false;
	}
	return true;
}

void UGloamsteadGeneratedAssetMeshForgeProvider::InvalidateAcceptedCatalog(
	const TArray<FString>& Codes)
{
	if (LoadedCatalog)
	{
		QuarantineCatalogObjectGeneration(LoadedCatalog);
	}
	const uint64 InvalidationEpoch = ProviderEpoch;
	const uint64 PriorLoadGeneration = LoadGeneration;
	CancelOutstandingPreload();
	if (ProviderEpoch != InvalidationEpoch || LoadGeneration != PriorLoadGeneration + 1)
	{
		return;
	}
	bRequiresFreshConfiguration = true;
	AcceptedCatalogContractSha256.Reset();
	VerifiedTerminalScriptPackages.Reset();
	LoadedCatalog = nullptr;
	TArray<FString> MutationCodes = Codes;
	MutationCodes.AddUnique(TEXT("GAC039"));
	Fail(MutationCodes);
}

bool UGloamsteadGeneratedAssetMeshForgeProvider::IsCatalogObjectGenerationQuarantined(
	const UGloamsteadGeneratedAssetCatalog* Catalog) const
{
	return Catalog && RejectedCatalogObjectGenerations.ContainsByPredicate(
		[Catalog](const TWeakObjectPtr<UGloamsteadGeneratedAssetCatalog>& Rejected)
		{
			return Rejected.IsValid() && Rejected.Get() == Catalog;
		});
}

void UGloamsteadGeneratedAssetMeshForgeProvider::QuarantineCatalogObjectGeneration(
	UGloamsteadGeneratedAssetCatalog* Catalog)
{
	RejectedCatalogObjectGenerations.RemoveAll(
		[](const TWeakObjectPtr<UGloamsteadGeneratedAssetCatalog>& Rejected)
		{
			return !Rejected.IsValid();
		});
	if (Catalog && !IsCatalogObjectGenerationQuarantined(Catalog))
	{
		RejectedCatalogObjectGenerations.Add(Catalog);
	}
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Fail(const TArray<FString>& Codes)
{
	VerifiedTerminalScriptPackages.Reset();
	FailureCodes.Reset();
	for (const FString& Code : Codes)
	{
		FailureCodes.AddUnique(Code);
	}
	State = EGMFGeneratedProviderState::Failed;
}

FGloamsteadMeshForgeProviderDescriptor UGloamsteadGeneratedAssetMeshForgeProvider::GetDescriptor() const
{
	FGloamsteadMeshForgeProviderDescriptor Descriptor;
	Descriptor.ProviderId = TEXT("gmf.gloamstead_generated_catalog.v1");
	Descriptor.ProviderType = EGMFProviderType::GeneratedOwnedMeshForgeAsset;
	Descriptor.OwnershipClass = EGMFOwnershipClass::GeneratedOwned;
	Descriptor.bSupportsRuntimePrimitives = false;
	Descriptor.bSupportsGeneratedAssets = true;
	Descriptor.bCanSpawnHeartProxy = true;
	Descriptor.bCanSpawnRitualPointProxy = true;
	Descriptor.bCanSpawnInteractionRadiusProxy = true;
	Descriptor.bCanSpawnNightFeedbackProxy = true;
	return Descriptor;
}

bool UGloamsteadGeneratedAssetMeshForgeProvider::CanSpawn(EGMFProxyType /*Type*/) const
{
	UGloamsteadGeneratedAssetMeshForgeProvider* MutableThis =
		const_cast<UGloamsteadGeneratedAssetMeshForgeProvider*>(this);
	return MutableThis->EnsureAcceptedCatalogContractCurrent() && IsReadyForBuild();
}

UObject* UGloamsteadGeneratedAssetMeshForgeProvider::ResolveEntryObject(const FSoftObjectPath& ObjectPath)
{
#if WITH_DEV_AUTOMATION_TESTS
	auto ExecuteResolutionCallback = [this, &ObjectPath]()
	{
		if (FSimpleDelegate* Exact = TestObjectResolutionCallbacksByPath.Find(ObjectPath))
		{
			FSimpleDelegate Callback = MoveTemp(*Exact);
			TestObjectResolutionCallbacksByPath.Remove(ObjectPath);
			Callback.ExecuteIfBound();
			return;
		}
		FSimpleDelegate Callback = MoveTemp(TestObjectResolutionCallback);
		TestObjectResolutionCallback.Unbind();
		Callback.ExecuteIfBound();
	};
#endif
#if WITH_DEV_AUTOMATION_TESTS
	if (const TWeakObjectPtr<UObject>* Override = TestResolvedObjects.Find(ObjectPath))
	{
		UObject* Resolved = Override->Get();
		TStrongObjectPtr<UObject> ResolvedGuard(Resolved);
		ExecuteResolutionCallback();
		return ResolvedGuard.Get();
	}
	++TestProductionTryLoadCount;
#endif
	UObject* Resolved = ObjectPath.TryLoad();
	TStrongObjectPtr<UObject> ResolvedGuard(Resolved);
#if WITH_DEV_AUTOMATION_TESTS
	ExecuteResolutionCallback();
#endif
	return ResolvedGuard.Get();
}

FGloamsteadGeneratedAssetObservedProvenance
UGloamsteadGeneratedAssetMeshForgeProvider::ReadObservedProvenance(const FSoftObjectPath& ObjectPath) const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (const FGloamsteadGeneratedAssetObservedProvenance* Override = TestObservedProvenance.Find(ObjectPath))
	{
		return *Override;
	}
#endif
	FGloamsteadGeneratedAssetObservedProvenance Observed;
	const FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	// On-disk/cooked tags are the independent installer-authored evidence. In-memory catalog values cannot
	// satisfy this lookup, so a caller cannot bless stale content by mutating the catalog at runtime.
	const FAssetData AssetData = Module.Get().GetAssetByObjectPath(
		ObjectPath, /*bIncludeOnlyOnDiskAssets*/ true, /*bSkipARFilteredAssets*/ false);
	if (AssetData.IsValid())
	{
		AssetData.GetTagValue(GloamsteadGeneratedAssetProvenanceTags::ObjectSha256, Observed.ObjectSha256);
		AssetData.GetTagValue(GloamsteadGeneratedAssetProvenanceTags::ReceiptSha256, Observed.ReceiptSha256);
		AssetData.GetTagValue(GloamsteadGeneratedAssetProvenanceTags::BundleId, Observed.BundleId);
		AssetData.GetTagValue(GloamsteadGeneratedAssetProvenanceTags::PackageSha256, Observed.PackageSha256);
	}
	return Observed;
}

bool UGloamsteadGeneratedAssetMeshForgeProvider::ReadPackageDependencies(
	FName PackageName, TArray<FName>& OutDependencies) const
{
	OutDependencies.Reset();
#if WITH_DEV_AUTOMATION_TESTS
	if (TestUnavailablePackageDependencyQueries.Contains(PackageName))
	{
		return false;
	}
	if (const TArray<FName>* Override = TestPackageDependencies.Find(PackageName))
	{
		OutDependencies = *Override;
		return true;
	}
#endif
	const FAssetRegistryModule& Module =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	// The empty query includes both Hard and Soft package edges and reads only the on-disk/cooked graph.
	return Module.Get().GetDependencies(
		PackageName,
		OutDependencies,
		UE::AssetRegistry::EDependencyCategory::Package,
		UE::AssetRegistry::FDependencyQuery());
}

TArray<FString> UGloamsteadGeneratedAssetMeshForgeProvider::ValidateCatalogDependencyClosure()
{
	TArray<FString> Codes;
	if (!EnsureAcceptedCatalogContractCurrent())
	{
		return FailureCodes;
	}
	if (!LoadedCatalog)
	{
		Codes.Add(TEXT("GAC017"));
		return Codes;
	}
	if (!IsReadyForBuild())
	{
		for (const FString& FailureCode : FailureCodes)
		{
			Codes.AddUnique(FailureCode);
		}
		if (Codes.Num() == 0)
		{
			Codes.Add(TEXT("GAC037"));
		}
		return Codes;
	}

	TMap<FName, int32> EntryByPackage;
	TMap<FName, int32> ExternalByPackage;
	TSet<FName> AmbiguousPackages;
	for (int32 Index = 0; Index < LoadedCatalog->Entries.Num(); ++Index)
	{
		const FGloamsteadGeneratedAssetEntry& Entry = LoadedCatalog->Entries[Index];
		const FName PackageName(*Entry.Asset.ToSoftObjectPath().GetLongPackageName());
		if (PackageName.IsNone() || EntryByPackage.Contains(PackageName))
		{
			Codes.AddUnique(TEXT("GAC029"));
			AmbiguousPackages.Add(PackageName);
		}
		else
		{
			EntryByPackage.Add(PackageName, Index);
		}

		for (const FString& Code : GACValidateObservedProvenance(
			Entry, *LoadedCatalog, ReadObservedProvenance(Entry.Asset.ToSoftObjectPath())))
		{
			Codes.AddUnique(Code);
		}
	}
	for (int32 Index = 0; Index < LoadedCatalog->ExternalPackageRecords.Num(); ++Index)
	{
		const FGloamsteadGeneratedExternalPackageRecord& Record =
			LoadedCatalog->ExternalPackageRecords[Index];
		const FName PackageName(*Record.PackageName);
		if (PackageName.IsNone() || EntryByPackage.Contains(PackageName)
			|| ExternalByPackage.Contains(PackageName))
		{
			Codes.AddUnique(TEXT("GAC029"));
			AmbiguousPackages.Add(PackageName);
		}
		else
		{
			ExternalByPackage.Add(PackageName, Index);
		}
		for (const FString& Code : GACValidateObservedProvenance(
			Record, *LoadedCatalog, ReadObservedProvenance(Record.ProvenanceObject.ToSoftObjectPath())))
		{
			Codes.AddUnique(Code);
		}
	}

	TMap<FName, uint8> VisitState; // 1 = active recursion stack, 2 = fully visited.
	TFunction<void(FName)> Visit = [&](FName PackageName)
	{
		if (VisitState.FindRef(PackageName) == 1)
		{
			Codes.AddUnique(TEXT("GAC032"));
			return;
		}
		if (VisitState.FindRef(PackageName) == 2)
		{
			return;
		}
		const int32* EntryIndex = EntryByPackage.Find(PackageName);
		const int32* ExternalIndex = ExternalByPackage.Find(PackageName);
		if ((!EntryIndex && !ExternalIndex) || AmbiguousPackages.Contains(PackageName))
		{
			Codes.AddUnique(IsPackageUnderRoot(PackageName.ToString(), GloamGeneratedPackageRoot)
				? TEXT("GAC029") : TEXT("GAC033"));
			return;
		}
		VisitState.Add(PackageName, 1);

		TArray<FName> ObservedDependencies;
		if (!ReadPackageDependencies(PackageName, ObservedDependencies))
		{
			Codes.AddUnique(TEXT("GAC027"));
			VisitState.Add(PackageName, 2);
			return;
		}

		TSet<FName> ObservedDirect;
		for (const FName DependencyPackage : ObservedDependencies)
		{
			ObservedDirect.Add(DependencyPackage);
		}
		TSet<FName> DeclaredDirect;
		const TArray<FString>* DeclaredDependencyNames = nullptr;
		if (EntryIndex)
		{
			DeclaredDependencyNames = &LoadedCatalog->Entries[*EntryIndex].DirectPackageDependencies;
		}
		else
		{
			DeclaredDependencyNames =
				&LoadedCatalog->ExternalPackageRecords[*ExternalIndex].DirectPackageDependencies;
		}
		for (const FString& DeclaredName : *DeclaredDependencyNames)
		{
			DeclaredDirect.Add(FName(*DeclaredName));
		}
		for (const FName Observed : ObservedDirect)
		{
			if (!DeclaredDirect.Contains(Observed))
			{
				Codes.AddUnique(TEXT("GAC030"));
			}
		}
		for (const FName Declared : DeclaredDirect)
		{
			if (!ObservedDirect.Contains(Declared))
			{
				Codes.AddUnique(TEXT("GAC031"));
			}
		}

		for (const FName DependencyPackage : ObservedDirect)
		{
			const FString DependencyName = DependencyPackage.ToString();
			if (DependencyName.StartsWith(TEXT("/Script/"), ESearchCase::CaseSensitive))
			{
				if (VerifiedTerminalScriptPackages.Contains(DependencyPackage))
				{
					continue;
				}
				Codes.AddUnique(TEXT("GAC033"));
				continue;
			}
			if (IsTerminalPlatformPackage(DependencyName,
				LoadedCatalog->TerminalPlatformPackages,
				LoadedCatalog->TerminalPlatformPackageRoots))
			{
				continue;
			}
			if (IsPackageUnderRoot(DependencyName, LoadedCatalog->VersionRoot))
			{
				if (!EntryByPackage.Contains(DependencyPackage)
					|| AmbiguousPackages.Contains(DependencyPackage))
				{
					Codes.AddUnique(TEXT("GAC029"));
				}
				else
				{
					Visit(DependencyPackage);
				}
			}
			else if (IsPackageUnderRoot(DependencyName, GloamGeneratedPackageRoot))
			{
				Codes.AddUnique(TEXT("GAC028"));
			}
			else if (!ExternalByPackage.Contains(DependencyPackage)
				|| AmbiguousPackages.Contains(DependencyPackage))
			{
				Codes.AddUnique(TEXT("GAC033"));
			}
			else
			{
				Visit(DependencyPackage);
			}
		}
		VisitState.Add(PackageName, 2);
	};

	for (const TPair<FName, int32>& Pair : EntryByPackage)
	{
		Visit(Pair.Key);
	}
	// External records are not permitted to hide an unattached, unverified subgraph.
	for (const TPair<FName, int32>& Pair : ExternalByPackage)
	{
		Visit(Pair.Key);
	}
	return Codes;
}

FGloamsteadMeshForgeProxyInstance UGloamsteadGeneratedAssetMeshForgeProvider::CreateProxy(
	const FGloamsteadMeshForgeProxySpec& Spec,
	const FGloamsteadMeshForgeSourceBinding& Binding,
	UWorld* World)
{
	FGloamsteadMeshForgeProxyInstance Instance;
	Instance.Spec = Spec;
	Instance.Binding = Binding;
	Instance.ProviderType = EGMFProviderType::GeneratedOwnedMeshForgeAsset;
	Instance.OwnershipClass = EGMFOwnershipClass::GeneratedOwned;
	Instance.bRuntimeOnly = false;

	if (!EnsureAcceptedCatalogContractCurrent() || !IsReadyForBuild() || !LoadedCatalog)
	{
		Instance.FailureCodes = FailureCodes;
		if (Instance.FailureCodes.Num() == 0) { Instance.FailureCodes.Add(TEXT("GAC017")); }
		return Instance;
	}
	TStrongObjectPtr<UGloamsteadGeneratedAssetCatalog> CatalogGuard(LoadedCatalog.Get());
	UGloamsteadGeneratedAssetCatalog* const Catalog = CatalogGuard.Get();
	const FProviderOperationSnapshot OperationSnapshot = CaptureOperationSnapshot();
	auto RejectStaleOperation = [&]()
	{
		if (ValidateCurrentOperationAfterBoundary(OperationSnapshot))
		{
			return false;
		}
		Instance.FailureCodes.AddUnique(TEXT("GAC039"));
		return true;
	};

	const FGloamsteadGeneratedAssetEntry* Entry = Catalog->FindExact(
		Spec.GeneratedAssetRole, Spec.GeneratedAssetState);
	if (!Entry)
	{
		Instance.FailureCodes.Add(TEXT("GAC016"));
		return Instance;
	}
	// Reentrant asset loads may invalidate LoadedCatalog. Retain only value snapshots across those boundaries.
	const FGloamsteadGeneratedAssetEntry SelectedEntry = *Entry;

	Instance.GeneratedAssetPath = SelectedEntry.Asset.ToSoftObjectPath().ToString();
	Instance.GeneratedVersionRoot = Catalog->VersionRoot;
	Instance.GeneratedBundleId = Catalog->BundleId;
	Instance.GeneratedReceiptSha256 = Catalog->ReceiptSha256;
	Instance.GeneratedObjectSha256 = SelectedEntry.ObjectSha256;
	Instance.GeneratedOwnershipId = SelectedEntry.OwnershipId;
	Instance.GeneratedLicenseId = SelectedEntry.LicenseId;
	if (!World || !Binding.bLocationResolved)
	{
		Instance.FailureCodes.Add(TEXT("GAC025"));
		return Instance;
	}
	TStrongObjectPtr<UWorld> WorldGuard(World);
	Instance.FailureCodes = ValidateCatalogDependencyClosure();
	if (RejectStaleOperation())
	{
		return Instance;
	}
	if (Instance.FailureCodes.Num() > 0)
	{
		return Instance;
	}

	UObject* LoadedObject = ResolveEntryObject(SelectedEntry.Asset.ToSoftObjectPath());
	if (RejectStaleOperation())
	{
		return Instance;
	}
	if (!LoadedObject)
	{
		Instance.FailureCodes.Add(TEXT("GAC017"));
		return Instance;
	}
	TStrongObjectPtr<UObject> LoadedObjectGuard(LoadedObject);
	for (const TSoftObjectPtr<UObject>& Dependency : SelectedEntry.Dependencies)
	{
		TOptional<FGloamsteadGeneratedAssetEntry> DependencyEntry;
		for (const FGloamsteadGeneratedAssetEntry& Candidate : Catalog->Entries)
		{
			if (Candidate.Asset.ToSoftObjectPath() == Dependency.ToSoftObjectPath())
			{
				DependencyEntry = Candidate;
				break;
			}
		}
		UObject* DependencyObject = ResolveEntryObject(Dependency.ToSoftObjectPath());
		TStrongObjectPtr<UObject> DependencyObjectGuard(DependencyObject);
		DependencyObject = DependencyObjectGuard.Get();
		if (RejectStaleOperation())
		{
			return Instance;
		}
		if (!DependencyEntry.IsSet() || !DependencyObject)
		{
			Instance.FailureCodes.AddUnique(TEXT("GAC017"));
			return Instance;
		}
		for (const FString& Code : GACValidateLoadedObject(DependencyEntry.GetValue(), DependencyObject, false))
		{
			Instance.FailureCodes.AddUnique(Code);
		}
		for (const FString& Code : GACValidateObservedProvenance(
			DependencyEntry.GetValue(), *Catalog, ReadObservedProvenance(Dependency.ToSoftObjectPath())))
		{
			Instance.FailureCodes.AddUnique(Code);
		}
		if (RejectStaleOperation())
		{
			return Instance;
		}
		if (Instance.FailureCodes.Num() > 0) { return Instance; }
	}
	UClass* ExpectedClass = SelectedEntry.ExpectedClass.LoadSynchronous();
	TStrongObjectPtr<UClass> ExpectedClassGuard(ExpectedClass);
#if WITH_DEV_AUTOMATION_TESTS
	++TestExpectedClassLoadCount;
	{
		FSimpleDelegate Callback = MoveTemp(TestExpectedClassLoadCallback);
		TestExpectedClassLoadCallback.Unbind();
		Callback.ExecuteIfBound();
	}
#endif
	if (RejectStaleOperation())
	{
		return Instance;
	}
	if (!ExpectedClass)
	{
		Instance.FailureCodes.Add(TEXT("GAC017"));
		return Instance;
	}
	Instance.FailureCodes = GACValidateLoadedObject(SelectedEntry, LoadedObject,
		/*bRequireStaticMeshForProvider*/ true);
	if (Instance.FailureCodes.Num() > 0)
	{
		return Instance;
	}
	Instance.FailureCodes = GACValidateObservedProvenance(
		SelectedEntry, *Catalog, ReadObservedProvenance(SelectedEntry.Asset.ToSoftObjectPath()));
	if (RejectStaleOperation())
	{
		return Instance;
	}
	if (Instance.FailureCodes.Num() > 0)
	{
		return Instance;
	}
	UStaticMesh* Mesh = CastChecked<UStaticMesh>(LoadedObject);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGloamsteadMeshForgeProxyActor* Actor = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	if (!bTestForceSpawnFailure)
#endif
	{
		Actor = WorldGuard->SpawnActor<AGloamsteadMeshForgeProxyActor>(
			AGloamsteadMeshForgeProxyActor::StaticClass(), Binding.WorldLocation, FRotator::ZeroRotator, Params);
	}
#if WITH_DEV_AUTOMATION_TESTS
	{
		TFunction<void(AGloamsteadMeshForgeProxyActor*)> Callback = MoveTemp(TestActorSpawnCallback);
		TestActorSpawnCallback = nullptr;
		if (Callback)
		{
			Callback(Actor);
		}
	}
#endif
	if (RejectStaleOperation())
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
		Instance.SpawnedActor = nullptr;
		Instance.bSpawned = false;
		Instance.bVisibleProxyCreated = false;
		return Instance;
	}
	if (Actor)
	{
		Actor->ConfigureGeneratedVisual(Mesh, Spec.Color,
			UGloamsteadEnginePrimitiveMeshForgeProvider::ScaleForType(Spec.ProxyType, Spec.Scale), true);
		Actor->SetProjectedMaterialParameters(Spec.ProjectedWetness, !Spec.ProjectedWarningTag.IsNone());
		Instance.SpawnedActor = Actor;
		Instance.bSpawned = true;
		Instance.bVisibleProxyCreated = Actor->HasVisibleMesh();
	}
	if (!Actor || !Instance.bVisibleProxyCreated)
	{
		Instance.FailureCodes.Add(TEXT("GAC026"));
	}
	return Instance;
}

#if WITH_DEV_AUTOMATION_TESTS
void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetLoadedCatalog(
	UGloamsteadGeneratedAssetCatalog* Catalog,
	const FString& InExpectedBundleId,
	const FString& InExpectedReceiptSha256,
	const FGloamsteadGeneratedAssetRuntimeIdentity& ObservedRuntimeIdentity,
	FSimpleDelegate ObservationCallback)
{
	AdvanceProviderEpoch();
	if (bRequiresFreshConfiguration)
	{
		TArray<FString> Codes = FailureCodes;
		Codes.AddUnique(TEXT("GAC039"));
		Fail(Codes);
		return;
	}
	Test_SetObservedRuntimeIdentity(ObservedRuntimeIdentity, MoveTemp(ObservationCallback));
	LoadedCatalog = Catalog;
	ExpectedBundleId = InExpectedBundleId;
	ExpectedReceiptSha256 = InExpectedReceiptSha256;
	ExpectedTargetBuildIdentitySha256 = Catalog ? Catalog->TargetBuildIdentitySha256 : FString();
	FailureCodes.Reset();
	State = EGMFGeneratedProviderState::Uninitialized;
	ValidateLoadedCatalog();
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetObservedRuntimeIdentity(
	const FGloamsteadGeneratedAssetRuntimeIdentity& ObservedRuntimeIdentity,
	FSimpleDelegate ObservationCallback)
{
	AdvanceProviderEpoch();
	RuntimeIdentitySource = MakeShared<FFixedRuntimeIdentitySource>(
		ObservedRuntimeIdentity, MoveTemp(ObservationCallback));
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetResolvedObject(
	const FSoftObjectPath& ObjectPath, UObject* Object)
{
	TestResolvedObjects.Add(ObjectPath, Object);
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetObservedProvenance(
	const FSoftObjectPath& ObjectPath,
	const FGloamsteadGeneratedAssetObservedProvenance& Provenance)
{
	TestObservedProvenance.Add(ObjectPath, Provenance);
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetPackageDependencies(
	const FSoftObjectPath& ObjectPath,
	const TArray<FName>& DependencyPackages)
{
	TestPackageDependencies.Add(FName(*ObjectPath.GetLongPackageName()), DependencyPackages);
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_MarkPackageDependencyQueryUnavailable(
	const FString& PackageName)
{
	TestUnavailablePackageDependencyQueries.Add(FName(*PackageName));
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetObjectResolutionCallback(
	FSimpleDelegate Callback)
{
	TestObjectResolutionCallback = MoveTemp(Callback);
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetObjectResolutionCallbackForPath(
	const FSoftObjectPath& ObjectPath,
	FSimpleDelegate Callback)
{
	TestObjectResolutionCallbacksByPath.Add(ObjectPath, MoveTemp(Callback));
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetExpectedClassLoadCallback(
	FSimpleDelegate Callback)
{
	TestExpectedClassLoadCallback = MoveTemp(Callback);
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetActorSpawnCallback(
	TFunction<void(AGloamsteadMeshForgeProxyActor*)> Callback)
{
	TestActorSpawnCallback = MoveTemp(Callback);
}

TArray<FString> UGloamsteadGeneratedAssetMeshForgeProvider::Test_ValidateDependencyClosure()
{
	return ValidateCatalogDependencyClosure();
}

uint64 UGloamsteadGeneratedAssetMeshForgeProvider::Test_BeginPendingCatalogLoad()
{
	AdvanceProviderEpoch();
	CancelOutstandingPreload();
	State = EGMFGeneratedProviderState::Loading;
	return LoadGeneration;
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_CompleteCatalogLoad(
	uint64 InLoadGeneration,
	const FSoftObjectPath& RequestedPath,
	UGloamsteadGeneratedAssetCatalog* Catalog,
	FSimpleDelegate Completion)
{
	FGloamsteadGeneratedCatalogLoadCompletion TypedCompletion;
	if (Completion.IsBound())
	{
		TypedCompletion = FGloamsteadGeneratedCatalogLoadCompletion::CreateLambda(
			[Completion = MoveTemp(Completion)](
				const FGloamsteadGeneratedCatalogLoadResult& Result) mutable
			{
				if (Result.Terminal == EGMFGeneratedCatalogLoadTerminal::Accepted
					|| Result.Terminal == EGMFGeneratedCatalogLoadTerminal::Rejected)
				{
					Completion.ExecuteIfBound();
				}
			});
	}
	AcceptCatalogLoad(InLoadGeneration, RequestedPath, Catalog, MoveTemp(TypedCompletion));
}
#endif
