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
	static TAutoConsoleVariable<bool> CVarEngineMessages(
		TEXT("gloam.HUD.AllowEngineScreenMessages"),
		false,
		TEXT("Let engine on-screen debug messages (Blueprint Print String) draw over the game. Off by "
			 "default: BP_FirstNightDirector captions with Print String nodes, which render as blue "
			 "debug text stacked at the top-left, directly over the sanctuary readout - and every one "
			 "of those lines is already presented properly by the caption widget and the HUD. Set 1 to "
			 "get them back while debugging Blueprints."),
		ECVF_Default);

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

	/**
	 * The bottom strip belongs to the caption widgets, and this HUD must not draw into it.
	 *
	 * WBP_FirstNightCaption and WBP_GloamPrompt are both added with AddToPlayerScreen and both sit
	 * bottom-centre, and BP_FirstNightDirector additionally captions with Print String nodes that
	 * render as engine screen messages. None of those are surfaces this actor owns or can move, and
	 * the result was the Heart's warning, the dawn verdict and the ending reckoning all being drawn
	 * through by text from three different systems.
	 *
	 * Yielding the strip is the fix that does not require deleting anyone else's content.
	 */
	constexpr float CaptionBandTop = 0.86f;

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

	/**
	 * The night's verdict as a headline, in the Heart's register rather than the enum's.
	 *
	 * The enum names (Success/Partial/Failure) are the right words for the code and the wrong words
	 * for the screen: this game never tells the player they won, it tells them what the sanctuary
	 * did. GetNightOutcomeResultDisplayName stays the debug and telemetry spelling.
	 */
	/**
	 * Channel types are authored as FNames like "ObjectReaction". Lower-casing them wholesale
	 * printed "objectreaction" on screen, which reads as a typo rather than as a medium.
	 */
	FString ChannelWords(FName ChannelType)
	{
		if (ChannelType.IsNone())
		{
			return TEXT("something");
		}
		const FString Raw = ChannelType.ToString();
		FString Out;
		Out.Reserve(Raw.Len() + 4);
		for (int32 i = 0; i < Raw.Len(); ++i)
		{
			const TCHAR C = Raw[i];
			if (i > 0 && FChar::IsUpper(C) && !FChar::IsUpper(Raw[i - 1]))
			{
				Out.AppendChar(TEXT(' '));
			}
			Out.AppendChar(FChar::ToLower(C));
		}
		return Out;
	}

	FString VerdictHeadline(ENightOutcomeResult Result)
	{
		switch (Result)
		{
		case ENightOutcomeResult::Success: return TEXT("THE SANCTUARY HELD");
		case ENightOutcomeResult::Partial: return TEXT("THE DARK RECEDED, AND LINGERS");
		case ENightOutcomeResult::Failure: return TEXT("A SCAR REMAINS");
		default:                           return TEXT("THE NIGHT PASSED");
		}
	}

	FLinearColor VerdictColor(ENightOutcomeResult Result)
	{
		switch (Result)
		{
		case ENightOutcomeResult::Success: return LightFill;
		case ENightOutcomeResult::Partial: return Ink;
		case ENightOutcomeResult::Failure: return RotFill;
		default:                           return Dim;
		}
	}

	/** A short verdict word for the ending's per-night table, where the headline is too long. */
	FString VerdictWord(ENightOutcomeResult Result)
	{
		switch (Result)
		{
		case ENightOutcomeResult::Success: return TEXT("held");
		case ENightOutcomeResult::Partial: return TEXT("lingering");
		case ENightOutcomeResult::Failure: return TEXT("scarred");
		default:                           return TEXT("unread");
		}
	}

	/** Tags are authored as "Boon.PathLoop" / "Scar.DeadEnd"; the screen wants the part after the dot. */
	FString TagLeaf(FName Tag)
	{
		if (Tag.IsNone())
		{
			return FString();
		}
		const FString Full = Tag.ToString();
		FString Left;
		FString Right;
		return Full.Split(TEXT("."), &Left, &Right) ? Right : Full;
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

	// Says out loud that the readout reached the player. HUDClass is set on AGloamsteadGameMode in
	// C++, and the shipped game-mode Blueprint overrides its pawn and controller but not its HUD -
	// which is a claim about a binary asset, and therefore one worth confirming from a real boot
	// rather than inferring. A map that logs nothing here is a map with no readout.
	// Suppress engine screen messages in the shipped game. This is a presentation decision, not a
	// silencing one: the lines are Blueprint Print Strings, they still go to the log, the CVar turns
	// them back on, and their content already reaches the player through the caption widget and this
	// readout. What they were doing on screen was drawing blue debug text across the meters.
	if (GEngine && GetWorld() && GetWorld()->IsGameWorld()
		&& !GloamsteadHUDDraw::CVarEngineMessages.GetValueOnGameThread())
	{
		GEngine->bEnableOnScreenDebugMessages = false;
		GEngine->ClearOnScreenDebugMessages();
	}

	UE_LOG(LogTemp, Log, TEXT("GloamsteadHUD: the sanctuary readout is live (phase authority %s)."),
		DayNight.IsValid() ? TEXT("found") : TEXT("MISSING"));
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
		if (bDawnBound)
		{
			Heart->OnDawnReflectionDelegate.RemoveDynamic(this, &AGloamsteadHUD::HandleDawnReflection);
		}
	}
	bBound = false;
	bDawnBound = false;

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
			// The same argument applies twice over to the dawn outcome: the Heart clears its
			// per-cycle interpretation state inside the very call that broadcasts the reflection,
			// so a poll one frame later reads the next cycle instead of the night just endured.
			Heart->OnDawnReflectionDelegate.AddDynamic(this, &AGloamsteadHUD::HandleDawnReflection);
			bDawnBound = true;
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

