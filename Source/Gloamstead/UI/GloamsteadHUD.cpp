#include "UI/GloamsteadHUD.h"

#include "Actors/GloamsteadNightThreat.h"
#include "Data/ExperienceCycleTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GloamsteadPlayerController.h"
#include "HAL/IConsoleManager.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Systems/VeilHeart.h"

/**
 * Named, not anonymous - adaptive unity does not keep anonymous-namespace names apart in this
 * module, and that has already shipped one broken commit here.
 */
namespace GloamsteadHUDDraw
{
	static TAutoConsoleVariable<bool> CVarShowHUD(
		TEXT("gloam.HUD.Show"),
		true,
		TEXT("Draw the Gloamstead canvas HUD (default on). Set 0 to photograph the world clean."),
		ECVF_Default);

	// A dark sanctuary read at night. Parchment for facts, amber for the Heart, rust for corruption.
	const FLinearColor Ink        (0.90f, 0.87f, 0.79f, 1.f);
	const FLinearColor Dim        (0.62f, 0.60f, 0.55f, 1.f);
	const FLinearColor Amber      (1.00f, 0.80f, 0.42f, 1.f);
	const FLinearColor LightFill  (1.00f, 0.84f, 0.52f, 1.f);
	const FLinearColor RotFill    (0.72f, 0.30f, 0.20f, 1.f);
	const FLinearColor Panel      (0.02f, 0.02f, 0.03f, 0.58f);
	const FLinearColor MeterTrack (0.10f, 0.10f, 0.11f, 0.85f);

	FString PhaseName(EGloamsteadDayPhase Phase)
	{
		switch (Phase)
		{
		case EGloamsteadDayPhase::Day:   return TEXT("DAY");
		case EGloamsteadDayPhase::Dusk:  return TEXT("DUSK");
		case EGloamsteadDayPhase::Night: return TEXT("NIGHT");
		case EGloamsteadDayPhase::Dawn:  return TEXT("DAWN");
		default:                         return TEXT("--");
		}
	}

	/** Cycle numbers are spoken as Roman numerals everywhere in this project's authored text. */
	FString RomanNumeral(int32 Value)
	{
		static const TCHAR* Numerals[] = {
			TEXT("0"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"),
			TEXT("V"), TEXT("VI"), TEXT("VII"), TEXT("VIII"), TEXT("IX")
		};
		return (Value >= 0 && Value < UE_ARRAY_COUNT(Numerals))
			? FString(Numerals[Value])
			: FString::FromInt(Value);
	}

	/**
	 * What the player is being asked to do in this phase, in one line.
	 *
	 * Deliberately phase-derived rather than authored per cycle: the cadence is the same every time
	 * (read, restore, rest, endure, wake), and an authored objective string per cycle would be six
	 * more places for the arc to drift out of step with the code that actually gates each step.
	 */
	FString PhaseObjective(EGloamsteadDayPhase Phase)
	{
		switch (Phase)
		{
		// The keys are named here because this game has no tutorial and no control screen, and its
		// two night verbs are bound at key level rather than through an input-mapping asset - so
		// nothing else in the build ever tells the player that Strike and Ward exist.
		case EGloamsteadDayPhase::Day:
			return TEXT("Read the warning. Find its evidence. Restore the place it names.   "
						"[R] restore   [E] interact   [Q] examine");
		case EGloamsteadDayPhase::Dusk:
			return TEXT("Bring the night at the Heart when you are ready.   [E] at the Heart");
		case EGloamsteadDayPhase::Night:
			return TEXT("Hold what you built. Light answers what walks.   "
						"[LMB] strike to buy seconds   [RMB] ward and cleanse");
		case EGloamsteadDayPhase::Dawn:
			return TEXT("Rest at the Heart to see what the night cost.   [E] at the Heart");
		default:
			return FString();
		}
	}
}

TArrayView<const AGloamsteadHUD::FGloamControlRow> AGloamsteadHUD::GetControlRows()
{
	// Kept in the same order the player meets them: move, then the day verbs, then the night verbs,
	// then the one choice. Strike and Ward sit together because they are the pair that answers a
	// night, and a player who learns one without the other will try to win a fight.
	static const FGloamControlRow Rows[] = {
		{ TEXT("W A S D"),    TEXT("move"),         TEXT("") },
		{ TEXT("mouse"),      TEXT("look"),         TEXT("") },
		{ TEXT("space"),      TEXT("jump"),         TEXT("") },
		{ TEXT("R"),          TEXT("restore"),      TEXT("raise the ritual this place asks for") },
		{ TEXT("E"),          TEXT("interact"),     TEXT("rest at the Heart; read what you find") },
		{ TEXT("Q"),          TEXT("examine"),      TEXT("look closer at the evidence") },
		{ TEXT("left mouse"), TEXT("strike"),       TEXT("interrupt a threat - buys seconds, wins nothing") },
		{ TEXT("right mouse"),TEXT("ward"),         TEXT("answer it with light; cleanse what can be cleansed") },
		{ TEXT("1 / 2"),      TEXT("refuse / accept"), TEXT("when something offers you a bargain") },
		{ TEXT("Esc"),        TEXT("hold"),         TEXT("stop the sanctuary where it stands") },
	};
	return MakeArrayView(Rows, UE_ARRAY_COUNT(Rows));
}

AGloamsteadHUD::AGloamsteadHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGloamsteadHUD::BeginPlay()
{
	Super::BeginPlay();
	ResolveSources();

	if (DayNight.IsValid())
	{
		DayNight->OnPhaseChanged.AddDynamic(this, &AGloamsteadHUD::HandlePhaseChanged);
		bBound = true;
	}

	// The Heart may not be spawned yet at HUD BeginPlay; ResolveSources retries every draw, and the
	// binding is made the first time it succeeds.
	if (UWorld* World = GetWorld())
	{
		PhaseEnteredWorldTime = World->GetTimeSeconds();
	}
}

void AGloamsteadHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bBound && DayNight.IsValid())
	{
		DayNight->OnPhaseChanged.RemoveDynamic(this, &AGloamsteadHUD::HandlePhaseChanged);
	}
	if (Heart.IsValid())
	{
		Heart->OnWarningEmittedDelegate.RemoveDynamic(this, &AGloamsteadHUD::HandleHeartWarning);
	}
	bBound = false;

	Super::EndPlay(Reason);
}

