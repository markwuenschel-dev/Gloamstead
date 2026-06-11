#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "GloamsteadBlueprintLibrary.generated.h"

/**
 * Blueprint helpers for PIE smoke tests (e.g. Level Blueprint test keys).
 * Use these when "Get World Subsystem" is awkward to find in the node menu.
 */
UCLASS()
class GLOAMSTEAD_API UGloamsteadBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gloamstead|DayNight", meta = (WorldContext = "WorldContextObject"))
	static void AdvanceGloamsteadDayPhase(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Gloamstead|DayNight", meta = (WorldContext = "WorldContextObject"))
	static EGloamsteadDayPhase GetGloamsteadDayPhase(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Gloamstead|DayNight", meta = (WorldContext = "WorldContextObject"))
	static int32 GetGloamsteadNightCount(const UObject* WorldContextObject);
};