void AGloamsteadHUD::HandleDawnReflection(const FNightRuntimeOutcome& Outcome)
{
	// The cycle the night belonged to, not the one now arming. The Heart reflects before the next
	// plan is armed, so the active slot is still the night it just resolved - but a reflection that
	// arrived after the handoff would otherwise be filed under a cycle the player never played.
	// With no cycle authority at all the only honest answer is "the next one", which keeps distinct
	// nights distinct at the cost of not recognising a repeat - a trade that only ever applies in a
	// world with no experience subsystem, which is not a world anyone plays.
	const int32 Cycle = Cycles.IsValid() && Cycles->GetActivePlan().Slot > 0
		? Cycles->GetActivePlan().Slot
		: NightLedger.Num() + 1;

	RecordNightIntoLedger(Cycle, Outcome);
}

void AGloamsteadHUD::RecordNightIntoLedger(int32 Cycle, const FNightRuntimeOutcome& Outcome)
{
	LastDawnOutcome = Outcome;
	bHasDawnOutcome = true;

	FGloamNightRecord Record;
	Record.Cycle = Cycle;
	Record.Result = Outcome.Result;
	Record.NightType = Outcome.NightType;
	Record.ResultTag = Outcome.ResultTag;
	Record.Grade = Outcome.SecondReadingGrade;
	Record.ReadingTag = Outcome.SecondReadingTag;
	Record.bWarningHeeded = Outcome.bWarningHeeded;

	// Dawn reflection can be driven more than once for a single night: the BP-compat entry point
	// exists and passes a default outcome. One row per cycle, last writer wins, so the ledger stays
	// the length of the arc rather than the number of times something called reflect.
	const int32 Existing = NightLedger.IndexOfByPredicate(
		[&Record](const FGloamNightRecord& R) { return R.Cycle == Record.Cycle; });
	if (Existing != INDEX_NONE)
	{
		NightLedger[Existing] = Record;
	}
	else
	{
		NightLedger.Add(Record);
	}
}

FString AGloamsteadHUD::Test_GetDawnVerdictLine() const
{
	if (!bHasDawnOutcome)
	{
		return FString();
	}
	return GloamsteadHUDDraw::VerdictHeadline(LastDawnOutcome.Result);
}

bool AGloamsteadHUD::ShouldDrawDawnSummary() const
{
	return bHasDawnOutcome
		&& DayNight.IsValid()
		&& DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dawn;
}

