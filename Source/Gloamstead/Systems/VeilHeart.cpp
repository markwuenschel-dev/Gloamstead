#include "Systems/VeilHeart.h"
#include "Actors/GloamsteadEvidenceSource.h"
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

		// A progression load can complete before this actor exists. DayNight keeps
		// its validated v3 interpretation facts pending until the one real Heart
		// has both a world and a catalog, then restores them here without replaying
		// a warning or fabricating evidence.
		if (UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>())
		{
			DayNight->NotifyHeartReadyForProgressionRestore(this);
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

const FExperienceCyclePlan* AVeilHeart::ResolveActivePlan() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bHasTestActivePlan)
	{
		return &TestActivePlan;
	}
#endif

	const UGloamsteadDayNightSubsystem* DayNight = GetDayNight(this);
	return DayNight ? DayNight->GetUpcomingPlan() : nullptr;
}

UGloamsteadPCGSubsystem* AVeilHeart::ResolvePCGSubsystem() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UGloamsteadPCGSubsystem>();
	}
	return nullptr;
}

bool AVeilHeart::IsExactWarningPresentedForPlan(const FExperienceCyclePlan& Plan) const
{
	return Plan.IsAuthoredPlan()
		&& Plan.WarningId != NAME_None
		&& LastEmittedWarningId == Plan.WarningId
		&& FindExactWarningById(Plan.WarningId, Plan.NightType) != nullptr;
}

bool AVeilHeart::HasRequiredSupportEvidence(const FExperienceCyclePlan& Plan) const
{
	if (Plan.RequiredSupportIds.IsEmpty()
		|| Plan.MinimumDistinctSupportCount < 2
		|| EncounteredSupportIds.Num() < Plan.MinimumDistinctSupportCount)
	{
		return false;
	}

	for (const FName EncounteredId : EncounteredSupportIds)
	{
		if (EncounteredId == NAME_None || !Plan.RequiredSupportIds.Contains(EncounteredId))
		{
			return false;
		}
	}
	return true;
}

void AVeilHeart::OnRestorationComplete(const FRestorationEventPayload& Payload)
{
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: Restoration received - Ritual: %d, LightDelta: %.2f, CorruptionCleared: %.2f, Warning: %s, WarningTag: %s, Subject: %s"),
		static_cast<int32>(Payload.RitualType), Payload.LightDelta, Payload.CorruptionCleared,
		*Payload.WarningId.ToString(), *Payload.WarningTagSatisfied.ToString(), *Payload.SemanticSubject.ToString());

	EvaluateRestorationAgainstWarnings(Payload);
	EvaluateRestorationAgainstActivePlan(Payload);
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

bool AVeilHeart::RecordSupportEncounterFromEvidenceSource(const AGloamsteadEvidenceSource* Source)
{
	if (!IsValid(Source) || Source->GetWorld() == nullptr || Source->GetWorld() != GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: rejected evidence from a foreign or non-world source."));
		return false;
	}

	return RecordSupportEncounterInternal(Source->GetWarningId(), Source->GetSupportId(), Source->GetChannelType());
}

bool AVeilHeart::RecordSupportEncounterInternal(FName WarningId, FName SupportId, FName ChannelType)
{
	if (!EnsureWarningCatalog())
	{
		return false;
	}

	const FExperienceCyclePlan* ActivePlan = ResolveActivePlan();
	if (!ActivePlan || !IsExactWarningPresentedForPlan(*ActivePlan) || WarningId != ActivePlan->WarningId)
	{
		UE_LOG(LogTemp, Verbose, TEXT("VeilHeart: support %s rejected because it does not name the currently presented authored warning."),
			*SupportId.ToString());
		return false;
	}

	const FVeilHeartWarningFragment* ExactWarning = FindExactWarningById(ActivePlan->WarningId, ActivePlan->NightType);
	FString ContractError;
	if (!ExactWarning || !ExactWarning->MatchesExactPlanContract(*ActivePlan, &ContractError))
	{
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: support rejected because the warning contract is invalid: %s."), *ContractError);
		return false;
	}

	const bool bKnownSupport = ExactWarning->SupportChannels.ContainsByPredicate(
		[SupportId, ChannelType](const FVeilHeartWarningSupportChannel& Channel)
		{
			return Channel.SupportId == SupportId && Channel.ChannelType == ChannelType;
		});
	if (SupportId == NAME_None || ChannelType == NAME_None || !bKnownSupport || EncounteredSupportIds.Contains(SupportId))
	{
		UE_LOG(LogTemp, Verbose, TEXT("VeilHeart: duplicate, unknown, or wrong-medium support %s was not counted."), *SupportId.ToString());
		return false;
	}

	EncounteredSupportIds.Add(SupportId);
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: recorded support %s for exact warning %s (%d/%d distinct)."),
		*SupportId.ToString(), *ActivePlan->WarningId.ToString(), EncounteredSupportIds.Num(), ActivePlan->MinimumDistinctSupportCount);
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
bool AVeilHeart::Test_RecordSupportEncounter(FName WarningId, FName SupportId, FName ChannelType)
{
	return RecordSupportEncounterInternal(WarningId, SupportId, ChannelType);
}
#endif

