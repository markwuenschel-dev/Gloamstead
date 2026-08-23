#include "Systems/VeilHeart.h"
#include "Systems/GloamsteadDayNightSubsystem.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

AVeilHeart::AVeilHeart()
{
	PrimaryActorTick.bCanEverTick = false;

	// The Heart is the player's rest point (IGloamInteractable). The interaction system focuses its target via
	// an object-type overlap (UGloamInteractionComponent::UpdateFocus), so the Heart must carry a collision
	// volume to be findable at all — without one, rest / "greet the dawn" can never fire for a real player and
	// the Dawn->Day advance soft-locks. Query-only + overlap responses keep it detectable yet non-blocking,
	// exactly like the readability proxies.
	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->InitSphereRadius(150.0f);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldStatic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(false); // focus uses a world overlap query, not overlap events
}

namespace
{
	UGloamsteadDayNightSubsystem* GetDayNight(const AActor* Actor)
	{
		const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UGloamsteadDayNightSubsystem>() : nullptr;
	}
}

// ===== IGloamInteractable — the Heart as the player's rest point =====

bool AVeilHeart::CanInteract_Implementation(AActor* /*Interactor*/) const
{
	// Interactable only during the resting phases (Day/Dawn); inert once dusk gathers.
	const UGloamsteadDayNightSubsystem* DayNight = GetDayNight(this);
	return DayNight && DayNight->CanRestNow();
}

FText AVeilHeart::GetInteractionPrompt_Implementation() const
{
	const UGloamsteadDayNightSubsystem* DayNight = GetDayNight(this);
	if (DayNight && DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dawn)
	{
		return NSLOCTEXT("Gloamstead", "HeartWake", "Greet the dawn");
	}
	return NSLOCTEXT("Gloamstead", "HeartRest", "Rest at the Heart");
}

void AVeilHeart::Interact_Implementation(AActor* /*Interactor*/)
{
	UGloamsteadDayNightSubsystem* DayNight = GetDayNight(this);
	if (!DayNight)
	{
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: rest requested but no DayNight subsystem."));
		return;
	}
	if (DayNight->RequestRest())
	{
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: the player rests at the Heart; the cycle turns."));
	}
}

void AVeilHeart::Examine_Implementation(AActor* /*Interactor*/)
{
	// The Heart speaks its memory of the last night (the outcome dawn recorded).
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: examined - last night '%s' resolved %s (tag %s)."),
		*GetNightConsequenceTypeDisplayName(LastNightOutcome.NightType),
		*GetNightOutcomeResultDisplayName(LastNightOutcome.Result),
		*LastNightOutcome.ResultTag.ToString());
}

void AVeilHeart::BeginPlay()
{
	Super::BeginPlay();

	EnsureWarningCatalog();

	if (UWorld* World = GetWorld())
	{
		if (UGloamsteadPCGSubsystem* PCGSub = World->GetSubsystem<UGloamsteadPCGSubsystem>())
		{
			PCGSub->OnStructureRestored.AddDynamic(this, &AVeilHeart::OnRestorationComplete);
		}
	}
}

bool AVeilHeart::EnsureWarningCatalog()
{
	if (WarningCatalog)
	{
		return true;
	}

	WarningCatalog = Cast<UVeilHeartWarningCatalog>(
		StaticLoadObject(UVeilHeartWarningCatalog::StaticClass(), nullptr,
			TEXT("/Game/Data/DA_VeilHeartWarningCatalog.DA_VeilHeartWarningCatalog")));
	if (WarningCatalog)
	{
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Loaded warning catalog from /Game/Data/DA_VeilHeartWarningCatalog."));
	}
	return WarningCatalog != nullptr;
}

bool AVeilHeart::RegisterWarningPresenter(UObject* Presenter, FName WarningHandlerFunction)
{
	if (!IsValid(Presenter) || Presenter == this || WarningHandlerFunction == NAME_None
		|| !OnWarningEmittedDelegate.Contains(Presenter, WarningHandlerFunction))
	{
		return false;
	}

	if (RegisteredWarningPresenter.IsValid()
		&& (RegisteredWarningPresenter.Get() != Presenter || RegisteredWarningPresenterFunction != WarningHandlerFunction))
	{
		return false;
	}

	RegisteredWarningPresenter = Presenter;
	RegisteredWarningPresenterFunction = WarningHandlerFunction;
	return true;
}

void AVeilHeart::UnregisterWarningPresenter(UObject* Presenter)
{
	if (RegisteredWarningPresenter.Get() == Presenter)
	{
		RegisteredWarningPresenter.Reset();
		RegisteredWarningPresenterFunction = NAME_None;
	}
}

bool AVeilHeart::HasValidWarningPresenter() const
{
	UObject* Presenter = RegisteredWarningPresenter.Get();
	return IsValid(Presenter)
		&& RegisteredWarningPresenterFunction != NAME_None
		&& OnWarningEmittedDelegate.Contains(Presenter, RegisteredWarningPresenterFunction);
}

void AVeilHeart::OnRestorationComplete(const FRestorationEventPayload& Payload)
{
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: Restoration received - Ritual: %d, LightDelta: %.2f, CorruptionCleared: %.2f, WarningTag: %s"),
		static_cast<int32>(Payload.RitualType), Payload.LightDelta, Payload.CorruptionCleared,
		*Payload.WarningTagSatisfied.ToString());

	EvaluateRestorationAgainstWarnings(Payload);
}