void AGloamsteadHUD::DrawEvidenceJournal(float X, float Y, float Width)
{
	using namespace GloamsteadHUDDraw;

	if (!Heart.IsValid())
	{
		return;
	}

	TArray<FVeilHeartEvidenceLine> Lines;
	int32 Required = 0;
	if (!Heart->GetStandingEvidence(Lines, Required) || Lines.Num() == 0)
	{
		return;
	}

	int32 Found = 0;
	for (const FVeilHeartEvidenceLine& Line : Lines)
	{
		Found += Line.bFound ? 1 : 0;
	}

	const bool bGateMet = Required <= 0 || Found >= Required;

	DrawRect(Panel, X - 12.f, Y - 12.f, Width + 24.f, 34.f + Lines.Num() * 30.f);

	DrawText(TEXT("WHAT YOU HAVE FOUND"), Dim, X, Y, GEngine->GetSmallFont(), 1.f);
	// The gate is stated, always. It is a hard condition on the cycle, and a player who cannot see
	// it is being asked to satisfy a rule the game never told them.
	DrawText(FString::Printf(TEXT("%d / %d"), Found, FMath::Max(Required, 1)),
		bGateMet ? LightFill : Amber, X + Width - 34.f, Y, GEngine->GetSmallFont(), 1.f);
	Y += 20.f;

	for (const FVeilHeartEvidenceLine& Line : Lines)
	{
		if (Line.bFound)
		{
			const FString Said = Line.EvidenceText.IsEmpty()
				// Reachable when a catalog authors a channel without its sentence. Naming the medium
				// is still a true thing to say, and beats a blank row that reads as a bug.
				? FString::Printf(TEXT("something %s, and you have read it"), *ChannelWords(Line.ChannelType))
				: Line.EvidenceText.ToString();
			Y = DrawWrapped(X + 12.f, Y, Width - 12.f, Said, Ink, 1.f) + 6.f;
		}
		else
		{
			// The medium, never the sentence. Knowing one clue is something you hear rather than
			// something you see is direction; knowing what it says is the answer.
			DrawText(FString::Printf(TEXT("- %s, not yet found"), *ChannelWords(Line.ChannelType)),
				Dim, X + 12.f, Y, GEngine->GetSmallFont(), 1.f);
			Y += 22.f;
		}
	}
}

