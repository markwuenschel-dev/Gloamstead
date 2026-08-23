#include "Actors/GloamsteadRestoredGardenBed.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AGloamsteadRestoredGardenBed::AGloamsteadRestoredGardenBed()
{
	PrimaryActorTick.bCanEverTick = false;

	GardenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GardenBedMesh"));
	SetRootComponent(GardenMesh);
	GardenMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GardenMesh->SetMobility(EComponentMobility::Movable);
	GardenMesh->SetWorldScale3D(FVector(0.72f, 0.50f, 0.18f));

	// These are tracked, project-owned sanctuary kit resources. The low slab
	// plus soil material reads as a tended bed now; WorldForge Task 8 owns the
	// richer state-aware foliage and surrounding environmental transformation.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GardenBedMeshFinder(
		TEXT("/Game/Gloamstead/Kit/Meshes/SM_Slab_Floor_A.SM_Slab_Floor_A"));
	if (GardenBedMeshFinder.Succeeded())
	{
		GardenMesh->SetStaticMesh(GardenBedMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GardenSoilMaterialFinder(
		TEXT("/Game/Gloamstead/Kit/Materials/MI_Sanctuary_Soil.MI_Sanctuary_Soil"));
	if (GardenSoilMaterialFinder.Succeeded())
	{
		GardenMesh->SetMaterial(0, GardenSoilMaterialFinder.Object);
	}
}

bool AGloamsteadRestoredGardenBed::HasVisibleGardenMesh() const
{
	return GardenMesh && GardenMesh->GetStaticMesh() != nullptr;
}
