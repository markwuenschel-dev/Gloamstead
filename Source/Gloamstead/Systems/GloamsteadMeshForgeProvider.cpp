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
		explicit FFixedRuntimeIdentitySource(const FGloamsteadGeneratedAssetRuntimeIdentity& InIdentity)
			: Identity(InIdentity) {}

		virtual bool Observe(
			FGloamsteadGeneratedAssetRuntimeIdentity& OutIdentity,
			TArray<FString>& OutFailureCodes) const override
		{
			OutIdentity = Identity;
			OutFailureCodes.Reset();
			return true;
		}

	private:
		FGloamsteadGeneratedAssetRuntimeIdentity Identity;
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

void UGloamsteadGeneratedAssetMeshForgeProvider::Configure(const UGloamsteadGeneratedAssetSettings& Settings)
{
	CancelOutstandingPreload();
	// Configuration is a production lifecycle boundary. A development automation observer is valid
	// only for the explicit test operation that installed it and must never survive reuse of this UObject.
	RuntimeIdentitySource = MakeShared<FUnavailableRuntimeIdentitySource>();
	CatalogPath = Settings.Catalog;
	ExpectedBundleId = Settings.ExpectedActiveBundleId;
	ExpectedReceiptSha256 = Settings.ExpectedReceiptSha256;
	ExpectedTargetBuildIdentitySha256 = Settings.ExpectedTargetBuildIdentitySha256;
	LoadedCatalog = nullptr;
	FailureCodes.Reset();
	State = EGMFGeneratedProviderState::Uninitialized;
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Deactivate()
{
	CancelOutstandingPreload();
	RuntimeIdentitySource = MakeShared<FUnavailableRuntimeIdentitySource>();
	LoadedCatalog = nullptr;
	FailureCodes.Reset();
	State = EGMFGeneratedProviderState::Uninitialized;
}

void UGloamsteadGeneratedAssetMeshForgeProvider::PreloadCatalogAsync(FSimpleDelegate Completion)
{
	CancelOutstandingPreload();
	FailureCodes.Reset();
	LoadedCatalog = nullptr;
	if (CatalogPath.IsNull())
	{
		Fail({ TEXT("GAC017") });
		Completion.ExecuteIfBound();
		return;
	}

	State = EGMFGeneratedProviderState::Loading;
	const uint64 RequestGeneration = LoadGeneration;
	const FSoftObjectPath RequestedPath = CatalogPath.ToSoftObjectPath();
	PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		RequestedPath,
		FStreamableDelegate::CreateWeakLambda(this,
			[this, RequestGeneration, RequestedPath, Completion]() mutable
			{
				FinishCatalogLoad(RequestGeneration, RequestedPath, MoveTemp(Completion));
			}));
	if (!PreloadHandle.IsValid())
	{
		Fail({ TEXT("GAC017") });
		Completion.ExecuteIfBound();
	}
}

void UGloamsteadGeneratedAssetMeshForgeProvider::CancelOutstandingPreload()
{
	++LoadGeneration;
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle->ReleaseHandle();
		PreloadHandle.Reset();
	}
}

void UGloamsteadGeneratedAssetMeshForgeProvider::FinishCatalogLoad(
	uint64 InLoadGeneration, FSoftObjectPath RequestedPath, FSimpleDelegate Completion)
{
	UGloamsteadGeneratedAssetCatalog* Catalog = Cast<UGloamsteadGeneratedAssetCatalog>(RequestedPath.ResolveObject());
	AcceptCatalogLoad(InLoadGeneration, RequestedPath, Catalog, MoveTemp(Completion));
}

void UGloamsteadGeneratedAssetMeshForgeProvider::AcceptCatalogLoad(
	uint64 InLoadGeneration,
	const FSoftObjectPath& RequestedPath,
	UGloamsteadGeneratedAssetCatalog* Catalog,
	FSimpleDelegate Completion)
{
	if (InLoadGeneration != LoadGeneration || RequestedPath != CatalogPath.ToSoftObjectPath())
	{
		return;
	}
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->ReleaseHandle();
		PreloadHandle.Reset();
	}
	LoadedCatalog = Catalog;
	if (!LoadedCatalog) { Fail({ TEXT("GAC017") }); }
	else { ValidateLoadedCatalog(); }
	Completion.ExecuteIfBound();
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
	FGloamsteadGeneratedAssetRuntimeIdentity ObservedRuntimeIdentity;
	TArray<FString> ObservationFailures;
	if (!RuntimeIdentitySource.IsValid())
	{
		RuntimeIdentitySource = MakeShared<FUnavailableRuntimeIdentitySource>();
	}
	if (!RuntimeIdentitySource->Observe(ObservedRuntimeIdentity, ObservationFailures))
	{
		ObservationFailures.AddUnique(TEXT("GAC037"));
	}
	for (const FString& Code : ObservationFailures)
	{
		Codes.AddUnique(Code);
	}
	for (const FString& Code : GACValidateActiveBinding(*LoadedCatalog, ExpectedBundleId,
		ExpectedReceiptSha256, ExpectedTargetBuildIdentitySha256, ObservedRuntimeIdentity))
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