void AGloamsteadHUD::DrawDawnSummary()
{
	using namespace GloamsteadHUDDraw;

	const float ViewW = Canvas->ClipX;
	const float ViewH = Canvas->ClipY;

	const float PanelW = FMath::Clamp(ViewW * 0.40f, 380.f, 760.f);
	const float X = (ViewW - PanelW) * 0.5f;
	float Y = ViewH * 0.24f;

	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.80f),
		X - 26.f, Y - 34.f, PanelW + 52.f, 212.f);

	DrawText(TEXT("DAWN"), Dim, X, Y - 26.f, GEngine->GetSmallFont(), 1.f);

	DrawText(VerdictHeadline(LastDawnOutcome.Result), VerdictColor(LastDawnOutcome.Result),
		X, Y, GEngine->GetMediumFont(), 1.35f);
	Y += 34.f;

	DrawText(FString::Printf(TEXT("the %s night"),
			*GetNightConsequenceTypeDisplayName(LastDawnOutcome.NightType).ToLower()),
		Dim, X, Y, GEngine->GetSmallFont(), 1.f);
	Y += 26.f;

	// Three facts, in the order the player earned them: did you read it, how did you read it, and
	// what did that do to the ground. Anything the night did not establish is stated as absent
	// rather than left blank - a missing row and an unheeded warning look identical otherwise.
	const float LabelX = X;
	const float ValueX = X + PanelW * 0.38f;
	auto Row = [&](const TCHAR* Label, const FString& Value, const FLinearColor& Color)
	{
		DrawText(Label, Dim, LabelX, Y, GEngine->GetSmallFont(), 1.f);
		DrawText(Value, Color, ValueX, Y, GEngine->GetSmallFont(), 1.f);
		Y += 19.f;
	};

	Row(TEXT("the warning"),
		LastDawnOutcome.bWarningHeeded ? TEXT("heeded") : TEXT("unheeded"),
		LastDawnOutcome.bWarningHeeded ? LightFill : RotFill);

	const FString ReadingLeaf = TagLeaf(LastDawnOutcome.SecondReadingTag);
	const FString GradeName = GetExperienceReadingGradeDisplayName(LastDawnOutcome.SecondReadingGrade);
	const FString ReadingValue = LastDawnOutcome.SecondReadingGrade == EExperienceReadingGrade::Unread
		? FString(TEXT("you took the warning at its word"))
		: (ReadingLeaf.IsEmpty() ? GradeName : GradeName + TEXT("  -  ") + ReadingLeaf);
	Row(TEXT("your second reading"), ReadingValue,
		LastDawnOutcome.SecondReadingGrade == EExperienceReadingGrade::Insight   ? LightFill :
		LastDawnOutcome.SecondReadingGrade == EExperienceReadingGrade::Overreach ? RotFill   : Ink);

	// Sign convention: TargetCorruptionDelta is positive when the bloom worsened. Saying "receded
	// by" over a negated negative is the one place this panel does arithmetic, so it is spelled out.
	const float Delta = LastDawnOutcome.TargetCorruptionDelta;
	const FString BloomValue = FMath::IsNearlyZero(Delta, 0.005f)
		? FString(TEXT("unchanged"))
		: (Delta < 0.f
			? FString::Printf(TEXT("receded by %.0f%%"), -Delta * 100.f)
			: FString::Printf(TEXT("worsened by %.0f%%"), Delta * 100.f));
	Row(TEXT("the bloom"), BloomValue, Delta < 0.f ? LightFill : (Delta > 0.f ? RotFill : Dim));

	if (!LastDawnOutcome.ResultTag.IsNone())
	{
		Row(TEXT("carried forward"), TagLeaf(LastDawnOutcome.ResultTag), Amber);
	}

	Y += 12.f;
	DrawText(TEXT("[E] at the Heart to go on"), Amber, X, Y, GEngine->GetSmallFont(), 1.1f);
}

