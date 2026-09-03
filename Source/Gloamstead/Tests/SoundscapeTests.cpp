// The sanctuary's voice.
//
// Gloamstead shipped with zero audio assets - not one wave, cue or MetaSound anywhere in Content/ -
// in a design whose whole subject is a place that speaks to you and a night you are meant to hear
// coming. Authoring audio content needs a human and source material; synthesising a bed does not, so
// the soundscape is generated in C++ and these are the parts of it a headless run can actually check.
//
// What is NOT checked here, and cannot be: whether it sounds good. There is no audio device under
// -nullrhi, and taste is not an assertion. These pin the things that would make it wrong rather than
// merely unpleasant - four phases that are actually distinguishable, a night that is the darkest of
// them, and a synth whose parameters and strikes survive the trip to the render thread.
#include "Misc/AutomationTest.h"

#include "Audio/GloamsteadSanctuarySynth.h"
#include "Audio/GloamsteadSoundscapeSubsystem.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GloamsteadSoundscapeFixtures
{
	// Named namespace: adaptive unity does not keep anonymous ones file-local in this module.
	const EGloamsteadDayPhase AllPhases[] = {
		EGloamsteadDayPhase::Day,
		EGloamsteadDayPhase::Dusk,
		EGloamsteadDayPhase::Night,
		EGloamsteadDayPhase::Dawn,
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamEveryPhaseHasItsOwnVoiceTest,
	"Gloamstead.Audio.EveryPhaseHasAVoiceOfItsOwn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamEveryPhaseHasItsOwnVoiceTest::RunTest(const FString& /*Parameters*/)
{
	using namespace GloamsteadSoundscapeFixtures;

	// Audible at all. A phase voiced at zero level is a phase that silently reintroduces the bug
	// this whole system exists to fix.
	for (const EGloamsteadDayPhase Phase : AllPhases)
	{
		const FGloamPhaseVoicing Voicing = UGloamsteadSoundscapeSubsystem::VoicingFor(Phase);
		TestTrue(*FString::Printf(TEXT("phase %d is audible"), static_cast<int32>(Phase)),
			Voicing.Level > 0.f);
		TestTrue(*FString::Printf(TEXT("phase %d has a real fundamental"), static_cast<int32>(Phase)),
			Voicing.RootHz > 20.f);
	}

	// Pairwise distinguishable on pitch. Two phases voiced identically would leave the player unable
	// to hear the transition at all, which is the one thing the bed is for.
	for (int32 A = 0; A < UE_ARRAY_COUNT(AllPhases); ++A)
	{
		for (int32 B = A + 1; B < UE_ARRAY_COUNT(AllPhases); ++B)
		{
			const FGloamPhaseVoicing First = UGloamsteadSoundscapeSubsystem::VoicingFor(AllPhases[A]);
			const FGloamPhaseVoicing Second = UGloamsteadSoundscapeSubsystem::VoicingFor(AllPhases[B]);
			TestTrue(
				*FString::Printf(TEXT("phases %d and %d are not voiced identically"),
					static_cast<int32>(AllPhases[A]), static_cast<int32>(AllPhases[B])),
				!FMath::IsNearlyEqual(First.RootHz, Second.RootHz, 1.f));
		}
	}

	const FGloamPhaseVoicing Day = UGloamsteadSoundscapeSubsystem::VoicingFor(EGloamsteadDayPhase::Day);
	const FGloamPhaseVoicing Dusk = UGloamsteadSoundscapeSubsystem::VoicingFor(EGloamsteadDayPhase::Dusk);
	const FGloamPhaseVoicing Night = UGloamsteadSoundscapeSubsystem::VoicingFor(EGloamsteadDayPhase::Night);
	const FGloamPhaseVoicing Dawn = UGloamsteadSoundscapeSubsystem::VoicingFor(EGloamsteadDayPhase::Dawn);

	// The shape of the day, asserted rather than assumed: it descends into the night and opens at
	// dawn. Someone retuning these by ear should not be able to invert that without being told.
	TestTrue(TEXT("dusk sits below day"), Dusk.RootHz < Day.RootHz);
	TestTrue(TEXT("night is the lowest voicing in the game"),
		Night.RootHz < Dusk.RootHz && Night.RootHz < Day.RootHz && Night.RootHz < Dawn.RootHz);
	TestTrue(TEXT("night is the darkest voicing in the game"),
		Night.Brightness < Day.Brightness && Night.Brightness < Dusk.Brightness
			&& Night.Brightness < Dawn.Brightness);
	TestTrue(TEXT("dawn is the brightest voicing in the game"),
		Dawn.Brightness > Day.Brightness && Dawn.Brightness > Dusk.Brightness);

	// Only the night moves on its own. A resting phase that wobbles reads as a fault in the mix.
	TestTrue(TEXT("the night is the only phase that moves under itself"), Night.TremoloHz > 0.f);
	TestTrue(TEXT("day rests still"), FMath::IsNearlyZero(Day.TremoloHz));
	TestTrue(TEXT("dawn rests still"), FMath::IsNearlyZero(Dawn.TremoloHz));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamSynthAcceptsVoicingTest,
	"Gloamstead.Audio.TheSynthTakesAVoicingAndAStrike",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamSynthAcceptsVoicingTest::RunTest(const FString& /*Parameters*/)
{
	// Never Start()ed: there is no audio device under -nullrhi, and this is about the parameter
	// hand-off from the game thread, not about rendering.
	UGloamsteadSanctuarySynth* Synth =
		NewObject<UGloamsteadSanctuarySynth>(GetTransientPackage(), TEXT("ArcSynthUnderTest"));
	if (!TestNotNull(TEXT("the sanctuary synth constructs"), Synth))
	{
		return false;
	}

	// Silent until voiced. A synth that defaults to audible would drone through the main menu.
	TestTrue(TEXT("the synth starts silent"), FMath::IsNearlyZero(Synth->Test_GetTargetLevel()));

	Synth->SetVoicing(/*RootHz*/ 73.42f, /*Brightness*/ 0.16f, /*Level*/ 0.34f,
		/*TremoloHz*/ 1.15f, /*Unease*/ 0.5f);
	TestEqual(TEXT("the synth takes the voicing's fundamental"), Synth->Test_GetTargetRootHz(), 73.42f);
	TestEqual(TEXT("the synth takes the voicing's level"), Synth->Test_GetTargetLevel(), 0.34f);

	// Clamped, not trusted. These values reach a render thread that must never be handed a NaN or a
	// gain above unity by a caller doing arithmetic on a corruption value.
	Synth->SetVoicing(/*RootHz*/ 999999.f, 5.f, 12.f, 900.f, 44.f);
	TestTrue(TEXT("an absurd fundamental is clamped into the audible range"),
		Synth->Test_GetTargetRootHz() <= 2000.f);
	TestTrue(TEXT("gain above unity is clamped"), Synth->Test_GetTargetLevel() <= 1.f);

	const int32 Before = Synth->Test_GetStrikeCount();
	Synth->Strike(587.33f, 0.42f, 3.2f);
	TestEqual(TEXT("a struck tone is queued for the render thread"),
		Synth->Test_GetStrikeCount(), Before + 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
