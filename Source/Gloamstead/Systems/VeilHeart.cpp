#include "Systems/VeilHeart.h"
#include "Systems/GloamsteadExperienceCycleSubsystem.h"
#include "Engine/GameInstance.h"
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

	/** True once every authored cycle is finished and no further plan exists. */
	bool IsExperienceComplete(const AActor* Actor)
	{
		const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UGloamsteadExperienceCycleSubsystem* Experience =
			GameInstance ? GameInstance->GetSubsystem<UGloamsteadExperienceCycleSubsystem>() : nullptr;
		return Experience && Experience->IsExperienceComplete();
	}
}

// ===== IGloamInteractable — the Heart as the player's rest point =====

bool AVeilHeart::CanInteract_Implementation(AActor* /*Interactor*/) const
{
	// Interactable only during the resting phases (Day/Dawn); inert once dusk gathers.
	//
	// Once the authored experience is complete the Heart stays interactable on purpose. CanRestNow() is
	// false forever at that point, and returning it unconditionally left the player standing beside the
	// object the whole game is about with no prompt, no response and no explanation - a soft-lock dressed
	// as an ending. The Heart still answers; it simply has no further night to bring.
	if (IsExperienceComplete(this))
	{
		return true;
	}

	const UGloamsteadDayNightSubsystem* DayNight = GetDayNight(this);
	return DayNight && DayNight->CanRestNow();
}

FText AVeilHeart::GetInteractionPrompt_Implementation() const
{
	if (IsExperienceComplete(this))
	{
		return NSLOCTEXT("Gloamstead", "HeartComplete", "Sit with what the sanctuary remembers");
	}

	const UGloamsteadDayNightSubsystem* DayNight = GetDayNight(this);
	if (DayNight && DayNight->GetCurrentPhase() == EGloamsteadDayPhase::Dawn)
	{
		return NSLOCTEXT("Gloamstead", "HeartWake", "Greet the dawn");
	}
	return NSLOCTEXT("Gloamstead", "HeartRest", "Rest at the Heart");
}

