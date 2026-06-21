// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GloamInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGloamFocusedInteractableChanged, AActor*, NewFocus);

/**
 *  Tracks which IGloamInteractable the player is currently looking at and routes the interaction
 *  verbs to it. Lives on the player pawn alongside URitualPlacementComponent (which owns ritual-point
 *  restoration; this component owns discrete world objects — the Veil Heart rest point, examinables).
 *
 *  Design seam: the focus *decision* is the pure static FindBestInteractableIndex (scored by view-cone
 *  alignment then proximity), kept free of any world/trace dependency so it is exhaustively unit-testable.
 *  The component tick is a thin shell that gathers nearby interactables from the world and feeds that
 *  pure function. Verb dispatch goes through the IGloamInteractable BlueprintNativeEvents, so C++ and
 *  Blueprint interactables are driven identically.
 */
UCLASS(ClassGroup = (Gloamstead), meta = (BlueprintSpawnableComponent))
class GLOAMSTEAD_API UGloamInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGloamInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Fires when the focused interactable changes (including to nullptr when focus is lost). */
	UPROPERTY(BlueprintAssignable, Category = "Gloam|Interaction")
	FGloamFocusedInteractableChanged OnFocusedInteractableChanged;

	// === Verbs (driven by the pawn's input bindings) ===

	/** Run the primary verb on the focused interactable if it currently allows interaction. Returns true if dispatched. */
	UFUNCTION(BlueprintCallable, Category = "Gloam|Interaction")
	bool TryInteract();

	/** Run the secondary "examine" verb on the focused interactable. Returns true if dispatched. */
	UFUNCTION(BlueprintCallable, Category = "Gloam|Interaction")
	bool TryExamine();

	// === Queries ===

	UFUNCTION(BlueprintPure, Category = "Gloam|Interaction")
	AActor* GetFocusedInteractable() const { return FocusedActor.Get(); }

	/** The focused interactable's HUD prompt, or empty text when nothing is focused / interaction disallowed. */
	UFUNCTION(BlueprintPure, Category = "Gloam|Interaction")
	FText GetCurrentPrompt() const;

	/**
	 *  Pure focus decision: index of the best candidate location, or INDEX_NONE.
	 *  A candidate qualifies if it is within MaxRange and inside the view cone (direction dot >= MinViewDot).
	 *  Qualifying candidates are scored by alignment (higher dot wins); ties broken by nearer distance.
	 *  No world access — deterministic and unit-testable.
	 */
	static int32 FindBestInteractableIndex(
		const TArray<FVector>& CandidateLocations,
		const FVector& ViewLocation,
		const FVector& ViewDirection,
		float MaxRange,
		float MinViewDot);

	// === Test seam: run the configured selection over explicit locations, no world needed. ===
	int32 Test_SelectFromLocations(const TArray<FVector>& Locations, const FVector& ViewLocation, const FVector& ViewDirection) const
	{
		return FindBestInteractableIndex(Locations, ViewLocation, ViewDirection, InteractionRange, MinViewConeDot);
	}

protected:
	/** Re-evaluate focus from the world and broadcast if it changed. */
	void UpdateFocus();

	void SetFocusedActor(AActor* NewFocus);

	/** Max distance an interactable can be focused from the view point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gloam|Interaction", meta = (ClampMin = "0.0"))
	float InteractionRange = 350.0f;

	/** Sphere radius used to gather candidate actors before the cone/range filter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gloam|Interaction", meta = (ClampMin = "0.0"))
	float DetectionRadius = 600.0f;

	/** Minimum dot(view dir, dir-to-candidate) to be considered "looked at" (0.5 ≈ 60° half-cone). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gloam|Interaction", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float MinViewConeDot = 0.5f;

	/** Seconds between focus re-evaluations (throttles the world query). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gloam|Interaction", meta = (ClampMin = "0.0"))
	float UpdateInterval = 0.1f;

private:
	TWeakObjectPtr<AActor> FocusedActor;
	float TimeSinceLastUpdate = 0.0f;
};
