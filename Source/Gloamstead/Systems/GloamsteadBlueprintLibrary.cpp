#include "Systems/GloamsteadBlueprintLibrary.h"

#include "Engine/World.h"
#include "Systems/GloamsteadDayNightSubsystem.h"

static UGloamsteadDayNightSubsystem* GetDayNightSubsystem(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<UGloamsteadDayNightSubsystem>() : nullptr;
}

void UGloamsteadBlueprintLibrary::AdvanceGloamsteadDayPhase(const UObject* WorldContextObject)
{
	if (UGloamsteadDayNightSubsystem* DayNight = GetDayNightSubsystem(WorldContextObject))
	{
		DayNight->AdvanceToNextPhase();
	}
}

EGloamsteadDayPhase UGloamsteadBlueprintLibrary::GetGloamsteadDayPhase(const UObject* WorldContextObject)
{
	if (const UGloamsteadDayNightSubsystem* DayNight = GetDayNightSubsystem(WorldContextObject))
	{
		return DayNight->GetCurrentPhase();
	}
	return EGloamsteadDayPhase::Day;
}

int32 UGloamsteadBlueprintLibrary::GetGloamsteadNightCount(const UObject* WorldContextObject)
{
	if (const UGloamsteadDayNightSubsystem* DayNight = GetDayNightSubsystem(WorldContextObject))
	{
		return DayNight->GetNightCount();
	}
	return 0;
}