void AVeilHeart::Interact_Implementation(AActor* /*Interactor*/)
{
	if (IsExperienceComplete(this))
	{
		// The ending seed. There is no authored night left to bring, so the Heart answers with the
		// sanctuary's memory instead of refusing in silence. A completion screen belongs here when one
		// exists (map.md Q3 / ticket T2); until then this is at least a legible terminal state rather
		// than an unhandled boundary condition.
		OnExperienceComplete();
		OnExperienceCompleteDelegate.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("VeilHeart: the authored experience is complete; the Heart has no further night to bring."));
		return;
	}

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
			// Generic restoration remains useful for legacy warning-tag feedback, but
			// interpretation receipts require this private native placement signal.
			// AVeilHeart is the sole subscriber granted access by the PCG subsystem;
			// neither Blueprints nor generic WorldForge/runtime code can broadcast it.
			PCGSub->PlacementAuthorizedRestoration.AddUObject(this, &AVeilHeart::OnPlacementAuthorizedRestoration);
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
	// An explicitly assigned catalog - authored on the Blueprint, or injected by a focused fixture - is the
	// caller's declared authority. The fail-closed contract check below guards the SHIPPED auto-load path,
	// which is the one a player build takes; applying it here would reject every fixture that deliberately
	// supplies a narrow catalog for one scenario.
	// A refusal is sticky and is checked FIRST: a catalog that failed the contract stays assigned so it can
	// be inspected in a log or a debugger, but it must never become usable again on a later call.
	if (bWarningCatalogLoadRefused)
	{
		return false;
	}

	if (WarningCatalog)
	{
		return true;
	}

	static const TCHAR* const CatalogPath = TEXT("/Game/Data/DA_VeilHeartWarningCatalog.DA_VeilHeartWarningCatalog");

	WarningCatalog = Cast<UVeilHeartWarningCatalog>(
		StaticLoadObject(UVeilHeartWarningCatalog::StaticClass(), nullptr, CatalogPath));
	if (!WarningCatalog)
	{
		bWarningCatalogLoadRefused = true;
		UE_LOG(LogTemp, Error,
			TEXT("VeilHeart: FAIL-CLOSED - could not load the authored warning catalog at %s. No authored night can ")
			TEXT("present its warning, so no night can begin. Re-import it with: pwsh -NoProfile -File ")
			TEXT("agent_collab/scripts/Invoke-GloamsteadDataAssetImport.ps1 -Manifest specs/data/vs-polish-starter.json"),
			CatalogPath);
		return false;
	}

	// Fail closed against the whole authored contract, in one central place. This deliberately does NOT
	// synthesize missing rows: a runtime substitute lets the shipped asset stay wrong indefinitely while the
	// game appears to work, which is exactly how the Cycle 1 tutorial row stayed missing through four cycles.
	TArray<FString> Problems;
	if (!ValidateWarningCatalogCoversAuthoredPlans(*WarningCatalog, Problems))
	{
		// The refusal flag - not a null pointer - is what makes this fail closed. Leaving the catalog assigned
		// keeps the actual defective content inspectable; nulling it turned a precise contract defect into a
		// misleading "no catalog at all" symptom.
		bWarningCatalogLoadRefused = true;
		for (const FString& Problem : Problems)
		{
			UE_LOG(LogTemp, Error, TEXT("VeilHeart: authored warning contract defect - %s"), *Problem);
		}
		UE_LOG(LogTemp, Error,
			TEXT("VeilHeart: FAIL-CLOSED - %s does not cover the authored experience contract (%d defect(s) listed ")
			TEXT("above). Add the missing row(s) to specs/data/vs-polish-starter.json and re-import with ")
			TEXT("agent_collab/scripts/Invoke-GloamsteadDataAssetImport.ps1. The warning catalog is authored ")
			TEXT("content, not runtime C++ content."),
			CatalogPath, Problems.Num());
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("VeilHeart: Loaded warning catalog from %s; the authored contract is satisfied for every plan."),
		CatalogPath);
	return true;
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

	// Show the new presenter what the player is currently owed. Without this, a warning earned at dawn is
	// delivered to whoever happened to be presenting THEN - and if that presenter detaches before day, the
	// player is left holding a night they were never warned about.
	if (UWorld* World = GetWorld())
	{
		if (UGloamsteadDayNightSubsystem* DayNight = World->GetSubsystem<UGloamsteadDayNightSubsystem>())
		{
			DayNight->NotifyWarningPresenterChanged();
		}
	}

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
		&& LastEmittedPlanId == Plan.PlanId
		&& LastEmittedWarningId == Plan.WarningId
		&& LastEmittedWarningNightType == Plan.NightType
		&& FindExactWarningById(Plan.WarningId, Plan.NightType) != nullptr;
}

bool AVeilHeart::HasRequiredSupportEvidence(const FExperienceCyclePlan& Plan) const
{
	return Plan.MinimumDistinctSupportCount >= 2
		&& EncounteredSupportIds.Num() >= Plan.MinimumDistinctSupportCount
		&& HasValidEncounteredSupportSet(Plan, EncounteredSupportIds);
}

bool AVeilHeart::HasValidEncounteredSupportSet(const FExperienceCyclePlan& Plan, const TSet<FName>& SupportIds) const
{
	if (Plan.RequiredSupportIds.IsEmpty())
	{
		return false;
	}

	for (const FName EncounteredId : SupportIds)
	{
		if (EncounteredId == NAME_None || !Plan.RequiredSupportIds.Contains(EncounteredId))
		{
			return false;
		}
	}
	return true;
}

bool AVeilHeart::ReceiptUsesExactlyEncounteredSupports(
	const FExperienceInterpretationReceipt& Receipt,
	const TSet<FName>& SupportIds) const
{
	if (Receipt.SupportIds.Num() != SupportIds.Num())
	{
		return false;
	}

	TSet<FName> ReceiptSupportIds;
	for (const FName SupportId : Receipt.SupportIds)
	{
		if (SupportId == NAME_None || ReceiptSupportIds.Contains(SupportId) || !SupportIds.Contains(SupportId))
		{
			return false;
		}
		ReceiptSupportIds.Add(SupportId);
	}

	return ReceiptSupportIds.Num() == SupportIds.Num();
}

