#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightRuntimeTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "GloamsteadHUD.generated.h"

class AVeilHeart;
class UGloamsteadExperienceCycleSubsystem;
class UGloamsteadPCGSubsystem;

/**
 * The sanctuary's heads-up display, drawn on the canvas from C++.
 *
 * Gloamstead shipped with no HUD of any kind. The player could not see which cycle they were in,
 * which phase, how much light the sanctuary held, how far corruption had spread, how long the night
 * had left, or - after the first night - what the Heart had actually said, since the caption widget
 * shows a warning once and then clears. Every one of those is a fact the game asks the player to
 * reason with, and every one of them lived only in the log.
 *
 * This is Canvas rather than UMG deliberately. A UMG HUD is a binary Widget Blueprint, and Widget
 * Blueprints can only be authored in the editor - which would have left the whole readout blocked on
 * a human. Canvas is C++, so it ships, and it is gate-testable. A designed UMG pass can replace it
 * later without changing a single system: nothing here writes state, it only reads.
 *
 * Toggle with `gloam.HUD.Show 0` to photograph the world without it.
 */
UCLASS()
class GLOAMSTEAD_API AGloamsteadHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGloamsteadHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void DrawHUD() override;

	/** Bound to the Heart so the warning stays legible for the whole cycle, not for one caption. */
	UFUNCTION()
	void HandleHeartWarning(const FVeilHeartWarningFragment& WarningFragment);

	/** Bound to the phase authority so the night countdown starts from the real transition. */
	UFUNCTION()
	void HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase);

	/**
	 * Bound to the Heart so the night the player just endured is answered on screen.
	 *
	 * The dawn outcome is the game's whole payoff - it carries whether the objective resolved,
	 * whether the warning was heeded, what the second reading graded out as, and the scar or boon
	 * that carries into the next cycle. Every one of those was computed, broadcast, and then read by
	 * nothing: the only consumer was a UE_LOG. A player finished six nights and was never told once
	 * whether they had understood any of them.
	 */
	UFUNCTION()
	void HandleDawnReflection(const FNightRuntimeOutcome& Outcome);

	/** One night, as the ending's reckoning remembers it. */
	struct FGloamNightRecord
	{
		int32 Cycle = 0;
		ENightOutcomeResult Result = ENightOutcomeResult::None;
		ENightConsequenceType NightType = ENightConsequenceType::Invalid;
		FName ResultTag = NAME_None;
		EExperienceReadingGrade Grade = EExperienceReadingGrade::Unread;
		FName ReadingTag = NAME_None;
		bool bWarningHeeded = false;
	};

	/** Test seam: the arc as the HUD has recorded it, one entry per resolved night. */
	const TArray<FGloamNightRecord>& Test_GetNightLedger() const { return NightLedger; }

	/**
	 * Test seam: record a night under an explicit cycle.
	 *
	 * The public handler resolves the cycle from the experience subsystem, which is a GameInstance
	 * subsystem and therefore absent from a bare test world - so without this seam the ledger's one
	 * real rule (one row per cycle, last reflection wins) could only be exercised by standing up a
	 * game instance, a catalog and an armed plan to assert a property that has nothing to do with
	 * any of them.
	 */
	void Test_RecordNight(int32 Cycle, const FNightRuntimeOutcome& Outcome)
	{
		RecordNightIntoLedger(Cycle, Outcome);
	}

	/**
	 * Test seam: the verdict headline for the night just resolved, or empty when none has resolved.
	 *
	 * Deliberately NOT phase-gated - that is a separate question, and folding the two together made
	 * the headline untestable without standing up a phase authority and walking it to Dawn.
	 */
	FString Test_GetDawnVerdictLine() const;

	/** The predicate DrawHUD uses: is there a resolved night AND is the player standing in its dawn. */
	bool ShouldDrawDawnSummary() const;

	/** Test seam: the warning line the HUD would draw right now. */
	const FText& Test_GetStandingWarningText() const { return StandingWarningText; }

	/** Test seam: seconds of night remaining, or a negative value when it is not night. */
	float Test_GetNightSecondsRemaining() const;

	/**
	 * Every verb the player has, as one row each: key, name, and what it is for.
	 *
	 * Public and static because it is the ONLY place in the build that documents the controls, and
	 * two of the five verbs are bound at key level rather than through an input-mapping asset - so
	 * an asset-side check cannot see them and a player has nowhere else to learn them. The test
	 * asserts this table names every verb the character binds, which is what stops a seventh verb
	 * from shipping undocumented.
	 */
	struct FGloamControlRow
	{
		const TCHAR* Key;
		const TCHAR* Verb;
		const TCHAR* Meaning;
	};
	static TArrayView<const FGloamControlRow> GetControlRows();

