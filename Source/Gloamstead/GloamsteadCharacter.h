// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "GloamsteadCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class URitualPlacementComponent;
class UGloamInteractionComponent;
class UNightConsequenceRuntime;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AGloamsteadCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Restoration of ritual points (queries the PCG subsystem, previews + confirms placement). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gloamstead", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URitualPlacementComponent> RitualPlacement;

	/** Focus + verbs for discrete world IGloamInteractable objects (Veil Heart rest, examinables). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gloamstead", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGloamInteractionComponent> Interaction;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Restore Input Action — the signature verb: enter ritual placement, then confirm a valid target. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* RestoreAction;

	/** Interact Input Action — primary verb on the focused IGloamInteractable world object. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractAction;

	/** Examine Input Action — secondary "examine/focus" verb on the focused IGloamInteractable. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ExamineAction;

public:

	/** Constructor */
	AGloamsteadCharacter();	

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Restore input: enter ritual placement mode, then confirm once the previewed target is valid. */
	void OnRestoreInput();

	/** Interact input: run the primary verb on the currently focused IGloamInteractable. */
	void OnInteractInput();

	/** Examine input: run the examine verb on the currently focused IGloamInteractable. */
	void OnExamineInput();

	/** Ward input: spend a deliberate light beat against a strategy-owned night threat. */
	void OnWardInput();

	/** Strike input: interrupt the nearest threat's work. Buys seconds; resolves nothing. */
	void OnStrikeInput();

	/** Mirror input: refuse the false reflection. */
	void OnMirrorRefuseInput();

	/** Mirror input: accept the bargain, then hold it with light. */
	void OnMirrorAcceptInput();

public:

	// === Automated-playtest console hooks ===
	//
	// Restore/Interact/Examine bind to ETriggerEvent::Started, and simulated key events do NOT produce
	// the press transition Enhanced Input needs for an edge trigger — verified empirically: with logging
	// on the handlers, playtest_key produced no handler call at all. These exec commands call the SAME
	// handler functions the input bindings call, so an automated run can exercise everything downstream
	// of the input layer. They add no behaviour of their own and are not bound to any key.
	//
	// They do NOT prove the keyboard path; that is established by the asset wiring (IMC_Default maps
	// R -> IA_Restore / E -> IA_Interact / Q -> IA_Examine, and the character's action slots are set).

	UFUNCTION(Exec)
	void GloamRestore() { OnRestoreInput(); }

	UFUNCTION(Exec)
	void GloamInteract() { OnInteractInput(); }

	UFUNCTION(Exec)
	void GloamExamine() { OnExamineInput(); }

	/** Automated-playtest and accessibility hook for the light ward (also bound to Right Mouse). */
	UFUNCTION(Exec)
	void GloamWard() { OnWardInput(); }

	/** Automated-playtest and accessibility hook for the strike (also bound to Left Mouse). */
	UFUNCTION(Exec)
	void GloamStrike() { OnStrikeInput(); }

	/** Automated-playtest and accessibility hook for the Mirror/Bargain refusal choice. */
	UFUNCTION(Exec)
	void GloamMirrorRefuse() { OnMirrorRefuseInput(); }

	/** Automated-playtest and accessibility hook for the Mirror/Bargain acceptance choice. */
	UFUNCTION(Exec)
	void GloamMirrorAccept() { OnMirrorAcceptInput(); }

	/** Playtest positioning only: walking the plaza needs movement input the harness cannot simulate. */
	UFUNCTION(Exec)
	void GloamTeleport(float X, float Y, float Z);

	/**
	 * Bring the night, the way resting at the Heart does.
	 *
	 * Every phase transition in this game is player-gated by design: Day->Dusk and Dusk->Night go
	 * through UGloamsteadDayNightSubsystem::RequestRest, whose only non-test caller is AVeilHeart
	 * reacting to a player interaction. That is the right design and it had one consequence nobody
	 * had noticed: a headless run supplies no input, so the world sits in Day forever and the phase
	 * log never fires once. Fifteen-minute unattended boots looked like a hang and were a world
	 * correctly waiting for a person.
	 *
	 * The existing GloamInteract exec cannot substitute: it runs the focused interactable, and
	 * nothing is focused unless the pawn is standing at the Heart. This drives the same subsystem
	 * entry point the Heart drives, so an automated pass exercises the real cadence rather than a
	 * parallel one. It adds no behaviour of its own and is bound to no key.
	 */
	UFUNCTION(Exec)
	void GloamRest();

	/**
	 * Open the first rest without restoring the lantern first.
	 *
	 * Cycle I gates rest behind AGloamsteadFirstNightDirector seeing the lantern restored. An
	 * automated pass that wants to reach Cycle II's interpretation - the first cycle with evidence,
	 * readings and a real warning - should not have to solve placement geometry to get there.
	 */
	UFUNCTION(Exec)
	void GloamUnlockFirstRest();

	/** Print where the arc actually is, so a headless run can be asserted on rather than guessed at. */
	UFUNCTION(Exec)
	void GloamStatus();

	/**
	 * Walk the whole authored arc unattended, one beat every BeatSeconds.
	 *
	 * This exists because of a specific, expensive misreading. Every phase transition in this game
	 * is player-gated on purpose, so an unattended boot sits in Day forever and logs no transition
	 * at all. That looked exactly like a hang, and cost three fifteen-minute boots and a CPU-sampling
	 * pass to rule out - the game thread was ticking the whole time, correctly waiting for a person.
	 *
	 * A single -ExecCmds string cannot substitute: it fires once at startup, before the pawn exists
	 * and long before any cycle could be answered. So this schedules itself on a timer and drives
	 * the same entry points a player drives - rest to bring the night, ward and strike during it,
	 * rest again at dawn - until the experience reports complete.
	 *
	 * It is a HARNESS, not a difficulty setting: it never fabricates a restoration, never grants a
	 * reading, and never forces a phase the subsystem refuses. A cycle that will not advance under
	 * it is a cycle a player could not advance either, which is exactly what makes it worth running.
	 */
	UFUNCTION(Exec)
	void GloamAutoPlay(float BeatSeconds = 2.0f);

	/**
	 * Command-line entry point for the same harness: `-GloamAutoPlay` (optionally
	 * `-GloamAutoPlayBeat=1.0`).
	 *
	 * `-ExecCmds` cannot start this and it is worth saying why, because it looks like it should:
	 * those commands run during engine initialisation, before the world has a pawn for a Character
	 * exec to route to, so the whole string is parsed, logged on the command line, and silently
	 * executed against nothing. A switch read from BeginPlay runs at the only moment that is
	 * actually correct - once this pawn exists in a live world.
	 */
	void StartAutoPlayIfRequested();