bool AVeilHeart::DoesReceiptProveExactPlan(
	const FExperienceInterpretationReceipt& Receipt,
	const FExperienceCyclePlan& Plan,
	const TSet<FName>& SupportIds) const
{
	const FVeilHeartWarningFragment* ExactWarning = FindExactWarningById(Plan.WarningId, Plan.NightType);
	UGloamsteadPCGSubsystem* PCG = ResolvePCGSubsystem();
	FString ContractError;
	if (!Plan.IsAuthoredPlan()
		|| !ExactWarning
		|| !ExactWarning->MatchesExactPlanContract(Plan, &ContractError)
		|| !PCG
		|| !Receipt.IsValid()
		|| !HasValidEncounteredSupportSet(Plan, SupportIds)
		|| SupportIds.Num() < Plan.MinimumDistinctSupportCount
		|| !ReceiptUsesExactlyEncounteredSupports(Receipt, SupportIds)
		|| Receipt.ReceiptId != Plan.InterpretationReceiptId
		|| Receipt.PlanId != Plan.PlanId
		|| Receipt.WarningId != Plan.WarningId
		|| Receipt.SemanticSubject != Plan.SemanticSubject
		|| Receipt.RestorationRitualType != Plan.RequiredRitualType
		|| Receipt.RestorationPointIndex == INDEX_NONE
		|| !PCG->IsPointRestored(Receipt.RestorationPointIndex)
		|| !PCG->PointMatchesExperiencePlan(Receipt.RestorationPointIndex, Plan, /*bRequireRestored*/ true)
		|| !Plan.RequiredRestorationTags.Contains(Receipt.RestorationTag))
	{
		return false;
	}

	return true;
}

void AVeilHeart::OnRestorationComplete(const FRestorationEventPayload& Payload)
{
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: Restoration received - Ritual: %d, LightDelta: %.2f, CorruptionCleared: %.2f, Warning: %s, WarningTag: %s, Subject: %s"),
		static_cast<int32>(Payload.RitualType), Payload.LightDelta, Payload.CorruptionCleared,
		*Payload.WarningId.ToString(), *Payload.WarningTagSatisfied.ToString(), *Payload.SemanticSubject.ToString());

	EvaluateRestorationAgainstWarnings(Payload);
	// OnStructureRestored is deliberately generic and Blueprint-observable.
	// It may retain legacy tag feedback, but cannot assert a player-confirmed
	// authored ritual or mint an interpretation receipt.
}

void AVeilHeart::OnPlacementAuthorizedRestoration(const FRestorationEventPayload& Payload)
{
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: placement-authorized restoration received for point %d."), Payload.PointIndex);
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

	// A player can still find another authored clue after earning the receipt.
	// Keep that legitimate knowledge restorable by expanding the receipt to the
	// exact new encountered set, but only if the expanded receipt continues to
	// prove this exact active plan. Anything else is stale/corrupt state and may
	// not survive into a v3 snapshot.
	if (LastInterpretationReceipt.HasAnyFacts())
	{
		FExperienceInterpretationReceipt ExpandedReceipt = LastInterpretationReceipt;
		ExpandedReceipt.SupportIds = EncounteredSupportIds.Array();
		if (DoesReceiptProveExactPlan(ExpandedReceipt, *ActivePlan, EncounteredSupportIds))
		{
			LastInterpretationReceipt = MoveTemp(ExpandedReceipt);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("VeilHeart: clearing a stale interpretation receipt after a new support encounter."));
			LastInterpretationReceipt = FExperienceInterpretationReceipt();
		}
	}

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
	State.PresentedPlanId = LastEmittedPlanId;
	State.PresentedWarningId = LastEmittedWarningId;
	State.EncounteredSupportIds = EncounteredSupportIds.Array();
	State.InterpretationReceipt = LastInterpretationReceipt;

	// A receipt is created from this exact set in EvaluateRestorationAgainstActivePlan.
	// If memory corruption or a future caller violates that invariant, do not
	// persist a self-contradictory v3 snapshot for a later restore to interpret.
	const FExperienceCyclePlan* ActivePlan = ResolveActivePlan();
	if (LastInterpretationReceipt.HasAnyFacts()
		&& (!ActivePlan
			|| !DoesReceiptProveExactPlan(LastInterpretationReceipt, *ActivePlan, EncounteredSupportIds)))
	{
		UE_LOG(LogTemp, Warning, TEXT("VeilHeart: refusing to capture a receipt whose support set does not exactly match encountered evidence."));
		State.Reset();
	}
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
	LastEmittedPlanId = NAME_None;
	LastEmittedWarningId = NAME_None;
	LastEmittedWarningNightType = ENightConsequenceType::Invalid;
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
		|| State.PresentedPlanId != ActivePlan->PlanId
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

	// Validate every persisted fact against local values before mutating the
	// Heart. In particular, a receipt may not name a superset/subset of the
	// encountered evidence: array order is irrelevant, exact set equality is
	// required. This leaves the Heart fully reset on any failed v3 restore.
	if (State.InterpretationReceipt.HasAnyFacts())
	{
		if (!State.InterpretationReceipt.IsValid()
			|| !DoesReceiptProveExactPlan(State.InterpretationReceipt, *ActivePlan, RestoredSupportIds))
		{
			UE_LOG(LogTemp, Warning, TEXT("VeilHeart: refusing persisted interpretation receipt because its facts are not an exact proof of the active plan."));
			return false;
		}
	}

	LastEmittedPlanId = State.PresentedPlanId;
	LastEmittedWarningId = State.PresentedWarningId;
	LastEmittedWarningNightType = ActivePlan->NightType;
	EncounteredSupportIds = MoveTemp(RestoredSupportIds);
	LastInterpretationReceipt = State.InterpretationReceipt;
	return true;
}