bool AVeilHeart::EvaluateRestorationAgainstActivePlan(const FRestorationEventPayload& Payload)
{
	if (!EnsureWarningCatalog())
	{
		return false;
	}

	const FExperienceCyclePlan* ActivePlan = ResolveActivePlan();
	if (!ActivePlan || !IsExactWarningPresentedForPlan(*ActivePlan) || !HasRequiredSupportEvidence(*ActivePlan))
	{
		return false;
	}

	const FVeilHeartWarningFragment* ExactWarning = FindExactWarningById(ActivePlan->WarningId, ActivePlan->NightType);
	FString ContractError;
	if (!ExactWarning || !ExactWarning->MatchesExactPlanContract(*ActivePlan, &ContractError))
	{
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: restoration rejected because the warning contract is invalid: %s."), *ContractError);
		return false;
	}

	UGloamsteadPCGSubsystem* PCG = ResolvePCGSubsystem();
	if (!PCG
		|| Payload.PointIndex == INDEX_NONE
		|| !PCG->IsPointRestored(Payload.PointIndex)
		|| !PCG->PointMatchesExperiencePlan(Payload.PointIndex, *ActivePlan, /*bRequireRestored*/ true))
	{
		// Never treat caller-populated WarningId, SemanticSubject, RitualType, or
		// WarningTagSatisfied as authority. Receipt evidence is minted only after
		// PCG has marked this point restored and its own metadata matches the full
		// active plan contract.
		UE_LOG(LogTemp, Verbose, TEXT("VeilHeart: restoration did not come from a restored PCG point matching warning %s, subject %s, ritual, and tag."),
			*ActivePlan->WarningId.ToString(), *ActivePlan->SemanticSubject.ToString());
		return false;
	}

	if (HasExactInterpretationForPlan(*ActivePlan))
	{
		return false;
	}

	LastInterpretationReceipt = FExperienceInterpretationReceipt();
	LastInterpretationReceipt.ReceiptId = ActivePlan->InterpretationReceiptId;
	LastInterpretationReceipt.PlanId = ActivePlan->PlanId;
	LastInterpretationReceipt.WarningId = ActivePlan->WarningId;
	LastInterpretationReceipt.SemanticSubject = ActivePlan->SemanticSubject;
	LastInterpretationReceipt.RestorationTag = ActivePlan->RequiredRestorationTags[0];
	LastInterpretationReceipt.RestorationRitualType = ActivePlan->RequiredRitualType;
	LastInterpretationReceipt.RestorationPointIndex = Payload.PointIndex;
	LastInterpretationReceipt.SupportIds.Reset(EncounteredSupportIds.Num());
	for (const FName SupportId : EncounteredSupportIds)
	{
		LastInterpretationReceipt.SupportIds.Add(SupportId);
	}

	UE_LOG(LogTemp, Log, TEXT("VeilHeart: exact interpretation receipt %s earned for %s at subject %s."),
		*LastInterpretationReceipt.ReceiptId.ToString(), *LastInterpretationReceipt.WarningId.ToString(),
		*LastInterpretationReceipt.SemanticSubject.ToString());
	return true;
}

FVeilHeartInterpretationPersistentState AVeilHeart::CaptureInterpretationPersistentState() const
{
	FVeilHeartInterpretationPersistentState State;
	State.PresentedWarningId = LastEmittedWarningId;
	State.EncounteredSupportIds = EncounteredSupportIds.Array();
	State.InterpretationReceipt = LastInterpretationReceipt;
	return State;
}

bool AVeilHeart::IsInterpretationCatalogReady()
{
	return EnsureWarningCatalog();
}

void AVeilHeart::ResetInterpretationPersistentState()
{
	EncounteredSupportIds.Empty();
	LastInterpretationReceipt = FExperienceInterpretationReceipt();
	LastEmittedWarningId = NAME_None;
}

bool AVeilHeart::RestoreInterpretationPersistentState(const FVeilHeartInterpretationPersistentState& State)
{
	ResetInterpretationPersistentState();
	if (!State.HasAnyFacts())
	{
		return true;
	}

	if (!EnsureWarningCatalog())
	{
		return false;
	}

	const FExperienceCyclePlan* ActivePlan = ResolveActivePlan();
	if (!ActivePlan || !ActivePlan->IsAuthoredPlan()
		|| State.PresentedWarningId != ActivePlan->WarningId)
	{
		return false;
	}

	const FVeilHeartWarningFragment* ExactWarning = FindExactWarningById(ActivePlan->WarningId, ActivePlan->NightType);
	FString ContractError;
	if (!ExactWarning || !ExactWarning->MatchesExactPlanContract(*ActivePlan, &ContractError))
	{
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: refusing persisted interpretation because the active warning contract is invalid: %s."), *ContractError);
		return false;
	}

	TSet<FName> RestoredSupportIds;
	for (const FName SupportId : State.EncounteredSupportIds)
	{
		if (SupportId == NAME_None
			|| RestoredSupportIds.Contains(SupportId)
			|| !ActivePlan->RequiredSupportIds.Contains(SupportId))
		{
			return false;
		}
		RestoredSupportIds.Add(SupportId);
	}

	LastEmittedWarningId = State.PresentedWarningId;
	EncounteredSupportIds = MoveTemp(RestoredSupportIds);
	LastInterpretationReceipt = State.InterpretationReceipt;

	if (!State.InterpretationReceipt.IsValid())
	{
		return true;
	}

	if (!HasExactInterpretationForPlan(*ActivePlan))
	{
		ResetInterpretationPersistentState();
		return false;
	}

	return true;
}

