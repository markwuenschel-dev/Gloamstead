#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Data/GloamsteadGeneratedAssetCatalog.h"
#include "GloamsteadGeneratedAssetSettings.generated.h"

/** Provider selection is explicit: generated production assets or a development-only primitive fallback. */
UENUM(BlueprintType)
enum class EGloamsteadMeshForgeProviderMode : uint8
{
	GeneratedCatalog = 0,
	EnginePrimitiveDevelopmentFallback,
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Gloamstead Generated Assets"))
class GLOAMSTEAD_API UGloamsteadGeneratedAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	/** Safety default before config is applied: production/generated mode, never implicit primitives. */
	static EGloamsteadMeshForgeProviderMode ProviderModeWhenConfigAbsent()
	{
		return EGloamsteadMeshForgeProviderMode::GeneratedCatalog;
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Provider")
	EGloamsteadMeshForgeProviderMode ProviderMode = ProviderModeWhenConfigAbsent();

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Provider",
		meta = (AllowedClasses = "/Script/Gloamstead.GloamsteadGeneratedAssetCatalog"))
	TSoftObjectPtr<UGloamsteadGeneratedAssetCatalog> Catalog;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Provider")
	FString ExpectedActiveBundleId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Provider")
	FString ExpectedReceiptSha256;

	/** Expected canonical target UE build + Gloamstead base commit + vendored plugin lock hash. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Provider")
	FString ExpectedTargetBuildIdentitySha256;
};
