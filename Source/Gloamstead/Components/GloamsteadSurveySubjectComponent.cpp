#include "Components/GloamsteadSurveySubjectComponent.h"
#include "Systems/GloamsteadSurveySubjectRegistry.h"
#include "Gloamstead.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UGloamsteadSurveySubjectComponent::UGloamsteadSurveySubjectComponent()
{
	// Pure identity declaration: it never ticks and never touches gameplay state.
	PrimaryComponentTick.bCanEverTick = false;
}

// Distinctively named and `static` rather than sitting in an anonymous namespace: an anonymous
// namespace does NOT keep helper names apart once files land in the same unity translation unit
// (see the same note in GloamsteadSurveySubjectTypes.cpp, which was written after a C2264).
static UGloamsteadSurveySubjectRegistry* GSSRegistryForComponent(const UActorComponent* Component)
{
	const UWorld* World = Component ? Component->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UGloamsteadSurveySubjectRegistry>() : nullptr;
}

void UGloamsteadSurveySubjectComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bRegisterOnBeginPlay)
	{
		RegisterWithRegistry();
	}
}

void UGloamsteadSurveySubjectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromRegistry();
	Super::EndPlay(EndPlayReason);
}

void UGloamsteadSurveySubjectComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// Belt and braces: a component removed without ever running EndPlay must still drop its claim.
	// Unregistration is idempotent, so the EndPlay path doing it first is harmless.
	UnregisterFromRegistry();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UGloamsteadSurveySubjectComponent::RegisterWithRegistry()
{
	LastFailureCodes.Reset();

	UGloamsteadSurveySubjectRegistry* Registry = GSSRegistryForComponent(this);
	if (!Registry)
	{
		LastFailureCodes.AddUnique(TEXT("GSS009"));
		UE_LOG(LogGloamstead, Warning,
			TEXT("[GSS009] Survey subject '%s' on '%s' could not register: no survey-subject registry for this world."),
			*SubjectId.ToString(), *GetNameSafe(GetOwner()));
		return false;
	}

	const bool bOk = Registry->RegisterSubjectComponent(this, LastFailureCodes);
	if (!bOk)
	{
		UE_LOG(LogGloamstead, Warning,
			TEXT("[GSS] Survey subject '%s' on '%s' was NOT registered: %s"),
			*SubjectId.ToString(), *GetNameSafe(GetOwner()),
			*FString::Join(LastFailureCodes, TEXT(",")));
	}
	return bOk;
}

void UGloamsteadSurveySubjectComponent::UnregisterFromRegistry()
{
	if (UGloamsteadSurveySubjectRegistry* Registry = GSSRegistryForComponent(this))
	{
		Registry->UnregisterSubjectComponent(this);
	}
}

bool UGloamsteadSurveySubjectComponent::IsRegistered() const
{
	const UGloamsteadSurveySubjectRegistry* Registry = GSSRegistryForComponent(this);
	return Registry && Registry->GetRegisteredComponent(SubjectId) == this;
}