void AGloamsteadHUD::ResolveSources()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!DayNight.IsValid())
	{
		DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>();
	}
	// Cycle identity is deliberately a GameInstance subsystem, not a world one, so the armed plan
	// survives level transitions - so it is fetched from the game instance, not the world.
	if (!Cycles.IsValid())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			Cycles = GameInstance->GetSubsystem<UGloamsteadExperienceCycleSubsystem>();
		}
	}
	if (!PCG.IsValid())
	{
		PCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	}
	if (!Heart.IsValid())
	{
		for (TActorIterator<AVeilHeart> It(World); It; ++It)
		{
			Heart = *It;
			// Listening rather than polling: the Heart owns when it speaks, and a getter for "the
			// last thing said" would be a second source of that truth to keep in step.
			Heart->OnWarningEmittedDelegate.AddDynamic(this, &AGloamsteadHUD::HandleHeartWarning);
			break;
		}
	}
}

void AGloamsteadHUD::HandleHeartWarning(const FVeilHeartWarningFragment& WarningFragment)
{
	StandingWarningId = WarningFragment.WarningId;
	StandingWarningText = WarningFragment.Fragment;
}

void AGloamsteadHUD::HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase)
{
	if (UWorld* World = GetWorld())
	{
		PhaseEnteredWorldTime = World->GetTimeSeconds();
	}
}

float AGloamsteadHUD::Test_GetNightSecondsRemaining() const
{
	if (!DayNight.IsValid() || DayNight->GetCurrentPhase() != EGloamsteadDayPhase::Night)
	{
		return -1.f;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return -1.f;
	}
	const float Elapsed = World->GetTimeSeconds() - PhaseEnteredWorldTime;
	return FMath::Max(0.f, DayNight->NightDurationSeconds - Elapsed);
}

float AGloamsteadHUD::DrawMeter(
	float X, float Y, float Width, const FString& Label, float Value, const FLinearColor& Fill)
{
	using namespace GloamsteadHUDDraw;

	const float Clamped = FMath::Clamp(Value, 0.f, 1.f);
	constexpr float BarHeight = 9.f;

	DrawText(Label, Dim, X, Y, GEngine->GetSmallFont(), 1.f);
	const float BarY = Y + 15.f;
	DrawRect(MeterTrack, X, BarY, Width, BarHeight);
	if (Clamped > 0.f)
	{
		DrawRect(Fill, X, BarY, Width * Clamped, BarHeight);
	}
	DrawText(FString::Printf(TEXT("%3d%%"), FMath::RoundToInt(Clamped * 100.f)),
		Dim, X + Width + 8.f, Y + 2.f, GEngine->GetSmallFont(), 1.f);

	return BarY + BarHeight + 10.f;
}