private:
	/** One beat of GloamAutoPlay. */
	void AutoPlayBeat();

	/**
	 * The day's work, in the order a player does it: read the evidence, restore the place the
	 * warning names, then commit the sharper reading. Returns true when it did something, so the
	 * beat only rests once the cycle's work is actually finished.
	 */
	bool AutoPlayDoDayWork();

	/**
	 * Get to Where. Returns true while still travelling, false once close enough to act.
	 *
	 * Teleport mode answers false immediately after moving; walk mode steers the real movement
	 * component and answers true until it arrives, which is what makes the two modes comparable.
	 */
	bool AutoPlayApproach(const FVector& Where);

	FTimerHandle AutoPlayTimer;
	int32 AutoPlayBeats = 0;

	/**
	 * -GloamAutoWalk: cross the sanctuary on foot instead of teleporting.
	 *
	 * The harness teleports by default because that is the cheapest way to exercise the loop. It
	 * also makes any playtime it reports a speedrun floor rather than a measurement: a run that
	 * never travels cannot say what travelling costs. With this set the pawn walks at its real
	 * MaxWalkSpeed to every evidence source, ritual point and reading choice, so the number the run
	 * produces is the mechanical length of the game rather than the length of its state machine.
	 */
	bool bAutoWalk = false;
	FVector AutoWalkTarget = FVector::ZeroVector;
	bool bAutoWalkActive = false;
	float AutoWalkReportAccumulator = 0.f;

	/**
	 * Stall recovery for walk mode.
	 *
	 * Lvl_Gloamstead contains no NavMeshBoundsVolume and no RecastNavMesh - verified by scanning the
	 * package - so nothing in this game paths: the night threats move by hand-rolled straight lines
	 * too. A straight-line walker therefore pins itself against the first wall between it and its
	 * target and reports "never arrived" forever, which is indistinguishable from a map a player
	 * could not cross. A human simply steers around; this is the cheapest equivalent, and its whole
	 * purpose is to tell those two cases apart.
	 */
	float AutoWalkStalledFor = 0.f;
	float AutoWalkSidestepFor = 0.f;
	float AutoWalkSidestepSign = 1.f;

	/** Close enough to act on a thing without standing inside it. */
	static constexpr float AutoPlayArriveRadius = 160.f;

	/** -GloamAutoShots: photograph each phase, so the Canvas HUD can be seen rather than inferred. */
	bool bAutoShots = false;
	int32 AutoShotIndex = 0;
	EGloamsteadDayPhase LastShotPhase = EGloamsteadDayPhase::Dawn;

public:

	/**
	 * The single most relevant on-screen prompt for what the player can do right now, or empty.
	 *
	 * Nothing previously surfaced GloamInteractionComponent::GetCurrentPrompt(), so the Heart's
	 * "Rest at the Heart" and the ritual's confirm/cancel existed only as data. Resolution order is
	 * placement first, then the focused interactable, because while a ritual is armed that IS the
	 * thing the player is doing. The bracketed keys mirror IMC_Default (R = Restore, E = Interact).
	 */
	UFUNCTION(BlueprintPure, Category = "Gloamstead|UI")
	FText GetPlayerPromptText() const;

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Returns the RitualPlacement subobject **/
	FORCEINLINE URitualPlacementComponent* GetRitualPlacement() const { return RitualPlacement; }

	/** Returns the Interaction subobject **/
	FORCEINLINE UGloamInteractionComponent* GetInteraction() const { return Interaction; }
};
