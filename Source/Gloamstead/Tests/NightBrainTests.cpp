// Gloamstead "night brain" invariants — night selection, MVP catalog, and Veil Heart warning matching.
#include "Misc/AutomationTest.h"
#include "Systems/NightConsequenceManager.h"
#include "Systems/VeilHeart.h"
#include "Data/NightConsequenceTypes.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Data/RitualTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

// PopulateMVPNightConsequenceRules produces exactly the Tutorial / Corruption / Omen set.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamNightMVPCatalogTest,
    "Gloamstead.Night.MVPCatalogPopulates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightMVPCatalogTest::RunTest(const FString& /*Parameters*/)
{
    UNightConsequenceCatalog* Catalog = NewObject<UNightConsequenceCatalog>();
    PopulateMVPNightConsequenceRules(*Catalog);

    if (!TestEqual(TEXT("MVP catalog has 3 rules"), Catalog->Rules.Num(), 3))
    {
        return false;
    }
    TestTrue(TEXT("rule 0 is Tutorial"), Catalog->Rules[0].NightType == ENightConsequenceType::Tutorial);
    TestTrue(TEXT("rule 1 is Corruption"), Catalog->Rules[1].NightType == ENightConsequenceType::Corruption);
    TestTrue(TEXT("rule 2 is Omen"), Catalog->Rules[2].NightType == ENightConsequenceType::Omen);
    TestEqual(TEXT("Tutorial weight 10"), Catalog->Rules[0].Weight, 10.f, KINDA_SMALL_NUMBER);
    TestTrue(TEXT("fallback is Corruption"), Catalog->FallbackNightType == ENightConsequenceType::Corruption);
    TestTrue(TEXT("force-tutorial-on-first-night set"), Catalog->bForceTutorialOnFirstNight);
    TestTrue(TEXT("Omen rule carries its clue tag"), Catalog->Rules[2].OmenClueTag == FName(TEXT("GardenRot")));
    TestNotEqual(TEXT("Retrieval has display name"), GetNightConsequenceTypeDisplayName(ENightConsequenceType::Retrieval), FString(TEXT("Invalid")));
    TestNotEqual(TEXT("SilencePossession has display name"), GetNightConsequenceTypeDisplayName(ENightConsequenceType::SilencePossession), FString(TEXT("Invalid")));
    TestNotEqual(TEXT("Mirror has display name"), GetNightConsequenceTypeDisplayName(ENightConsequenceType::Mirror), FString(TEXT("Invalid")));
    TestNotEqual(TEXT("Bargain has display name"), GetNightConsequenceTypeDisplayName(ENightConsequenceType::Bargain), FString(TEXT("Invalid")));
    TestNotEqual(TEXT("Fracture has display name"), GetNightConsequenceTypeDisplayName(ENightConsequenceType::Fracture), FString(TEXT("Invalid")));
    TestNotEqual(TEXT("TrueSiege has display name"), GetNightConsequenceTypeDisplayName(ENightConsequenceType::TrueSiege), FString(TEXT("Invalid")));
    TestNotEqual(TEXT("MirrorPillar has display name"), GetRitualTypeDisplayName(ERitualType::MirrorPillar), FString(TEXT("Invalid")));
    TestNotEqual(TEXT("BellShrine has display name"), GetRitualTypeDisplayName(ERitualType::BellShrine), FString(TEXT("Invalid")));
    return true;
}

// Same snapshot -> same chosen night type (determinism), and Tutorial's high weight dominates the MVP set.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamNightSelectionDeterministicTest,
    "Gloamstead.Night.SelectionDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightSelectionDeterministicTest::RunTest(const FString& /*Parameters*/)
{
    UNightConsequenceManager* Manager = NewObject<UNightConsequenceManager>();
    UNightConsequenceCatalog* Catalog = NewObject<UNightConsequenceCatalog>();
    PopulateMVPNightConsequenceRules(*Catalog);
    // Exercise the scoring path, not the first-night tutorial shortcut.
    Catalog->bForceTutorialOnFirstNight = false;
    Manager->bForceTutorialOnFirstNight = false;
    Manager->NightCatalog = Catalog;

    FNightSanctuarySnapshot Snapshot;
    Snapshot.AverageLightLevel = 0.5f;
    Snapshot.AverageCorruptionLevel = 0.3f;

    const ENightConsequenceType First = Manager->Test_SelectNightType(Snapshot);
    const ENightConsequenceType Second = Manager->Test_SelectNightType(Snapshot);
    TestTrue(TEXT("selection is deterministic for the same snapshot"), First == Second);
    TestTrue(TEXT("Tutorial (weight 10) wins the MVP scoring"), First == ENightConsequenceType::Tutorial);
    return true;
}

// First-night tutorial shortcut fires regardless of the snapshot when the force flag is set.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamNightForcedTutorialTest,
    "Gloamstead.Night.ForcedTutorialFirstNight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightForcedTutorialTest::RunTest(const FString& /*Parameters*/)
{
    UNightConsequenceManager* Manager = NewObject<UNightConsequenceManager>();
    UNightConsequenceCatalog* Catalog = NewObject<UNightConsequenceCatalog>();
    PopulateMVPNightConsequenceRules(*Catalog); // bForceTutorialOnFirstNight = true
    Manager->NightCatalog = Catalog;

    FNightSanctuarySnapshot Snapshot;
    Snapshot.AverageLightLevel = 0.1f;
    Snapshot.AverageCorruptionLevel = 0.9f; // would otherwise favor Corruption
    TestTrue(TEXT("first night is forced to Tutorial"), Manager->Test_SelectNightType(Snapshot) == ENightConsequenceType::Tutorial);
    return true;
}

