// Focus-selection invariants for UGloamInteractionComponent.
//
// The component's focus decision is the pure static FindBestInteractableIndex (view-cone + range
// scoring), deliberately free of world/trace dependencies so it is fully deterministic here. The
// world-querying tick is a thin shell over it and is exercised in PIE, not in automation. The final
// test confirms the component's configured range/cone actually plumb into the selector via the seam.
#include "Misc/AutomationTest.h"
#include "Components/GloamInteractionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Generous range / 60°-ish cone for the pure-math tests (component-config plumbing is tested separately).
	constexpr float TestRange = 1000.0f;
	constexpr float TestMinDot = 0.5f;
	const FVector ViewOrigin = FVector::ZeroVector;
	const FVector ViewForward = FVector(1.f, 0.f, 0.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamInteractionSelectorEmptyTest,
	"Gloamstead.Interaction.SelectorEmptyReturnsNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamInteractionSelectorEmptyTest::RunTest(const FString& /*Parameters*/)
{
	const TArray<FVector> None;
	TestEqual(TEXT("empty candidate list selects nothing"),
		UGloamInteractionComponent::FindBestInteractableIndex(None, ViewOrigin, ViewForward, TestRange, TestMinDot),
		INDEX_NONE);
	return true;
}

// In-range + in-cone qualifies; behind the view or beyond range is rejected.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamInteractionSelectorRangeConeTest,
	"Gloamstead.Interaction.SelectorRespectsRangeAndCone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamInteractionSelectorRangeConeTest::RunTest(const FString& /*Parameters*/)
{
	// Single candidate dead ahead, in range -> chosen.
	{
		const TArray<FVector> Ahead = { FVector(500.f, 0.f, 0.f) };
		TestEqual(TEXT("candidate ahead in range is chosen"),
			UGloamInteractionComponent::FindBestInteractableIndex(Ahead, ViewOrigin, ViewForward, TestRange, TestMinDot), 0);
	}
	// Behind the view -> rejected.
	{
		const TArray<FVector> Behind = { FVector(-500.f, 0.f, 0.f) };
		TestEqual(TEXT("candidate behind the view is rejected"),
			UGloamInteractionComponent::FindBestInteractableIndex(Behind, ViewOrigin, ViewForward, TestRange, TestMinDot), INDEX_NONE);
	}
	// Beyond range -> rejected.
	{
		const TArray<FVector> Far = { FVector(1500.f, 0.f, 0.f) };
		TestEqual(TEXT("candidate beyond range is rejected"),
			UGloamInteractionComponent::FindBestInteractableIndex(Far, ViewOrigin, ViewForward, TestRange, TestMinDot), INDEX_NONE);
	}
	// Just outside the cone (~70°, dot ~0.34 < 0.5) -> rejected.
	{
		const float Rad = FMath::DegreesToRadians(70.f);
		const TArray<FVector> WideAngle = { FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.f) * 500.f };
		TestEqual(TEXT("candidate outside the view cone is rejected"),
			UGloamInteractionComponent::FindBestInteractableIndex(WideAngle, ViewOrigin, ViewForward, TestRange, TestMinDot), INDEX_NONE);
	}
	return true;
}

// Alignment dominates; distance only breaks ties between equally-aligned candidates.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamInteractionSelectorScoringTest,
	"Gloamstead.Interaction.SelectorPrefersAlignmentThenDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamInteractionSelectorScoringTest::RunTest(const FString& /*Parameters*/)
{
	// Index 0: 45° off but nearer; Index 1: dead ahead but farther. Alignment wins -> index 1.
	{
		const TArray<FVector> Candidates = {
			FVector(300.f, 300.f, 0.f),   // dot ~0.707, dist ~424
			FVector(700.f, 0.f, 0.f)      // dot 1.0,   dist 700
		};
		TestEqual(TEXT("more-aligned candidate wins even when farther"),
			UGloamInteractionComponent::FindBestInteractableIndex(Candidates, ViewOrigin, ViewForward, TestRange, TestMinDot), 1);
	}
	// Two equally aligned (both dead ahead) -> nearer one wins (index 0).
	{
		const TArray<FVector> Candidates = {
			FVector(150.f, 0.f, 0.f),
			FVector(600.f, 0.f, 0.f)
		};
		TestEqual(TEXT("equal alignment breaks to the nearer candidate"),
			UGloamInteractionComponent::FindBestInteractableIndex(Candidates, ViewOrigin, ViewForward, TestRange, TestMinDot), 0);
	}
	return true;
}

// The component's configured InteractionRange / MinViewConeDot actually feed the selector.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamInteractionConfigPlumbingTest,
	"Gloamstead.Interaction.ComponentConfigPlumbsToSelector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamInteractionConfigPlumbingTest::RunTest(const FString& /*Parameters*/)
{
	UGloamInteractionComponent* Component = NewObject<UGloamInteractionComponent>();

	// Default config is InteractionRange = 350. A target ahead at 300 is in range; at 500 is not.
	const TArray<FVector> InRange = { FVector(300.f, 0.f, 0.f) };
	TestEqual(TEXT("target within the component's default range is selected"),
		Component->Test_SelectFromLocations(InRange, ViewOrigin, ViewForward), 0);

	const TArray<FVector> OutOfRange = { FVector(500.f, 0.f, 0.f) };
	TestEqual(TEXT("target beyond the component's default range is rejected"),
		Component->Test_SelectFromLocations(OutOfRange, ViewOrigin, ViewForward), INDEX_NONE);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
