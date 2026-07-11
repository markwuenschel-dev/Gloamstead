#include "Systems/NightPressureActor.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Engine/World.h"

ANightPressureActor::ANightPressureActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

float ANightPressureActor::ComputeMenaceFromLight(float AverageLight)
{
	return FMath::Clamp(1.f - AverageLight, 0.f, 1.f);
}

void ANightPressureActor::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		CachedPCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	}
}

void ANightPressureActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!CachedPCG)
	{
		return;
	}

	const float NewMenace = ComputeMenaceFromLight(CachedPCG->GetSanctuaryAverageLightLevel());
	if (!FMath::IsNearlyEqual(NewMenace, CurrentMenace, 0.01f))
	{
		CurrentMenace = NewMenace;
		OnMenaceChanged(NewMenace);
	}
}