// Empty catalog -> Corruption fallback (documented behavior).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamNightEmptyCatalogFallbackTest,
    "Gloamstead.Night.EmptyCatalogFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamNightEmptyCatalogFallbackTest::RunTest(const FString& /*Parameters*/)
{
    UNightConsequenceManager* Manager = NewObject<UNightConsequenceManager>();
    UNightConsequenceCatalog* Empty = NewObject<UNightConsequenceCatalog>(); // no rules
    Manager->NightCatalog = Empty;

    const FNightSanctuarySnapshot Snapshot;
    TestTrue(TEXT("empty catalog falls back to Corruption"), Manager->Test_SelectNightType(Snapshot) == ENightConsequenceType::Corruption);
    return true;
}

namespace
{
    UVeilHeartWarningCatalog* MakeWarningCatalog(const TArray<FName>& Tags)
    {
        UVeilHeartWarningCatalog* Catalog = NewObject<UVeilHeartWarningCatalog>();
        FVeilHeartWarningFragment Fragment;
        Fragment.WarningId = FName(TEXT("test_fragment"));
        Fragment.AssociatedNightType = ENightConsequenceType::Corruption;
        Fragment.SatisfiableTags = Tags;
        Catalog->Warnings.Add(Fragment);
        return Catalog;
    }
}

// A restoration whose explicit WarningTagSatisfied matches a catalog tag is recorded once.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamVeilHeartCatalogMatchTest,
    "Gloamstead.VeilHeart.CatalogTagMatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamVeilHeartCatalogMatchTest::RunTest(const FString& /*Parameters*/)
{
    AVeilHeart* Heart = NewObject<AVeilHeart>();
    Heart->WarningCatalog = MakeWarningCatalog({ FName(TEXT("ash_remembers_water")) });

    FRestorationEventPayload Payload;
    Payload.WarningTagSatisfied = FName(TEXT("ash_remembers_water"));
    Heart->EvaluateRestorationAgainstWarnings(Payload);
    TestEqual(TEXT("matching catalog tag satisfied once"), Heart->GetSatisfiedWarningTagCount(), 1);

    // Idempotent: the same tag does not double-count.
    Heart->EvaluateRestorationAgainstWarnings(Payload);
    TestEqual(TEXT("repeat does not double count"), Heart->GetSatisfiedWarningTagCount(), 1);
    return true;
}

// When WarningTagSatisfied is empty, the ritual display name is used as the fallback tag.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamVeilHeartRitualFallbackTest,
    "Gloamstead.VeilHeart.RitualNameFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamVeilHeartRitualFallbackTest::RunTest(const FString& /*Parameters*/)
{
    AVeilHeart* Heart = NewObject<AVeilHeart>();
    // GetRitualTypeDisplayName(LanternPost) == "Lantern Post".
    Heart->WarningCatalog = MakeWarningCatalog({ FName(TEXT("Lantern Post")) });

    FRestorationEventPayload Payload;
    Payload.WarningTagSatisfied = NAME_None;
    Payload.RitualType = ERitualType::LanternPost;
    Heart->EvaluateRestorationAgainstWarnings(Payload);
    TestEqual(TEXT("ritual-name fallback satisfies the matching warning"), Heart->GetSatisfiedWarningTagCount(), 1);
    return true;
}

// A tag with no matching catalog warning is not recorded.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamVeilHeartNoMatchTest,
    "Gloamstead.VeilHeart.NoMatchIgnored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamVeilHeartNoMatchTest::RunTest(const FString& /*Parameters*/)
{
    AVeilHeart* Heart = NewObject<AVeilHeart>();
    Heart->WarningCatalog = MakeWarningCatalog({ FName(TEXT("ash_remembers_water")) });

    FRestorationEventPayload Payload;
    Payload.WarningTagSatisfied = FName(TEXT("unrelated_tag"));
    Heart->EvaluateRestorationAgainstWarnings(Payload);
    TestEqual(TEXT("non-matching tag is ignored under a catalog"), Heart->GetSatisfiedWarningTagCount(), 0);
    return true;
}

// With no catalog assigned, any non-None tag is recorded directly (degraded-but-defined behavior).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamVeilHeartNoCatalogTest,
    "Gloamstead.VeilHeart.NoCatalogDirectRecord",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamVeilHeartNoCatalogTest::RunTest(const FString& /*Parameters*/)
{
    AVeilHeart* Heart = NewObject<AVeilHeart>();
    Heart->WarningCatalog = nullptr;

    FRestorationEventPayload Payload;
    Payload.WarningTagSatisfied = FName(TEXT("any_tag"));
    Heart->EvaluateRestorationAgainstWarnings(Payload);
    TestEqual(TEXT("tag recorded directly when no catalog"), Heart->GetSatisfiedWarningTagCount(), 1);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
