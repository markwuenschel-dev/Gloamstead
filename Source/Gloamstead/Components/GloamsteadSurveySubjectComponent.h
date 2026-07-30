#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GloamsteadSurveySubjectComponent.generated.h"

/**
 * Explicit survey-subject identity (Wave G2b).
 *
 * An actor carrying this component NAMES ITSELF as a survey subject. That is the whole mechanism —
 * the registry never searches for "the lantern", never matches on actor labels, and never picks the
 * nearest anything. Either a live actor declared the id, or the id does not resolve.
 *
 * This exists so an AUTHORED MAP can introduce a place-name (e.g. courtyard.lantern.first) without a
 * code change, while keeping the same fail-closed contract as the hard-coded declared-class table:
 * the map states the intent explicitly, so resolution is still reading a declaration, not guessing.
 *
 * Lifetime: registration happens on BeginPlay and is withdrawn on EndPlay and on component
 * destruction. The registry holds only WEAK references, so an actor that is destroyed without those
 * hooks firing cannot be resolved to a stale pointer — it resolves to a stale-registration failure
 * (GSS008) instead, which is a reported outcome rather than a dangling read.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GLOAMSTEAD_API UGloamsteadSurveySubjectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGloamsteadSurveySubjectComponent();

	/**
	 * The stable dotted place-name this actor claims, e.g. "courtyard.lantern.first".
	 * Empty/None is refused at registration (GSS002 + GSS009) — an anonymous claim is not a claim.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survey")
	FName SubjectId;

	/** Register automatically on BeginPlay. Turn off to control timing from Blueprint/C++. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survey")
	bool bRegisterOnBeginPlay = true;

	/**
	 * Claim SubjectId with this world's survey-subject registry.
	 *
	 * Idempotent for this component: re-registering the same component under the same id succeeds and
	 * changes nothing. A DIFFERENT live component claiming an id that is already claimed is refused
	 * with GSS007 — two actors claiming one place-name is ambiguity, and ambiguity never resolves.
	 *
	 * @return true when this component owns the id afterwards. Codes land in GetLastFailureCodes().
	 */
	UFUNCTION(BlueprintCallable, Category = "Survey")
	bool RegisterWithRegistry();

	/** Withdraw this component's claim. Safe to call when not registered. */
	UFUNCTION(BlueprintCallable, Category = "Survey")
	void UnregisterFromRegistry();

	/** True when this component currently holds the registration for its SubjectId. */
	UFUNCTION(BlueprintPure, Category = "Survey")
	bool IsRegistered() const;

	/** GSS codes from the most recent registration attempt. Empty after a clean registration. */
	UFUNCTION(BlueprintPure, Category = "Survey")
	TArray<FString> GetLastFailureCodes() const { return LastFailureCodes; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	/** Diagnostics only — the registry is the single source of truth for who owns an id. */
	UPROPERTY(Transient)
	TArray<FString> LastFailureCodes;
};
