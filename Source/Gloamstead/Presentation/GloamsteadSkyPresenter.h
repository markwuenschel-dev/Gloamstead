#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/VeilHeartWarningTypes.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "GloamsteadSkyPresenter.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class APostProcessVolume;
class AVeilHeart;

/** One phase's worth of sky. Pure presentation values; nothing here affects game state. */
USTRUCT(BlueprintType)
struct FGloamSkyPreset
{
	GENERATED_BODY()

	/** Sun elevation. Negative pitch points the light downward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	float SunPitch = -34.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	float SunYaw = 218.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	float SunIntensity = 13.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	FLinearColor SunColor = FLinearColor(1.f, 0.886f, 0.769f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	float SkyLightIntensity = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	FLinearColor SkyLightColor = FLinearColor(0.588f, 0.698f, 0.745f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float FogDensity = 0.028f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	FLinearColor FogInscattering = FLinearColor(0.10f, 0.13f, 0.14f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float ExposureBias = 1.7f;
};

/**
 * Gives the day/night phase a visible sky.
 *
 * The phase authority (UGloamsteadDayNightSubsystem) decides WHEN the world is in Day,
 * Dusk, Night or Dawn. This actor only decides what that LOOKS like, by subscribing to
 * OnPhaseChanged and blending the level's sun, sky light, fog and exposure toward the
 * matching preset. It never advances, requests or writes a phase, so the loop's authority
 * is unchanged: delete this actor and the game still plays identically, just unlit.
 *
 * Targets are found once on BeginPlay rather than wired by hand so the actor can simply be
 * dropped into a level.
 */
UCLASS()
class GLOAMSTEAD_API AGloamsteadSkyPresenter : public AActor
{
	GENERATED_BODY()

public:
	AGloamsteadSkyPresenter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gloamstead|Sky")
	FGloamSkyPreset DayPreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gloamstead|Sky")
	FGloamSkyPreset DuskPreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gloamstead|Sky")
	FGloamSkyPreset NightPreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gloamstead|Sky")
	FGloamSkyPreset DawnPreset;

	/** Seconds to cross-fade between presets. Dusk should be seen falling, not snap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gloamstead|Sky")
	float BlendSeconds = 4.f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Public so the automation tests can drive a phase without a live subsystem. */
	UFUNCTION()
	void HandlePhaseChanged(EGloamsteadDayPhase OldPhase, EGloamsteadDayPhase NewPhase);

	/**
	 * The generic post-tutorial warning surface. This actor takes the registered
	 * presenter role only after Cycle I's director has detached, so the exact
	 * Task 3 warning gate remains player-facing without reviving tutorial copy.
	 */
	UFUNCTION()
	void HandleHeartWarning(const FVeilHeartWarningFragment& WarningFragment);

	UFUNCTION(BlueprintImplementableEvent, Category = "Gloamstead|Sky")
	void OnHeartWarning(const FText& WarningText);

	const FGloamSkyPreset& PresetFor(EGloamsteadDayPhase Phase) const;

	/** Test seam: how far the blend has run, 0..1. */
	float Test_GetBlendAlpha() const { return BlendAlpha; }
	FName Test_GetLastPresentedWarningId() const { return LastPresentedWarningId; }
	/** The warning this presenter accepted for captioning (empty until one is). */
	FName Test_GetLastCaptionedWarningId() const { return LastCaptionedWarningId; }
	/** How many distinct warnings this presenter has accepted for captioning. The armed warning is
	 *  deliberately re-broadcast when a presenter registers, so this must not tick twice for one id. */
	int32 Test_GetCaptionAcceptedCount() const { return CaptionAcceptedCount; }
	/** Ordered phase events received by this sole global presentation writer. */
	const TArray<EGloamsteadDayPhase>& Test_GetPresentedPhaseHistory() const { return Test_PresentedPhaseHistory; }

private:
	void CacheTargets();
	void TryBindPostTutorialWarningPresenter();
	void UnbindPostTutorialWarningPresenter();
	void ApplyPreset(const FGloamSkyPreset& Preset);

	/**
	 * True when a Blueprint child actually implements EventName. A BlueprintImplementableEvent stub is
	 * owned by this native class; a Blueprint that implements it owns its own UFunction on the generated
	 * class, so a different outer means real Blueprint code should keep presentation authority.
	 */
	bool IsPresentationEventImplemented(FName EventName) const;

	/**
	 * Cycles II-VI had no warning surface at all: OnHeartWarning is a BlueprintImplementableEvent and no
	 * Blueprint subclass of this actor exists, so the Heart's words went nowhere. This is the same native
	 * caption fallback the first-night director already carries, plus the dedup the director never needed:
	 * DayNight deliberately re-broadcasts the armed warning when a presenter registers, so the identical
	 * fragment arrives twice and must caption once.
	 */
	void PresentWarningCaption(const FVeilHeartWarningFragment& WarningFragment);

	UPROPERTY(Transient)
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY(Transient)
	TObjectPtr<ASkyLight> Sky;

	UPROPERTY(Transient)
	TObjectPtr<AExponentialHeightFog> Fog;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> Grade;

	UPROPERTY(Transient)
	TObjectPtr<UGloamsteadDayNightSubsystem> CachedDayNight;

	TWeakObjectPtr<AVeilHeart> CachedHeart;
	bool bPostTutorialWarningPresenterBound = false;
	FName LastPresentedWarningId = NAME_None;

	/** The warning actually captioned to the screen, so a re-broadcast of the same one stays silent. */
	FName LastCaptionedWarningId = NAME_None;

	/** Distinct warnings accepted for captioning; the dedup latch keeps a re-broadcast from counting. */
	int32 CaptionAcceptedCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<class UUserWidget> FallbackCaptionWidget;
	TArray<EGloamsteadDayPhase> Test_PresentedPhaseHistory;

	FGloamSkyPreset FromPreset;
	FGloamSkyPreset ToPreset;
	float BlendAlpha = 1.f;
};
