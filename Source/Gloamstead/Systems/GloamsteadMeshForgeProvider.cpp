#include "Systems/GloamsteadMeshForgeProvider.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

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

void AGloamsteadMeshForgeProxyActor::SetVisualColor(const FLinearColor& Color, bool bEmissive)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}
	if (!DynMaterial)
	{
		UMaterialInterface* Base = MeshComponent->GetMaterial(0);
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
		// Best-effort across engine basic-shape materials — unknown params are ignored, the mesh stays visible.
		DynMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		DynMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
		DynMaterial->SetVectorParameterValue(TEXT("Emissive"), bEmissive ? (Color * 3.0f) : FLinearColor::Black);
		DynMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), bEmissive ? 3.0f : 0.0f);
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