bool AVeilHeart::HasExactInterpretationForPlan(const FExperienceCyclePlan& Plan) const
{
	return DoesReceiptProveExactPlan(LastInterpretationReceipt, Plan, EncounteredSupportIds);
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
		if (Candidate.WarningId != WarningId || Candidate.AssociatedNightType != ExpectedNightType)
		{
			continue;
		}

		if (ExactWarning)
		{
			// The same warning identity may intentionally have a different
			// authored fragment for another night type, but it may not have two
			// competing fragments for this exact plan contract.
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
	// Every later authored warning is different: its identity alone is not
	// admission; subject, ritual, tag, support IDs, and distinct media must agree
	// with the active plan before the Day authority can make rest available.
	if (!(Plan.NightType == ENightConsequenceType::Tutorial
		&& Plan.WarningId == FName(TEXT("TutorialLostPath"))))
	{
		FString ContractError;
		if (!ExactWarning->MatchesExactPlanContract(Plan, &ContractError))
		{
			UE_LOG(LogTemp, Warning, TEXT("VeilHeart: authored warning %s presentation refused because its contract is invalid: %s."),
				*Plan.WarningId.ToString(), *ContractError);
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
	if (LastEmittedPlanId != Plan.PlanId
		|| LastEmittedWarningId != Plan.WarningId
		|| LastEmittedWarningNightType != Plan.NightType)
	{
		// A new authored plan starts a new interpretation ledger. This matters
		// when a later night deliberately reuses a warning identity.
		EncounteredSupportIds.Reset();
		LastInterpretationReceipt = FExperienceInterpretationReceipt();
	}
	UE_LOG(LogTemp, Log, TEXT("VeilHeart: authored Day warning [%s] for night %s."),
		*ExactWarning->WarningId.ToString(), *GetNightConsequenceTypeDisplayName(Plan.NightType));
	LastEmittedPlanId = Plan.PlanId;
	LastEmittedWarningId = ExactWarning->WarningId;
	LastEmittedWarningNightType = Plan.NightType;
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
		EncounteredSupportIds.Reset();
		LastInterpretationReceipt = FExperienceInterpretationReceipt();
		LastEmittedPlanId = NAME_None;
		LastEmittedWarningId = Warning->WarningId;
		LastEmittedWarningNightType = NightType;
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
	EncounteredSupportIds.Reset();
	LastInterpretationReceipt = FExperienceInterpretationReceipt();
	LastEmittedPlanId = NAME_None;
	LastEmittedWarningId = ExactWarning->WarningId;
	LastEmittedWarningNightType = ExpectedNightType;
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