float AGloamsteadHUD::DrawWrapped(
	float X, float Y, float Width, const FString& Text, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine->GetSmallFont();
	if (!Font || Text.IsEmpty())
	{
		return Y;
	}

	// Measure-and-break rather than Canvas->WrapString: this runs once a frame on a short sentence,
	// and doing it here keeps the returned Y exact, which is what every row below depends on.
	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "), true);

	FString Line;
	float CursorY = Y;
	const float LineHeight = Font->GetMaxCharHeight() * Scale + 3.f;

	auto Flush = [&]()
	{
		if (!Line.IsEmpty())
		{
			DrawText(Line, Color, X, CursorY, Font, Scale);
			CursorY += LineHeight;
			Line.Reset();
		}
	};

	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		float TextWidth = 0.f;
		float TextHeight = 0.f;
		if (Canvas)
		{
			Canvas->TextSize(Font, Candidate, TextWidth, TextHeight, Scale, Scale);
		}
		if (TextWidth > Width && !Line.IsEmpty())
		{
			Flush();
			Line = Word;
		}
		else
		{
			Line = Candidate;
		}
	}
	Flush();

	return CursorY;
}

void AGloamsteadHUD::DrawPausedOverlay()
{
	using namespace GloamsteadHUDDraw;

	const float ViewW = Canvas->ClipX;
	const float ViewH = Canvas->ClipY;

	// A scrim rather than an opaque panel: the sanctuary should still be faintly there behind the
	// held moment, because what the player is being asked to think about is out in it.
	DrawRect(FLinearColor(0.01f, 0.01f, 0.015f, 0.86f), 0.f, 0.f, ViewW, ViewH);

	const float PanelW = FMath::Clamp(ViewW * 0.46f, 420.f, 820.f);
	const float X = (ViewW - PanelW) * 0.5f;
	float Y = ViewH * 0.16f;

	DrawText(TEXT("THE SANCTUARY HOLDS"), Amber, X, Y, GEngine->GetMediumFont(), 1.4f);
	Y += 44.f;

	// The controls table. This is the only screen in the build that documents the verbs.
	const float KeyColumn = X;
	const float VerbColumn = X + PanelW * 0.26f;
	const float MeaningColumn = X + PanelW * 0.46f;

	for (const FGloamControlRow& Row : GetControlRows())
	{
		DrawText(Row.Key, Ink, KeyColumn, Y, GEngine->GetSmallFont(), 1.f);
		DrawText(Row.Verb, LightFill, VerbColumn, Y, GEngine->GetSmallFont(), 1.f);
		if (Row.Meaning && *Row.Meaning)
		{
			DrawText(Row.Meaning, Dim, MeaningColumn, Y, GEngine->GetSmallFont(), 1.f);
		}
		Y += 21.f;
	}

	Y += 26.f;
	DrawText(TEXT("[Esc]  return to it"), Amber, X, Y, GEngine->GetSmallFont(), 1.2f);
}