void AGloamsteadHUD::DrawEndingReckoning()
{
	using namespace GloamsteadHUDDraw;

	const float ViewW = Canvas->ClipX;
	const float ViewH = Canvas->ClipY;

	DrawRect(FLinearColor(0.008f, 0.008f, 0.012f, 0.94f), 0.f, 0.f, ViewW, ViewH);

	const float PanelW = FMath::Clamp(ViewW * 0.52f, 460.f, 900.f);
	const float X = (ViewW - PanelW) * 0.5f;
	float Y = ViewH * 0.06f;

	// The proper name, used once, at the end. Everywhere else in the game it is "the Heart".
	DrawText(TEXT("THE GLOAMHEART IS QUIET"), Amber, X, Y, GEngine->GetMediumFont(), 1.6f);
	Y += 40.f;
	DrawText(TEXT("what the sanctuary remembers of you"), Dim, X, Y, GEngine->GetSmallFont(), 1.f);
	Y += 34.f;

	int32 Held = 0;
	int32 Lingering = 0;
	int32 Scarred = 0;

	const float CycleX   = X;
	const float NightX   = X + PanelW * 0.12f;
	const float VerdictX = X + PanelW * 0.44f;
	const float ReadingX = X + PanelW * 0.64f;

	for (const FGloamNightRecord& Record : NightLedger)
	{
		switch (Record.Result)
		{
		case ENightOutcomeResult::Success: ++Held; break;
		case ENightOutcomeResult::Partial: ++Lingering; break;
		case ENightOutcomeResult::Failure: ++Scarred; break;
		default: break;
		}

		DrawText(RomanNumeral(Record.Cycle), Dim, CycleX, Y, GEngine->GetSmallFont(), 1.f);
		DrawText(GetNightConsequenceTypeDisplayName(Record.NightType).ToLower(),
			Ink, NightX, Y, GEngine->GetSmallFont(), 1.f);
		DrawText(VerdictWord(Record.Result), VerdictColor(Record.Result),
			VerdictX, Y, GEngine->GetSmallFont(), 1.f);

		if (Record.Grade != EExperienceReadingGrade::Unread)
		{
			const FString Leaf = TagLeaf(Record.ReadingTag);
			DrawText(Leaf.IsEmpty() ? GetExperienceReadingGradeDisplayName(Record.Grade) : Leaf,
				Record.Grade == EExperienceReadingGrade::Insight   ? LightFill :
				Record.Grade == EExperienceReadingGrade::Overreach ? RotFill   : Dim,
				ReadingX, Y, GEngine->GetSmallFont(), 1.f);
		}
		Y += 21.f;
	}

	if (NightLedger.Num() == 0)
	{
		// Reachable: a save loaded past the last cycle, or a reflection that never broadcast. Better
		// to say the ledger is empty than to draw a reckoning of nothing and imply the arc was blank.
		DrawText(TEXT("no night was recorded"), Dim, X, Y, GEngine->GetSmallFont(), 1.f);
		Y += 21.f;
	}

	Y += 22.f;
	DrawText(FString::Printf(TEXT("%d night%s:  %d held,  %d lingering,  %d scarred"),
			NightLedger.Num(), NightLedger.Num() == 1 ? TEXT("") : TEXT("s"),
			Held, Lingering, Scarred),
		Ink, X, Y, GEngine->GetSmallFont(), 1.1f);
	Y += 28.f;

	if (PCG.IsValid())
	{
		Y = DrawMeter(X, Y, PanelW * 0.52f, TEXT("SANCTUARY LIGHT"),
			PCG->GetSanctuaryAverageLightLevel(), LightFill);
		Y = DrawMeter(X, Y, PanelW * 0.52f, TEXT("CORRUPTION"),
			PCG->GetSanctuaryAverageCorruptionLevel(), RotFill);
	}

	Y += 16.f;

	// The closing sentence is earned from the ledger, not from reaching the screen. A player who
	// scarred half the arc and one who read every warning both arrive here, and telling them the
	// same thing would make the whole interpretation layer decorative.
	const FString Closing =
		(NightLedger.Num() > 0 && Scarred == 0 && Held == NightLedger.Num())
			? TEXT("You understood every warning it could give. The Gloam did not take one thing you had "
				   "raised, and the light you leave behind is the sanctuary's own again.")
		: (Scarred == 0)
			? TEXT("Nothing was lost that cannot come back. What lingers, lingers quietly, and the ground "
				   "you restored remembers who restored it.")
		: (Scarred < NightLedger.Num())
			? TEXT("Some of it you read wrong, and the sanctuary carries those places still. It also "
				   "carries the ones you read right, and it is standing.")
			: TEXT("You were here, and you tried to read it. The Gloam kept most of what it came for. "
				   "The Heart is quiet anyway, and that much you did.");
	Y = DrawWrapped(X, Y, PanelW, Closing, Ink, 1.15f);

	// Clamped rather than merely offset: the closing paragraph wraps to a variable number of lines
	// depending on which of the four endings the ledger earned, so a fixed offset put this line
	// inside the caption band for the longer ones and not the shorter ones.
	Y = FMath::Min(Y + 26.f, ViewH * CaptionBandTop - 26.f);
	DrawText(TEXT("[Esc] hold the sanctuary"), Dim, X, Y, GEngine->GetSmallFont(), 1.f);
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
	// The per-night ceiling, not the base constant: a countdown that disagreed with the timer it
	// claims to show would be a HUD lying about the one number the player is watching.
	return FMath::Max(0.f, DayNight->GetCurrentNightDurationSeconds() - Elapsed);
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

void AGloamsteadHUD::DrawTitleScreen()
{
	using namespace GloamsteadHUDDraw;

	const float ViewW = Canvas->ClipX;
	const float ViewH = Canvas->ClipY;

	// A scrim, not an opaque plate: the sanctuary is behind this, and the first thing the game
	// should say about itself is that there is a place there.
	DrawRect(FLinearColor(0.01f, 0.01f, 0.015f, 0.88f), 0.f, 0.f, ViewW, ViewH);

	const float PanelW = FMath::Clamp(ViewW * 0.50f, 440.f, 880.f);
	const float X = (ViewW - PanelW) * 0.5f;
	float Y = ViewH * 0.20f;

	DrawText(TEXT("GLOAMSTEAD"), Amber, X, Y, GEngine->GetMediumFont(), 2.0f);
	Y += 52.f;
	DrawText(TEXT("a sanctuary, and six nights of it"), Dim, X, Y, GEngine->GetSmallFont(), 1.f);
	Y += 40.f;

	Y = DrawWrapped(X, Y, PanelW,
		TEXT("The Heart speaks once a day, and never plainly. Read what it says, find what backs it "
			 "up, and restore the place it names before the dark comes for it."),
		Ink, 1.15f) + 26.f;

	// The controls, here rather than only behind Escape. This game binds two of its verbs at key
	// level rather than through an input-mapping asset, so a player who never opens the pause screen
	// has nowhere else to learn that Strike and Ward exist.
	const float KeyColumn = X;
	const float VerbColumn = X + PanelW * 0.24f;
	const float MeaningColumn = X + PanelW * 0.44f;
	for (const FGloamControlRow& Row : GetControlRows())
	{
		DrawText(Row.Key, Ink, KeyColumn, Y, GEngine->GetSmallFont(), 1.f);
		DrawText(Row.Verb, LightFill, VerbColumn, Y, GEngine->GetSmallFont(), 1.f);
		if (Row.Meaning && *Row.Meaning)
		{
			DrawText(Row.Meaning, Dim, MeaningColumn, Y, GEngine->GetSmallFont(), 1.f);
		}
		Y += 20.f;
	}

	Y += 26.f;
	DrawText(TEXT("[E]  step into it"), Amber, X, Y, GEngine->GetMediumFont(), 1.2f);
	Y += 26.f;
	DrawText(TEXT("[Esc]  hold, at any time"), Dim, X, Y, GEngine->GetSmallFont(), 1.f);
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

	// Before anything else: the opening screen. Drawn ahead of the pause check so a player who opens
	// the title and presses Escape does not end up holding a sanctuary they have not entered.
	if (bTitleScreenPending)
	{
		DrawTitleScreen();
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

	// The end of the authored experience is a screen, not a log line. Until now IsExperienceComplete
	// had exactly one production reader - a one-line note on the left panel - so a player who
	// finished all six nights saw the readout carry on as though a seventh were coming.
	if (Cycles.IsValid() && Cycles->IsExperienceComplete())
	{
		DrawEndingReckoning();
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

	// ---- Day and dusk: what the standing warning is backed by, and how much of it you hold ----
	if (Phase == EGloamsteadDayPhase::Day || Phase == EGloamsteadDayPhase::Dusk)
	{
		DrawEvidenceJournal(PanelX, PanelY + 190.f, PanelW);
	}

	// ---- Dawn: answer the night the player just endured ----
	if (ShouldDrawDawnSummary())
	{
		DrawDawnSummary();
	}

	// ---- The Heart's words: the one thing the whole game asks you to interpret ----
	if (!StandingWarningText.IsEmpty())
	{
		const float WarnW = FMath::Clamp(ViewW * 0.46f, 340.f, 900.f);
		const float WarnX = (ViewW - WarnW) * 0.5f;
		const float WarnY = ViewH * (CaptionBandTop - 0.14f);

		DrawRect(Panel, WarnX - 16.f, WarnY - 22.f, WarnW + 32.f, 78.f);
		DrawText(TEXT("THE HEART SAYS"), Dim, WarnX, WarnY - 16.f, GEngine->GetSmallFont(), 1.f);
		DrawWrapped(WarnX, WarnY + 2.f, WarnW, StandingWarningText.ToString(), Amber, 1.15f);
	}

	// ---- What to do next, bottom-left, quiet ----
	const FString Objective = PhaseObjective(Phase);
	if (!Objective.IsEmpty())
	{
		DrawWrapped(PanelX, ViewH * (CaptionBandTop - 0.04f), FMath::Min(ViewW * 0.40f, 620.f), Objective, Dim, 1.f);
	}
}
