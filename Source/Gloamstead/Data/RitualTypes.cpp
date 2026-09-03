#include "Data/RitualTypes.h"

FString GetRitualTypeDisplayName(ERitualType Type)
{
    switch (Type)
    {
        case ERitualType::LanternPost:   return TEXT("Lantern Post");
        case ERitualType::GardenBed:     return TEXT("Garden Bed");
        case ERitualType::PathPoint:     return TEXT("Path Point");
        case ERitualType::MirrorPillar:  return TEXT("Mirror Pillar");
        case ERitualType::BellShrine:    return TEXT("Bell Shrine");
        case ERitualType::AnchorStone:   return TEXT("Anchor Stone");
        default:                         return TEXT("Invalid");
    }
}

bool IsDirectlyRestorable(ERitualType Type)
{
    // Every authored ritual form the six-cycle arc asks for is placed by the player at a point the
    // active plan already named. PathPoint was excluded while Phase 1 had no cycle that asked for a
    // road; Cycle III is exactly that cycle, so the exclusion would now block its own objective.
    return Type != ERitualType::Invalid;
}

float GetDefaultLightContribution(ERitualType Type)
{
    // Fallbacks only. A shipped DA_Ritual_* definition overrides these; they exist so a missing
    // definition degrades to a sane value instead of a silent zero-light restoration.
    switch (Type)
    {
        case ERitualType::LanternPost:   return 0.35f;
        case ERitualType::GardenBed:     return 0.15f;
        // A road carries light rather than making it: enough to link two lanterns, not enough to
        // replace one.
        case ERitualType::PathPoint:     return 0.20f;
        case ERitualType::MirrorPillar:  return 0.30f;
        case ERitualType::BellShrine:    return 0.20f;
        // An anchor binds light that already exists; its own contribution is deliberately small.
        case ERitualType::AnchorStone:   return 0.10f;
        default:                         return 0.0f;
    }
}

float GetDefaultCorruptionClearance(ERitualType Type)
{
    switch (Type)
    {
        case ERitualType::LanternPost:   return 0.20f;
        case ERitualType::GardenBed:     return 0.35f;
        case ERitualType::PathPoint:     return 0.15f;
        case ERitualType::MirrorPillar:  return 0.10f;
        case ERitualType::BellShrine:    return 0.25f;
        case ERitualType::AnchorStone:   return 0.15f;
        default:                         return 0.0f;
    }
}
