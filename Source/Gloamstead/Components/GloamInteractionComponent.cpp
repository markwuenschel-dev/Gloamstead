// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/GloamInteractionComponent.h"
#include "Interfaces/GloamInteractable.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h" // FOverlapResult (full definition)
#include "CollisionQueryParams.h" // FCollisionQueryParams, FCollisionObjectQueryParams
#include "WorldCollision.h"        // FCollisionShape

UGloamInteractionComponent::UGloamInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UGloamInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceLastUpdate += DeltaTime;
	if (TimeSinceLastUpdate < UpdateInterval)
	{
		return;
	}
	TimeSinceLastUpdate = 0.0f;

	UpdateFocus();
}

void UGloamInteractionComponent::UpdateFocus()
{
	const UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		SetFocusedActor(nullptr);
		return;
	}

	// View point: a controlled pawn returns its controller's eyes; otherwise the actor transform.
	FVector ViewLocation;
	FRotator ViewRotation;
	Owner->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();

	// Gather nearby actors that implement the interface (broad sphere, then the pure cone/range filter).
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(FName(TEXT("GloamInteractionGather")), /*bTraceComplex=*/false, Owner);
	const FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllObjects);
	World->OverlapMultiByObjectType(
		Overlaps, ViewLocation, FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(DetectionRadius), QueryParams);

	TArray<AActor*> Candidates;
	TArray<FVector> Locations;
	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Actor = Result.GetActor();
		if (!Actor || Candidates.Contains(Actor) || !Actor->Implements<UGloamInteractable>())
		{
			continue;
		}
		Candidates.Add(Actor);
		Locations.Add(Actor->GetActorLocation());
	}

	const int32 Best = FindBestInteractableIndex(Locations, ViewLocation, ViewDirection, InteractionRange, MinViewConeDot);
	SetFocusedActor(Best == INDEX_NONE ? nullptr : Candidates[Best]);
}

void UGloamInteractionComponent::SetFocusedActor(AActor* NewFocus)
{
	if (FocusedActor.Get() == NewFocus)
	{
		return;
	}
	FocusedActor = NewFocus;
	OnFocusedInteractableChanged.Broadcast(NewFocus);
}

bool UGloamInteractionComponent::TryInteract()
{
	AActor* Focus = FocusedActor.Get();
	if (!Focus || !Focus->Implements<UGloamInteractable>())
	{
		return false;
	}
	if (!IGloamInteractable::Execute_CanInteract(Focus, GetOwner()))
	{
		return false;
	}
	IGloamInteractable::Execute_Interact(Focus, GetOwner());
	return true;
}

bool UGloamInteractionComponent::TryExamine()
{
	AActor* Focus = FocusedActor.Get();
	if (!Focus || !Focus->Implements<UGloamInteractable>())
	{
		return false;
	}
	IGloamInteractable::Execute_Examine(Focus, GetOwner());
	return true;
}

FText UGloamInteractionComponent::GetCurrentPrompt() const
{
	AActor* Focus = FocusedActor.Get();
	if (!Focus || !Focus->Implements<UGloamInteractable>())
	{
		return FText::GetEmpty();
	}
	if (!IGloamInteractable::Execute_CanInteract(Focus, GetOwner()))
	{
		return FText::GetEmpty();
	}
	return IGloamInteractable::Execute_GetInteractionPrompt(Focus);
}

int32 UGloamInteractionComponent::FindBestInteractableIndex(
	const TArray<FVector>& CandidateLocations,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	float MaxRange,
	float MinViewDot)
{
	const FVector ViewDir = ViewDirection.GetSafeNormal();
	const float MaxRangeSq = MaxRange * MaxRange;

	int32 BestIndex = INDEX_NONE;
	float BestDot = -1.0f;
	float BestDistSq = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < CandidateLocations.Num(); ++Index)
	{
		const FVector ToCandidate = CandidateLocations[Index] - ViewLocation;
		const float DistSq = ToCandidate.SizeSquared();
		if (DistSq > MaxRangeSq || DistSq <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Dot = FVector::DotProduct(ToCandidate.GetSafeNormal(), ViewDir);
		if (Dot < MinViewDot)
		{
			continue;
		}

		// Prefer the most-aligned candidate; break ties by the nearer one.
		if (Dot > BestDot + KINDA_SMALL_NUMBER ||
			(FMath::IsNearlyEqual(Dot, BestDot) && DistSq < BestDistSq))
		{
			BestIndex = Index;
			BestDot = Dot;
			BestDistSq = DistSq;
		}
	}

	return BestIndex;
}