void AGloamsteadHUD::DrawHUD()
{
	Super::DrawHUD();

	using namespace GloamsteadHUDDraw;

	if (!Canvas || !GEngine || !CVarShowHUD.GetValueOnGameThread())
	{
		return;
	}

	// The held sanctuary replaces the readout rather than layering over it: the meters describe a
	// world that is not currently moving, and a countdown frozen mid-tick reads as a bug.
	if (const AGloamsteadPlayerController* Gloam = Cast<AGloamsteadPlayerController>(PlayerOwner))
	{
		if (Gloam->IsSanctuaryPaused())
		{
			DrawPausedOverlay();
			return;
		}
	}

	ResolveSources();

	UWorld* World = GetWorld();
	if (!World || !DayNight.IsValid())
	{
		return;
	}

	const float ViewW = Canvas->ClipX;
	const float ViewH = Canvas->ClipY;

	// Anchored in fractions of the viewport so the readout holds together at any resolution rather
	// than at the one it happened to be authored on.
	const float PanelX = ViewW * 0.025f;
	const float PanelY = ViewH * 0.035f;
	const float PanelW = FMath::Clamp(ViewW * 0.24f, 260.f, 460.f);

	const EGloamsteadDayPhase Phase = DayNight->GetCurrentPhase();

	// ---- Left panel: where am I in the arc, and what does the sanctuary hold ----
	float Y = PanelY;
	DrawRect(Panel, PanelX - 12.f, PanelY - 12.f, PanelW + 24.f, 168.f);

	int32 Slot = 0;
	if (Cycles.IsValid())
	{
		Slot = Cycles->GetActivePlan().Slot;
	}
	// Before the first Day arms a plan the active slot is 0. Nights completed is then the honest
	// number to show, rather than a "CYCLE 0" that names nothing in the authored arc.
	const int32 ShownCycle = Slot > 0 ? Slot : DayNight->GetNightCount() + 1;

	DrawText(FString::Printf(TEXT("CYCLE %s"), *RomanNumeral(ShownCycle)),
		Ink, PanelX, Y, GEngine->GetMediumFont(), 1.f);
	DrawText(PhaseName(Phase), Amber, PanelX + PanelW - 62.f, Y + 2.f, GEngine->GetMediumFont(), 1.f);
	Y += 26.f;

	if (Cycles.IsValid() && Cycles->IsExperienceComplete())
	{
		DrawText(TEXT("the authored nights are behind you"), Dim, PanelX, Y, GEngine->GetSmallFont(), 1.f);
		Y += 16.f;
	}
	else
	{
		DrawText(FString::Printf(TEXT("nights survived: %d"), DayNight->GetNightCount()),
			Dim, PanelX, Y, GEngine->GetSmallFont(), 1.f);
		Y += 16.f;
	}
	Y += 6.f;

	if (PCG.IsValid())
	{
		Y = DrawMeter(PanelX, Y, PanelW - 46.f, TEXT("SANCTUARY LIGHT"),
			PCG->GetSanctuaryAverageLightLevel(), LightFill);
		Y = DrawMeter(PanelX, Y, PanelW - 46.f, TEXT("CORRUPTION"),
			PCG->GetSanctuaryAverageCorruptionLevel(), RotFill);
	}
	else
	{
		// Say so rather than draw two empty bars: an empty meter and a missing subsystem look
		// identical, and one of them means the sanctuary never initialised.
		DrawText(TEXT("sanctuary not yet awake"), Dim, PanelX, Y, GEngine->GetSmallFont(), 1.f);
	}

	// ---- Night band: the countdown and what is out there ----
	if (Phase == EGloamsteadDayPhase::Night)
	{
		int32 ThreatsStanding = 0;
		for (TActorIterator<AGloamsteadNightThreat> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetThreatState() != ENightThreatState::Resolved)
			{
				++ThreatsStanding;
			}
		}

		const float Remaining = Test_GetNightSecondsRemaining();
		const float BandW = FMath::Clamp(ViewW * 0.20f, 220.f, 380.f);
		const float BandX = (ViewW - BandW) * 0.5f;
		const float BandY = ViewH * 0.035f;

		DrawRect(Panel, BandX - 12.f, BandY - 10.f, BandW + 24.f, 52.f);
		DrawText(FString::Printf(TEXT("NIGHT   %02d:%02d"),
				FMath::FloorToInt(Remaining) / 60, FMath::FloorToInt(Remaining) % 60),
			Amber, BandX, BandY, GEngine->GetMediumFont(), 1.f);

		const FString ThreatLine = ThreatsStanding == 0
			? FString(TEXT("nothing walks"))
			: FString::Printf(TEXT("%d abroad in the dark"), ThreatsStanding);
		DrawText(ThreatLine, ThreatsStanding == 0 ? Dim : RotFill,
			BandX, BandY + 22.f, GEngine->GetSmallFont(), 1.f);
	}

	// ---- The Heart's words: the one thing the whole game asks you to interpret ----
	if (!StandingWarningText.IsEmpty())
	{
		const float WarnW = FMath::Clamp(ViewW * 0.46f, 340.f, 900.f);
		const float WarnX = (ViewW - WarnW) * 0.5f;
		const float WarnY = ViewH * 0.80f;

		DrawRect(Panel, WarnX - 16.f, WarnY - 22.f, WarnW + 32.f, 78.f);
		DrawText(TEXT("THE HEART SAYS"), Dim, WarnX, WarnY - 16.f, GEngine->GetSmallFont(), 1.f);
		DrawWrapped(WarnX, WarnY + 2.f, WarnW, StandingWarningText.ToString(), Amber, 1.15f);
	}

	// ---- What to do next, bottom-left, quiet ----
	const FString Objective = PhaseObjective(Phase);
	if (!Objective.IsEmpty())
	{
		DrawWrapped(PanelX, ViewH * 0.92f, FMath::Min(ViewW * 0.40f, 620.f), Objective, Dim, 1.f);
	}
}