bool UGloamsteadGeneratedAssetMeshForgeProvider::RevalidateRuntimeIdentity()
{
	if (!LoadedCatalog)
	{
		Fail({ TEXT("GAC017") });
		return false;
	}
	FGloamsteadGeneratedAssetRuntimeIdentity ObservedRuntimeIdentity;
	TArray<FString> Codes;
	if (!RuntimeIdentitySource.IsValid())
	{
		RuntimeIdentitySource = MakeShared<FUnavailableRuntimeIdentitySource>();
	}
	if (!RuntimeIdentitySource->Observe(ObservedRuntimeIdentity, Codes))
	{
		Codes.AddUnique(TEXT("GAC037"));
	}
	for (const FString& Code : GACValidateActiveBinding(*LoadedCatalog, ExpectedBundleId,
		ExpectedReceiptSha256, ExpectedTargetBuildIdentitySha256, ObservedRuntimeIdentity))
	{
		Codes.AddUnique(Code);
	}
	if (Codes.Num() > 0)
	{
		Fail(Codes);
		return false;
	}
	return true;
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

UObject* UGloamsteadGeneratedAssetMeshForgeProvider::ResolveEntryObject(const FSoftObjectPath& ObjectPath) const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (const TWeakObjectPtr<UObject>* Override = TestResolvedObjects.Find(ObjectPath))
	{
		return Override->Get();
	}
#endif
	return ObjectPath.TryLoad();
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

TArray<FString> UGloamsteadGeneratedAssetMeshForgeProvider::ValidateCatalogDependencyClosure() const
{
	TArray<FString> Codes;
	if (!LoadedCatalog)
	{
		Codes.Add(TEXT("GAC017"));
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
	if (!World || !Binding.bLocationResolved)
	{
		Instance.FailureCodes.Add(TEXT("GAC025"));
		return Instance;
	}
	Instance.FailureCodes = ValidateCatalogDependencyClosure();
	if (Instance.FailureCodes.Num() > 0)
	{
		return Instance;
	}

	UObject* LoadedObject = ResolveEntryObject(Entry->Asset.ToSoftObjectPath());
	if (!LoadedObject)
	{
		Instance.FailureCodes.Add(TEXT("GAC017"));
		return Instance;
	}
	for (const TSoftObjectPtr<UObject>& Dependency : Entry->Dependencies)
	{
		const FGloamsteadGeneratedAssetEntry* DependencyEntry = nullptr;
		for (const FGloamsteadGeneratedAssetEntry& Candidate : LoadedCatalog->Entries)
		{
			if (Candidate.Asset.ToSoftObjectPath() == Dependency.ToSoftObjectPath())
			{
				DependencyEntry = &Candidate;
				break;
			}
		}
		UObject* DependencyObject = ResolveEntryObject(Dependency.ToSoftObjectPath());
		if (!DependencyEntry || !DependencyObject)
		{
			Instance.FailureCodes.AddUnique(TEXT("GAC017"));
			return Instance;
		}
		for (const FString& Code : GACValidateLoadedObject(*DependencyEntry, DependencyObject, false))
		{
			Instance.FailureCodes.AddUnique(Code);
		}
		for (const FString& Code : GACValidateObservedProvenance(
			*DependencyEntry, *LoadedCatalog, ReadObservedProvenance(Dependency.ToSoftObjectPath())))
		{
			Instance.FailureCodes.AddUnique(Code);
		}
		if (Instance.FailureCodes.Num() > 0) { return Instance; }
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
	Instance.FailureCodes = GACValidateObservedProvenance(
		*Entry, *LoadedCatalog, ReadObservedProvenance(Entry->Asset.ToSoftObjectPath()));
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
		Actor = World->SpawnActor<AGloamsteadMeshForgeProxyActor>(
			AGloamsteadMeshForgeProxyActor::StaticClass(), Binding.WorldLocation, FRotator::ZeroRotator, Params);
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
	const FGloamsteadGeneratedAssetRuntimeIdentity& ObservedRuntimeIdentity)
{
	Test_SetObservedRuntimeIdentity(ObservedRuntimeIdentity);
	LoadedCatalog = Catalog;
	ExpectedBundleId = InExpectedBundleId;
	ExpectedReceiptSha256 = InExpectedReceiptSha256;
	ExpectedTargetBuildIdentitySha256 = Catalog ? Catalog->TargetBuildIdentitySha256 : FString();
	FailureCodes.Reset();
	State = EGMFGeneratedProviderState::Uninitialized;
	ValidateLoadedCatalog();
}

void UGloamsteadGeneratedAssetMeshForgeProvider::Test_SetObservedRuntimeIdentity(
	const FGloamsteadGeneratedAssetRuntimeIdentity& ObservedRuntimeIdentity)
{
	RuntimeIdentitySource = MakeShared<FFixedRuntimeIdentitySource>(ObservedRuntimeIdentity);
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

TArray<FString> UGloamsteadGeneratedAssetMeshForgeProvider::Test_ValidateDependencyClosure() const
{
	return ValidateCatalogDependencyClosure();
}

uint64 UGloamsteadGeneratedAssetMeshForgeProvider::Test_BeginPendingCatalogLoad()
{
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
	AcceptCatalogLoad(InLoadGeneration, RequestedPath, Catalog, MoveTemp(Completion));
}
#endif
