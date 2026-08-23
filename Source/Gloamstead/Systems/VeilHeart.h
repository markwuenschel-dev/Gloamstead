#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RitualTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Interfaces/GloamInteractable.h"
#include "VeilHeart.generated.h"

class USphereComponent;
class AGloamsteadEvidenceSource;
class UGloamsteadPCGSubsystem;

/**
 * Broadcast when the Heart warns about the coming night, carrying the catalog fragment it chose.
 *
 * The BlueprintImplementableEvents below can only be answered by a Blueprint SUBCLASS of the Heart,
 * which meant the warning text was unreachable by anything else — including the first-night director,
 * which is the actor that actually owns the caption widget. These delegates let any listener present
 * the Heart's voice without having to be the Heart.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVeilHeartWarning, const FVeilHeartWarningFragment&, WarningFragment);

/** Broadcast at dawn with the night's real outcome, for the same reason as FOnVeilHeartWarning. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVeilHeartDawnReflection, const FNightRuntimeOutcome&, Outcome);

/**
 * The Veil Heart - Central protected object and emotional core of Gloamstead.
 * Listens to restoration events to evaluate "I understood the warning".
 *
 * It is also the player's "rest point": implementing IGloamInteractable, the Interact verb rests through
 * the resting phases (Day -> Dusk to bring the night, Dawn -> Day to wake), the player-driven advance that
 * carries the recurring loop; Examine speaks the Heart's memory of the last night.
 */
UCLASS(Blueprintable, BlueprintType)
class GLOAMSTEAD_API AVeilHeart : public AActor, public IGloamInteractable
{
    GENERATED_BODY()

public:
    AVeilHeart();

    virtual void BeginPlay() override;

    // IGloamInteractable — the Heart is the player's rest point.
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual FText GetInteractionPrompt_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual void Examine_Implementation(AActor* Interactor) override;

    /** Legacy tag feedback. This raw evaluator is intentionally not Blueprint-callable. */
    void EvaluateRestorationAgainstWarnings(const FRestorationEventPayload& Payload);

	/**
	 * Records a player encounter from one live, authored evidence actor. This is
	 * deliberately a C++ authority seam rather than a Blueprint payload API.
	 */
	bool RecordSupportEncounterFromEvidenceSource(const AGloamsteadEvidenceSource* Source);

	UFUNCTION(BlueprintCallable, Category="Veil Heart")
	void EmitWarningForNight(ENightConsequenceType NightType);

	/**
	 * Emits one exact authored warning. The ID must appear exactly once in the
	 * assigned catalog, its associated type must equal ExpectedNightType, and a
	 * registered live player-facing presenter must be available.
	 */
	UFUNCTION(BlueprintCallable, Category="Veil Heart")
	bool EmitWarningById(FName WarningId, ENightConsequenceType ExpectedNightType);

	/** Exact active-plan admission used by DayNight before it makes rest eligible. */
	bool CanPresentWarningForPlan(const FExperienceCyclePlan& Plan);
	/** Presents an already-admitted exact authored plan; no ID/type-only fallback. */
	bool EmitWarningForPlan(const FExperienceCyclePlan& Plan);

    /** Legacy no-outcome dawn reflection (BP compat): reflects on an empty outcome. */
    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void ProcessDawnReflection();

    /** Dawn reflection with the night's real outcome; distinguishes success / partial / failure. */
    UFUNCTION(BlueprintCallable, Category="Veil Heart")
    void ProcessDawnReflectionWithOutcome(const FNightRuntimeOutcome& Outcome);

    UFUNCTION(BlueprintPure, Category="Veil Heart")
    int32 GetSatisfiedWarningTagCount() const { return SatisfiedWarningTags.Num(); }

	/** Last concrete receipt earned by an exact warning/evidence/restoration match. */
	UFUNCTION(BlueprintPure, Category="Veil Heart|Interpretation")
	FExperienceInterpretationReceipt GetLastInterpretationReceipt() const { return LastInterpretationReceipt; }

	/** True only when the stored receipt exactly proves this authored plan was interpreted. */
	bool HasExactInterpretationForPlan(const FExperienceCyclePlan& Plan) const;

	/** Capture/restore only durable interpretation facts; cadence/presenter state stays live-only. */
	FVeilHeartInterpretationPersistentState CaptureInterpretationPersistentState() const;
	/** Whether the authored warning catalog is available for a delayed restore attempt. */
	bool IsInterpretationCatalogReady();
	bool RestoreInterpretationPersistentState(const FVeilHeartInterpretationPersistentState& State);
	void ResetInterpretationPersistentState();

    /** The outcome of the most recently reflected-upon night (session memory the next cycle can read). */
    UFUNCTION(BlueprintPure, Category="Veil Heart")
    FNightRuntimeOutcome GetLastNightOutcome() const { return LastNightOutcome; }

