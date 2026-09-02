#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
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

	/** The held-sanctuary screen: a scrim, the title, and the controls table. */
	void DrawPausedOverlay();

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

	bool bBound = false;
};