bool AVeilHeart::HasExactInterpretationForPlan(const FExperienceCyclePlan& Plan) const
{
	const FVeilHeartWarningFragment* ExactWarning = FindExactWarningById(Plan.WarningId, Plan.NightType);
	UGloamsteadPCGSubsystem* PCG = ResolvePCGSubsystem();
	FString ContractError;
	if (!Plan.IsAuthoredPlan()
		|| !ExactWarning
		|| !ExactWarning->MatchesExactPlanContract(Plan, &ContractError)
		|| !PCG
		|| !LastInterpretationReceipt.IsValid()
		|| LastInterpretationReceipt.ReceiptId != Plan.InterpretationReceiptId
		|| LastInterpretationReceipt.PlanId != Plan.PlanId
		|| LastInterpretationReceipt.WarningId != Plan.WarningId
		|| LastInterpretationReceipt.SemanticSubject != Plan.SemanticSubject
		|| LastInterpretationReceipt.RestorationRitualType != Plan.RequiredRitualType
		|| LastInterpretationReceipt.RestorationPointIndex == INDEX_NONE
		|| !PCG->IsPointRestored(LastInterpretationReceipt.RestorationPointIndex)
		|| !PCG->PointMatchesExperiencePlan(LastInterpretationReceipt.RestorationPointIndex, Plan, /*bRequireRestored*/ true)
		|| !Plan.RequiredRestorationTags.Contains(LastInterpretationReceipt.RestorationTag)
		|| LastInterpretationReceipt.SupportIds.Num() < Plan.MinimumDistinctSupportCount)
	{
		return false;
	}

	TSet<FName> ReceiptSupportIds;
	for (const FName SupportId : LastInterpretationReceipt.SupportIds)
	{
		if (SupportId == NAME_None
			|| ReceiptSupportIds.Contains(SupportId)
			|| !Plan.RequiredSupportIds.Contains(SupportId))
		{
			return false;
		}
		ReceiptSupportIds.Add(SupportId);
	}

	return ReceiptSupportIds.Num() >= Plan.MinimumDistinctSupportCount;
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

bool AVeilHeart::CanPresentWarningForPlan(const FExperienceCyclePlan& Plan)
{
	if (!Plan.IsAuthoredPlan() || !HasValidWarningPresenter() || !EnsureWarningCatalog())
	{
		return false;
	}

	const FVeilHeartWarningFragment* ExactWarning = FindExactWarningById(Plan.WarningId, Plan.NightType);
	if (!ExactWarning)
	{
		return false;
	}

	// The opening tutorial intentionally predates fair-crypticism evidence.
	// GardenRot is different: its identity alone is not admission; every
	// canonical subject, ritual, tag, support ID, and distinct medium must agree
	// with the active plan before the Day authority can make rest available.
	if (Plan.WarningId == FName(TEXT("GardenRot")))
	{
		FString ContractError;
		if (!ExactWarning->MatchesExactPlanContract(Plan, &ContractError))
		{
			UE_LOG(LogTemp, Warning, TEXT("VeilHeart: GardenRot presentation refused because its warning contract is invalid: %s."), *ContractError);
			return false;
		}
	}

	return true;
}

bool AVeilHeart::EmitWarningForPlan(const FExperienceCyclePlan& Plan)
{
	if (!CanPresentWarningForPlan(Plan))
	{
		return false;
	}

	const FVeilHeartWarningFragment* ExactWarning = FindExactWarningById(Plan.WarningId, Plan.NightType);
	check(ExactWarning);
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: authored Day warning [%s] for night %s."),
		*ExactWarning->WarningId.ToString(), *GetNightConsequenceTypeDisplayName(Plan.NightType));
	LastEmittedWarningId = ExactWarning->WarningId;
	OnWarningEmitted(*ExactWarning);
	OnWarningEmittedDelegate.Broadcast(*ExactWarning);
	return true;
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
	if (const FExperienceCyclePlan* ActivePlan = ResolveActivePlan();
		ActivePlan && ActivePlan->WarningId == WarningId && ActivePlan->NightType == ExpectedNightType)
	{
		return EmitWarningForPlan(*ActivePlan);
	}

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
	ResetInterpretationPersistentState();
}