	/** Exact authored warning most recently presented by this Heart. */
	UFUNCTION(BlueprintPure, Category="Veil Heart")
	FName GetLastEmittedWarningId() const { return LastEmittedWarningId; }

	/**
	 * Registers the one player-facing warning presenter after it has bound its
	 * exact dynamic-delegate handler. Blueprint events and incidental observers
	 * do not satisfy this presentation authority.
	 */
	bool RegisterWarningPresenter(UObject* Presenter, FName WarningHandlerFunction);
	void UnregisterWarningPresenter(UObject* Presenter);
	bool HasValidWarningPresenter() const;

	/** True only when this Heart owns exactly one matching catalog row. */
	bool HasExactWarningById(FName WarningId, ENightConsequenceType ExpectedNightType);

#if WITH_DEV_AUTOMATION_TESTS
	/** Narrow test seam for focused fair-crypticism catalog fixtures. */
	void Test_SetActivePlan(const FExperienceCyclePlan& InPlan)
	{
		TestActivePlan = InPlan;
		bHasTestActivePlan = true;
	}

	void Test_ClearActivePlan()
	{
		TestActivePlan = FExperienceCyclePlan::MakeInvalid(0);
		bHasTestActivePlan = false;
	}

	/** Test-only controlled route for source/media validation without exposing a Blueprint write API. */
	bool Test_RecordSupportEncounter(FName WarningId, FName SupportId, FName ChannelType);
#endif

    UFUNCTION(BlueprintImplementableEvent, Category="Veil Heart")
    void OnWarningEmitted(const FVeilHeartWarningFragment& WarningFragment);

    /** BP presentation hook for the dawn payoff (journal/feedback/VFX), fed the night's outcome. */
    UFUNCTION(BlueprintImplementableEvent, Category="Veil Heart")
    void OnDawnReflection(const FNightRuntimeOutcome& Outcome);

    /** Fires alongside OnWarningEmitted, for listeners that are not Blueprint subclasses of the Heart. */
    UPROPERTY(BlueprintAssignable, Category="Veil Heart")
    FOnVeilHeartWarning OnWarningEmittedDelegate;

    /** Fires alongside OnDawnReflection, for listeners that are not Blueprint subclasses of the Heart. */
    UPROPERTY(BlueprintAssignable, Category="Veil Heart")
    FOnVeilHeartDawnReflection OnDawnReflectionDelegate;

    /** Assign Content/Data/DA_VeilHeartWarningCatalog (auto-loaded at BeginPlay if left empty). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Veil Heart")
    TObjectPtr<UVeilHeartWarningCatalog> WarningCatalog;

    /**
     * Query-only volume so the player's interaction focus trace (an object-type overlap) can find the Heart.
     * Without it the Heart has no collision at all and rest/greet-dawn can never fire for a real player.
     * Non-blocking (overlap responses) so it never impedes movement.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Veil Heart")
    TObjectPtr<USphereComponent> InteractionVolume;

protected:
    UFUNCTION()
    void OnRestorationComplete(const FRestorationEventPayload& Payload);

    const FVeilHeartWarningFragment* FindWarningForNight(ENightConsequenceType NightType) const;

private:
	/** Lazily loads the assigned catalog for startup-order-safe exact emission. */
	bool EnsureWarningCatalog();
	const FVeilHeartWarningFragment* FindExactWarningById(FName WarningId, ENightConsequenceType ExpectedNightType) const;
	const FExperienceCyclePlan* ResolveActivePlan() const;
	UGloamsteadPCGSubsystem* ResolvePCGSubsystem() const;
	bool IsExactWarningPresentedForPlan(const FExperienceCyclePlan& Plan) const;
	bool HasRequiredSupportEvidence(const FExperienceCyclePlan& Plan) const;
	bool RecordSupportEncounterInternal(FName WarningId, FName SupportId, FName ChannelType);
	bool EvaluateRestorationAgainstActivePlan(const FRestorationEventPayload& Payload);

    TSet<FName> SatisfiedWarningTags;
	TSet<FName> EncounteredSupportIds;

	UPROPERTY()
	FExperienceInterpretationReceipt LastInterpretationReceipt;

    UPROPERTY()
    FNightRuntimeOutcome LastNightOutcome;

	UPROPERTY()
	FName LastEmittedWarningId = NAME_None;

	/** Weak identity avoids keeping a torn-down presenter alive across world teardown. */
	TWeakObjectPtr<UObject> RegisteredWarningPresenter;
	FName RegisteredWarningPresenterFunction = NAME_None;

#if WITH_DEV_AUTOMATION_TESTS
	FExperienceCyclePlan TestActivePlan;
	bool bHasTestActivePlan = false;
#endif
};