private:
	void ResolveSources();

	/** One labelled 0..1 meter. Returns the Y to draw the next row at. */
	float DrawMeter(float X, float Y, float Width, const FString& Label, float Value, const FLinearColor& Fill);

	/** Word-wraps text to Width and draws it, returning the Y below the last line. */
	float DrawWrapped(float X, float Y, float Width, const FString& Text, const FLinearColor& Color, float Scale);

	/**
	 * The opening screen: what the game is, what the verbs are, and how to start.
	 *
	 * The project had no main menu at all - the player was dropped into Day I with two of the five
	 * verbs bound at key level and documented nowhere they were likely to look. This is on canvas
	 * for the same reason the rest of the readout is: a Widget Blueprint can only be authored in the
	 * editor, and a menu that needs a human to exist is a menu that does not ship.
	 */
	void DrawTitleScreen();

	/** The held-sanctuary screen: a scrim, the title, and the controls table. */
	void DrawPausedOverlay();

	/**
	 * The interpretation aid: the clues found for the standing warning, and progress toward the gate.
	 *
	 * This is the journal the design has owed since workstream G. The catalogs already author a
	 * sentence per clue and the plan already sets a minimum distinct count; until now neither
	 * reached the screen, so a player was asked to gather enough evidence without being shown what
	 * they had gathered or how much was enough.
	 */
	void DrawEvidenceJournal(float X, float Y, float Width);

	/** One row per cycle, last reflection wins. The ledger is the arc, not a call count. */
	void RecordNightIntoLedger(int32 Cycle, const FNightRuntimeOutcome& Outcome);

	/** The dawn payoff panel: what the night was, what you did, and what it left behind. */
	void DrawDawnSummary();

	/** The ending: the whole arc read back as one reckoning, once the authored nights are behind you. */
	void DrawEndingReckoning();

	TWeakObjectPtr<UGloamsteadDayNightSubsystem> DayNight;
	TWeakObjectPtr<UGloamsteadExperienceCycleSubsystem> Cycles;
	TWeakObjectPtr<UGloamsteadPCGSubsystem> PCG;
	TWeakObjectPtr<AVeilHeart> Heart;

	/**
	 * The Heart's current words, held until the Heart speaks again.
	 *
	 * The existing caption surface presents a warning once and dedupes re-broadcasts, which is right
	 * for a caption and wrong for the only record of the sentence the player must interpret over a
	 * whole day of exploring.
	 */
	FText StandingWarningText;
	FName StandingWarningId = NAME_None;

	/** World time the current phase began, so Night can be counted down honestly. */
	float PhaseEnteredWorldTime = 0.f;

	/**
	 * The night just resolved, held so Dawn can answer it.
	 *
	 * Kept rather than polled from the Heart because the Heart clears its per-cycle interpretation
	 * state at the end of the same reflection call that broadcasts this - so by the time a draw ran,
	 * asking the Heart would already be asking the next cycle.
	 */
	FNightRuntimeOutcome LastDawnOutcome;
	bool bHasDawnOutcome = false;

	/** Every resolved night, in order, so the ending can be a reckoning rather than a congratulation. */
	TArray<FGloamNightRecord> NightLedger;

	bool bBound = false;
	bool bDawnBound = false;

	/** True until the player steps into the sanctuary. Cleared by AGloamsteadPlayerController. */
	bool bTitleScreenPending = true;

public:
	/** Whether the opening screen is still holding. */
	bool IsTitlePending() const { return bTitleScreenPending; }

	/** Step into the sanctuary. Idempotent: a second press during play must not re-open the title. */
	void DismissTitleScreen() { bTitleScreenPending = false; }

private:
};
