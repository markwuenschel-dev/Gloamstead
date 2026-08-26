#pragma once

#include "CoreMinimal.h"
#include "Data/ExperienceCycleTypes.h"
#include "Data/NightConsequenceTypes.h"
#include "NightThreatTypes.generated.h"

/**
 * The whole enemy roster, and it is deliberately this short.
 *
 * Every archetype teaches a different relationship to restoration, and the climax recombines the
 * three the player already knows rather than introducing a sixth thing to learn on the last night.
 * Nothing here is a damage-race: each one is pressure applied while the player does something else.
 */
UENUM(BlueprintType)
enum class ENightThreatArchetype : uint8
{
	None = 0,

	/**
	 * Cycle III. Tall, unfinished, long-reaching. It has no interest in the player unless obstructed:
	 * it walks to a restored structure and takes the light out of it. Connected light slows it to a
	 * crawl; darkness lets it run. The player beats it by maintaining the route and forcing it back
	 * into the light, not by killing it.
	 */
	Gatherer = 1,

	/**
	 * Cycle IV. The Gloam wearing the shape of something the sanctuary once knew. In ordinary sight
	 * it barely takes damage. A correctly faced mirror exposes the tether between it and the thing it
	 * has possessed; Strike interrupts, Cleanse is what actually resolves it.
	 */
	Borrowed = 2,

	/**
	 * Cycle V. It does not chase. It stands at the edge of the light and offers shortcuts - false
	 * prompts, false safe ground, a passable imitation of a fragment the Heart already gave. The
	 * restored bell dismisses it. The Heart stays truthful; this is the thing that is allowed to lie.
	 */
	Bargainer = 3,

	/**
	 * A companion manifestation rather than a fourth enemy family. It repeats what it just saw, a few
	 * seconds late and in the wrong place: a silhouette walking to the wrong objective, a lantern
	 * lighting where none stands, a bell answering itself. It costs the player attention, not health.
	 */
	Echo = 4,
};

GLOAMSTEAD_API FString GetNightThreatArchetypeDisplayName(ENightThreatArchetype Archetype);

/** What a threat walks toward when it has no reason to care about the player. */
UENUM(BlueprintType)
enum class ENightThreatTarget : uint8
{
	/** The night's authored objective point. */
	ObjectivePoint = 0,
	/** The brightest restored point - what a thief would come for. */
	BrightestRestored = 1,
	/** Holds at the edge of the light and waits to be answered. */
	EdgeOfLight = 2,
};

/** One authored line of a night's threat roster. */
USTRUCT(BlueprintType)
struct GLOAMSTEAD_API FNightThreatSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night|Threat")
	ENightThreatArchetype Archetype = ENightThreatArchetype::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night|Threat", meta = (ClampMin = "0", ClampMax = "3"))
	int32 Count = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night|Threat")
	ENightThreatTarget TargetPreference = ENightThreatTarget::ObjectivePoint;

	/**
	 * Light level at or above which this threat is held off entirely.
	 *
	 * Every threat in Gloamstead is light-vulnerable; this is how much light it takes. It is the tie
	 * between the combat layer and the restoration fantasy: the answer to a threat is always
	 * something the player built, never a weapon they found.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Night|Threat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RepelledAtLightLevel = 0.65f;

	bool IsAuthored() const { return Archetype != ENightThreatArchetype::None && Count > 0; }
};

/**
 * The threats one night actually fields.
 *
 * The cap is the design's, not an implementation limit: 1-3 light-vulnerable threats acting as
 * pressure while the player cleanses or activates something. BuildNightThreatRoster clamps to it
 * rather than trusting its own arithmetic, because "the climax got a fourth enemy" is exactly the
 * drift the constraint exists to catch.
 */
USTRUCT(BlueprintType)
struct GLOAMSTEAD_API FNightThreatRoster
{
	GENERATED_BODY()

	/** The design ceiling on simultaneous threats. Gloamstead is not an action game. */
	static constexpr int32 MaxSimultaneousThreats = 3;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Threat")
	TArray<FNightThreatSpec> Specs;

	int32 TotalCount() const
	{
		int32 Total = 0;
		for (const FNightThreatSpec& Spec : Specs)
		{
			Total += FMath::Max(0, Spec.Count);
		}
		return Total;
	}

	bool IsEmpty() const { return TotalCount() == 0; }
};

/**
 * Composes the roster for one night from three facts the player can trace back to their own choices:
 * which cycle this is, whether they read the warning at all, and what they did with its second clause.
 *
 * The grade is where the mechanic pays off. An Insight reading does not delete the night's threat -
 * that would make the sharper read a skip button - it removes the EXTRA one and raises the light
 * level at which the rest are held off, so the sanctuary the player configured does the work. An
 * Overreach adds exactly one manifestation, which is what "three answers invite company" means when
 * it stops being a sentence and becomes a night.
 */
GLOAMSTEAD_API FNightThreatRoster BuildNightThreatRoster(
	ENightConsequenceType NightType,
	EExperienceReadingGrade ReadingGrade,
	bool bWarningHeeded);
