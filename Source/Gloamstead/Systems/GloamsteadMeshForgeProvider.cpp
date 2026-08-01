#include "Systems/GloamsteadMeshForgeProvider.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Data/GloamsteadGeneratedAssetCatalog.h"
#include "Settings/GloamsteadGeneratedAssetSettings.h"

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

void UGloamsteadGeneratedAssetMeshForgeProvider::Configure(const UGloamsteadGeneratedAssetSettings& Settings)
{
	CatalogPath = Settings.Catalog;
	ExpectedBundleId = Settings.ExpectedActiveBundleId;
	ExpectedReceiptSha256 = Settings.ExpectedReceiptSha256;
	LoadedCatalog = nullptr;
	FailureCodes.Reset();
	State = EGMFGeneratedProviderState::Uninitialized;
}

void UGloamsteadGeneratedAssetMeshForgeProvider::PreloadCatalogAsync(FSimpleDelegate Completion)
{
	PreloadCompletion = MoveTemp(Completion);
	FailureCodes.Reset();
	LoadedCatalog = nullptr;
	if (CatalogPath.IsNull())
	{
		Fail({ TEXT("GAC017") });
		PreloadCompletion.ExecuteIfBound();
		return;
	}

	State = EGMFGeneratedProviderState::Loading;
	PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		CatalogPath.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UGloamsteadGeneratedAssetMeshForgeProvider::FinishCatalogLoad));
	if (!PreloadHandle.IsValid())
	{
		Fail({ TEXT("GAC017") });
		PreloadCompletion.ExecuteIfBound();
	}
}

void UGloamsteadGeneratedAssetMeshForgeProvider::FinishCatalogLoad()
{
	LoadedCatalog = CatalogPath.Get();
	if (!LoadedCatalog)
	{
		Fail({ TEXT("GAC017") });
	}
	else
	{
		ValidateLoadedCatalog();
	}
	PreloadCompletion.ExecuteIfBound();
}

void UGloamsteadGeneratedAssetMeshForgeProvider::ValidateLoadedCatalog()
{
	if (!LoadedCatalog)
	{
		Fail({ TEXT("GAC017") });
		return;
	}

	// The catalog is intentionally generic and may also carry placement/material/VFX entries. Selection
	// enforces the provider's narrower static-mesh support on the exact chosen entry before spawning.
	TArray<FString> Codes = GACValidateCatalog(*LoadedCatalog, /*bRequireMeshForgeCompatibleClasses*/ false);
	for (const FString& Code : GACValidateActiveBinding(*LoadedCatalog, ExpectedBundleId, ExpectedReceiptSha256))
	{
		Codes.AddUnique(Code);
	}
	if (Codes.Num() > 0)
	{
		Fail(Codes);
		return;
	}

	FailureCodes.Reset();
	State = EGMFGeneratedProviderState::Ready;
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Fail(const TArray<FString>& Codes)
{
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
	return IsReadyForBuild();
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

	if (!IsReadyForBuild() || !LoadedCatalog)
	{
		Instance.FailureCodes = FailureCodes;
		if (Instance.FailureCodes.Num() == 0) { Instance.FailureCodes.Add(TEXT("GAC017")); }
		return Instance;
	}

	const FGloamsteadGeneratedAssetEntry* Entry = LoadedCatalog->FindExact(
		Spec.GeneratedAssetRole, Spec.GeneratedAssetState);
	if (!Entry)
	{
		Instance.FailureCodes.Add(TEXT("GAC016"));
		return Instance;
	}

	Instance.GeneratedAssetPath = Entry->Asset.ToSoftObjectPath().ToString();
	Instance.GeneratedVersionRoot = LoadedCatalog->VersionRoot;
	Instance.GeneratedBundleId = LoadedCatalog->BundleId;
	Instance.GeneratedReceiptSha256 = LoadedCatalog->ReceiptSha256;
	Instance.GeneratedObjectSha256 = Entry->ObjectSha256;
	Instance.GeneratedOwnershipId = Entry->OwnershipId;
	Instance.GeneratedLicenseId = Entry->LicenseId;

	UObject* LoadedObject = Entry->Asset.LoadSynchronous();
	if (!LoadedObject)
	{
		Instance.FailureCodes.Add(TEXT("GAC017"));
		return Instance;
	}
	for (const TSoftObjectPtr<UObject>& Dependency : Entry->Dependencies)
	{
		if (!Dependency.LoadSynchronous())
		{
			Instance.FailureCodes.AddUnique(TEXT("GAC017"));
			return Instance;
		}
	}
	UClass* ExpectedClass = Entry->ExpectedClass.LoadSynchronous();
	if (!ExpectedClass)
	{
		Instance.FailureCodes.Add(TEXT("GAC017"));
		return Instance;
	}
	Instance.FailureCodes = GACValidateLoadedObject(*Entry, LoadedObject,
		/*bRequireStaticMeshForProvider*/ true);
	if (Instance.FailureCodes.Num() > 0)
	{
		return Instance;
	}
	UStaticMesh* Mesh = CastChecked<UStaticMesh>(LoadedObject);
	if (!World || !Binding.bLocationResolved)
	{
		return Instance;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGloamsteadMeshForgeProxyActor* Actor = World->SpawnActor<AGloamsteadMeshForgeProxyActor>(
		AGloamsteadMeshForgeProxyActor::StaticClass(), Binding.WorldLocation, FRotator::ZeroRotator, Params);
	if (Actor)
	{
		Actor->ConfigureGeneratedVisual(Mesh, Spec.Color,
			UGloamsteadEnginePrimitiveMeshForgeProvider::ScaleForType(Spec.ProxyType, Spec.Scale), true);
		Actor->SetProjectedMaterialParameters(Spec.ProjectedWetness, !Spec.ProjectedWarningTag.IsNone());
		Instance.SpawnedActor = Actor;
		Instance.bSpawned = true;
		Instance.bVisibleProxyCreated = Actor->HasVisibleMesh();
	}
	return Instance;
}

#if WITH_DEV_AUTOMATION_TESTS
void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetLoadedCatalog(
	UGloamsteadGeneratedAssetCatalog* Catalog,
	const FString& InExpectedBundleId,
	const FString& InExpectedReceiptSha256)
{
	LoadedCatalog = Catalog;
	ExpectedBundleId = InExpectedBundleId;
	ExpectedReceiptSha256 = InExpectedReceiptSha256;
	FailureCodes.Reset();
	State = EGMFGeneratedProviderState::Uninitialized;
	ValidateLoadedCatalog();
}
#endif
