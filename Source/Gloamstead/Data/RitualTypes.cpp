#include "Data/RitualTypes.h"

FString GetRitualTypeDisplayName(ERitualType Type)
{
    switch (Type)
    {
        case ERitualType::LanternPost:   return TEXT("Lantern Post");
        case ERitualType::GardenBed:     return TEXT("Garden Bed");
        case ERitualType::PathPoint:     return TEXT("Path Point");
        default:                         return TEXT("Invalid");
    }
}

bool IsDirectlyRestorable(ERitualType Type)
{
    // PathPoints are not directly restorable in Phase 1
    return Type == ERitualType::LanternPost || Type == ERitualType::GardenBed;
}

float GetDefaultLightContribution(ERitualType Type)
{
    switch (Type)
    {
        case ERitualType::LanternPost:   return 0.35f;
        case ERitualType::GardenBed:     return 0.15f;
        default:                         return 0.0f;
    }
}

float GetDefaultCorruptionClearance(ERitualType Type)
{
    switch (Type)
    {
        case ERitualType::LanternPost:   return 0.20f;
        case ERitualType::GardenBed:     return 0.35f;
        default:                         return 0.0f;
    }
}