void AVeilHeart::EvaluateRestorationAgainstWarnings(const FRestorationEventPayload& Payload)
{
	FName TagToCheck = Payload.WarningTagSatisfied;
	if (TagToCheck == NAME_None)
	{
		TagToCheck = FName(*GetRitualTypeDisplayName(Payload.RitualType));
	}

	if (TagToCheck == NAME_None)
	{
		return;
	}

	if (WarningCatalog)
	{
		for (const FVeilHeartWarningFragment& Warning : WarningCatalog->Warnings)
		{
			if (Warning.SatisfiableTags.Contains(TagToCheck))
			{
				if (!SatisfiedWarningTags.Contains(TagToCheck))
				{
					SatisfiedWarningTags.Add(TagToCheck);
					UE_LOG(LogTemp, Log, TEXT("VeilHeart: Warning tag satisfied via catalog: %s"), *TagToCheck.ToString());
				}
				return;
			}
		}
		UE_LOG(LogTemp, Verbose, TEXT("VeilHeart: Tag %s did not match any catalog warning."), *TagToCheck.ToString());
		return;
	}

	if (!SatisfiedWarningTags.Contains(TagToCheck))
	{
		SatisfiedWarningTags.Add(TagToCheck);
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Warning tag satisfied (no catalog): %s"), *TagToCheck.ToString());
	}
}

const FVeilHeartWarningFragment* AVeilHeart::FindWarningForNight(ENightConsequenceType NightType) const
{
	if (!WarningCatalog)
	{
		return nullptr;
	}

	const FVeilHeartWarningFragment* Best = nullptr;
	for (const FVeilHeartWarningFragment& Warning : WarningCatalog->Warnings)
	{
		if (Warning.AssociatedNightType == NightType)
		{
			if (!Best || Warning.ClarityTier >= Best->ClarityTier)
			{
				Best = &Warning;
			}
		}
	}
	return Best;
}

const FVeilHeartWarningFragment* AVeilHeart::FindExactWarningById(FName WarningId, ENightConsequenceType ExpectedNightType) const
{
	if (!WarningCatalog || WarningId == NAME_None || ExpectedNightType == ENightConsequenceType::Invalid)
	{
		return nullptr;
	}

	const FVeilHeartWarningFragment* ExactWarning = nullptr;
	for (const FVeilHeartWarningFragment& Candidate : WarningCatalog->Warnings)
	{
		if (Candidate.WarningId != WarningId)
		{
			continue;
		}

		if (ExactWarning || Candidate.AssociatedNightType != ExpectedNightType)
		{
			return nullptr;
		}
		ExactWarning = &Candidate;
	}

	return ExactWarning;
}

bool AVeilHeart::HasExactWarningById(FName WarningId, ENightConsequenceType ExpectedNightType)
{
	return EnsureWarningCatalog() && FindExactWarningById(WarningId, ExpectedNightType) != nullptr;
}

void AVeilHeart::EmitWarningForNight(ENightConsequenceType NightType)
{
	if (const FVeilHeartWarningFragment* Warning = FindWarningForNight(NightType))
	{
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dusk warning [%s] for night %s"),
			*Warning->WarningId.ToString(), *GetNightConsequenceTypeDisplayName(NightType));
		OnWarningEmitted(*Warning);
		OnWarningEmittedDelegate.Broadcast(*Warning);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: No catalog warning for night %s (catalog %s)."),
			*GetNightConsequenceTypeDisplayName(NightType),
			WarningCatalog ? TEXT("assigned") : TEXT("missing"));
	}
}

bool AVeilHeart::EmitWarningById(FName WarningId, ENightConsequenceType ExpectedNightType)
{
	if (!HasValidWarningPresenter() || !HasExactWarningById(WarningId, ExpectedNightType))
	{
		return false;
	}

	const FVeilHeartWarningFragment* ExactWarning = FindExactWarningById(WarningId, ExpectedNightType);
	if (!ExactWarning)
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("VeilHeart: Authored Day warning [%s] for night %s."),
		*ExactWarning->WarningId.ToString(), *GetNightConsequenceTypeDisplayName(ExpectedNightType));
	LastEmittedWarningId = ExactWarning->WarningId;
	OnWarningEmitted(*ExactWarning);
	OnWarningEmittedDelegate.Broadcast(*ExactWarning);
	return true;
}

void AVeilHeart::ProcessDawnReflection()
{
	// BP-compat entry point: reflect with no night outcome data.
	ProcessDawnReflectionWithOutcome(FNightRuntimeOutcome());
}

void AVeilHeart::ProcessDawnReflectionWithOutcome(const FNightRuntimeOutcome& Outcome)
{
	const int32 TagsThisCycle = SatisfiedWarningTags.Num();
	LastNightOutcome = Outcome;

	// Distinguish how the night resolved so payoff (and the next cycle) can react meaningfully.
	switch (Outcome.Result)
	{
	case ENightOutcomeResult::Success:
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dawn Reflection - the sanctuary held (%s). %d warning tag(s) heeded; night '%s' resolved."),
			*Outcome.ResultTag.ToString(), TagsThisCycle, *GetNightConsequenceTypeDisplayName(Outcome.NightType));
		break;
	case ENightOutcomeResult::Partial:
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dawn Reflection - the dark receded but lingers (%s). Bloom reduced by %.2f; %d tag(s) heeded."),
			*Outcome.ResultTag.ToString(), -Outcome.TargetCorruptionDelta, TagsThisCycle);
		break;
	case ENightOutcomeResult::Failure:
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: Dawn Reflection - a scar remains (%s). Bloom worsened by %.2f; the sanctuary carries this into the next night."),
			*Outcome.ResultTag.ToString(), Outcome.TargetCorruptionDelta);
		break;
	case ENightOutcomeResult::None:
	default:
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: Dawn Reflection - %d warning tag(s) satisfied this cycle (no night outcome)."),
			TagsThisCycle);
		break;
	}

	OnDawnReflection(Outcome);
	OnDawnReflectionDelegate.Broadcast(Outcome);

	SatisfiedWarningTags.Empty();
}
