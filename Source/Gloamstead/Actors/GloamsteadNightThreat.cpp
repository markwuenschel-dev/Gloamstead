#include "Actors/GloamsteadNightThreat.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PCG/GloamsteadPCGSubsystem.h"
#include "UObject/ConstructorHelpers.h"

/**
 * Named, not anonymous: this module builds through adaptive unity, where an anonymous namespace does
 * not keep file-local names apart (the C2264 note repeated across this module).
 */
namespace GloamsteadNightThreatBody
{
	/**
	 * The stock mannequin, used deliberately rather than for want of anything better.
	 *
	 * A night threat had no skeletal mesh of any kind: NightConsequenceRuntime spawns
	 * AGloamsteadNightThreat::StaticClass() directly, and this project's convention puts mesh and
	 * AnimBP on a Blueprint child - of which none exists for this class. So every Gatherer,
	 * Borrowed, Bargainer and Echo walked the sanctuary, drained light and died invisibly, across
	 * four of the six cycles.
	 *
	 * A featureless grey humanoid is also the honest read for what these are: the Gloam wearing the
	 * shape of something the sanctuary knew. A bespoke silhouette per archetype is real art work and
	 * is still owed; this is the body that makes the night legible today.
	 */
	constexpr const TCHAR* BodyMeshPath =
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");
	constexpr const TCHAR* BodyAnimPath =
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C");

	/** Mannequin meshes are authored feet-at-origin facing +Y; a capsule wants centre and +X. */
	const FVector MeshOffset(0.f, 0.f, -90.f);
	const FRotator MeshRotation(0.f, -90.f, 0.f);
}

FString GetNightThreatStateDisplayName(ENightThreatState State)
{
	switch (State)
	{
	case ENightThreatState::Approaching: return TEXT("Approaching");
	case ENightThreatState::Repelled:    return TEXT("Repelled");
	case ENightThreatState::Working:     return TEXT("Working");
	case ENightThreatState::Disrupted:   return TEXT("Disrupted");
	case ENightThreatState::Resolved:    return TEXT("Resolved");
	default:                             return TEXT("Unknown");
	}
}

AGloamsteadNightThreat::AGloamsteadNightThreat()
{
	PrimaryActorTick.bCanEverTick = true;

	// These threats want restored structures, not the player, and their behaviour is the handful of
	// rules in StepBehaviour. Possessing the Combat AI controller would start a StateTree that
	// pursues the player and fights this actor's own movement for control of the same pawn.
	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::Disabled;

	// A night threat is pressure, not a duel. Three hits from a player who chose to spend the time
	// is deliberate: striking is a way to buy seconds, never the way to win the night.
	MaxHP = 3.0f;
	MeleeDamage = 1.0f;
	DeathRemovalTime = 2.0f;

	// A body, at last. ConstructorHelpers rather than a runtime load so the mesh is part of this
	// class's CDO and the cost is paid once at class load, not on every spawn during a night.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMeshFinder(
			GloamsteadNightThreatBody::BodyMeshPath);
		if (BodyMeshFinder.Succeeded())
		{
			Body->SetSkeletalMeshAsset(BodyMeshFinder.Object);
		}

		static ConstructorHelpers::FClassFinder<UAnimInstance> BodyAnimFinder(
			GloamsteadNightThreatBody::BodyAnimPath);
		if (BodyAnimFinder.Succeeded())
		{
			Body->SetAnimInstanceClass(BodyAnimFinder.Class);
		}

		Body->SetRelativeLocationAndRotation(
			GloamsteadNightThreatBody::MeshOffset, GloamsteadNightThreatBody::MeshRotation);
		// The body is scenery on a pawn the player never collides with for gameplay reasons; the
		// capsule already owns the space.
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	GloamGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("GloamGlow"));
	GloamGlow->SetupAttachment(GetCapsuleComponent());
	GloamGlow->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	GloamGlow->SetMobility(EComponentMobility::Movable);
	GloamGlow->SetCastShadows(false);
	GloamGlow->SetAttenuationRadius(420.f);
	GloamGlow->SetIntensity(1400.f);

	// The threat walks; without a rotation rate it slides facing wherever it spawned. SetActorRotation
	// in Tick already turns it, so this only smooths what the movement component would fight.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bUseControllerDesiredRotation = false;
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.f, 220.f, 0.f);
	}
}

FLinearColor AGloamsteadNightThreat::GetArchetypeGlowColor(ENightThreatArchetype Archetype)
{
	switch (Archetype)
	{
	// It takes light out of what you built, so it burns like a stolen ember.
	case ENightThreatArchetype::Gatherer:  return FLinearColor(1.00f, 0.42f, 0.16f);
	// Wearing a shape that was the sanctuary's; sickly green is the tell that it is not.
	case ENightThreatArchetype::Borrowed:  return FLinearColor(0.36f, 0.95f, 0.55f);
	// Stands at the edge offering shortcuts. Warm and inviting, which is the trap.
	case ENightThreatArchetype::Bargainer: return FLinearColor(0.98f, 0.84f, 0.35f);
	// Costs attention, not light. Pale and cold, and dimmer than the rest by design.
	case ENightThreatArchetype::Echo:      return FLinearColor(0.62f, 0.72f, 0.95f);
	default:                               return FLinearColor(0.80f, 0.80f, 0.85f);
	}
}

bool AGloamsteadNightThreat::HasVisibleBody() const
{
	const USkeletalMeshComponent* Body = GetMesh();
	return Body != nullptr && Body->GetSkeletalMeshAsset() != nullptr;
}

void AGloamsteadNightThreat::ApplyArchetypePresentation()
{
	if (!GloamGlow)
	{
		return;
	}

	GloamGlow->SetLightColor(GetArchetypeGlowColor(Spec.Archetype));

	// Silhouette does the work colour cannot at close range: what is coming for the lantern is not
	// the same size as what is standing at the treeline offering you something.
	float BodyScale = 1.f;
	float GlowIntensity = 1400.f;
	switch (Spec.Archetype)
	{
	case ENightThreatArchetype::Gatherer:  BodyScale = 1.05f; GlowIntensity = 1600.f; break;
	case ENightThreatArchetype::Borrowed:  BodyScale = 1.00f; GlowIntensity = 1300.f; break;
	case ENightThreatArchetype::Bargainer: BodyScale = 1.18f; GlowIntensity = 1100.f; break;
	// An Echo drains nothing and deals nothing. It should look like the least of them, because it is.
	case ENightThreatArchetype::Echo:      BodyScale = 0.82f; GlowIntensity = 600.f;  break;
	default: break;
	}

	GloamGlow->SetIntensity(GlowIntensity);
	SetActorScale3D(FVector(BodyScale));
}

void AGloamsteadNightThreat::BeginPlay()
{
	Super::BeginPlay();

	if (const UWorld* World = GetWorld())
	{
		CachedPCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = ApproachSpeed;
	}
}

void AGloamsteadNightThreat::ConfigureThreat(const FNightThreatSpec& InSpec, int32 InBoundPointIndex)
{
	Spec = InSpec;
	BoundPointIndex = InBoundPointIndex;
	ThreatState = ENightThreatState::Approaching;
	LightDrained = 0.0f;
	DisruptionRemaining = 0.0f;

	// An Echo never touches the world state it imitates. It repeats what it saw a few seconds late
	// and in the wrong place, which costs the player attention rather than light - so it is spawned
	// already unable to drain anything, and its own damage is nil.
	if (Spec.Archetype == ENightThreatArchetype::Echo)
	{
		LightDrainPerSecond = 0.0f;
		MeleeDamage = 0.0f;
	}

	// A Bargainer holds at the edge of the light and waits to be answered. It does not close, and it
	// does not drain: its pressure is entirely the shortcut it is offering.
	if (Spec.Archetype == ENightThreatArchetype::Bargainer)
	{
		LightDrainPerSecond = 0.0f;
		MeleeDamage = 0.0f;
	}

	// Last, because it reads Spec: the archetype only exists as something to look at once it is set.
	ApplyArchetypePresentation();
}

float AGloamsteadNightThreat::ComputeAdvanceScale(float LightOnThreat, float RepelledAtLightLevel)
{
	const float Threshold = FMath::Clamp(RepelledAtLightLevel, 0.0f, 1.0f);
	const float Light = FMath::Clamp(LightOnThreat, 0.0f, 1.0f);

	if (Threshold <= 0.0f)
	{
		// Authored to be stopped by any light at all.
		return Light > 0.0f ? 0.0f : 1.0f;
	}

	if (Light >= Threshold)
	{
		return 0.0f;
	}

	// Linear falloff from full speed in the dark to pinned at the threshold. Deliberately linear:
	// the player needs to be able to feel that adding light slowed it, and a curve reads as noise.
	return FMath::Clamp(1.0f - (Light / Threshold), 0.0f, 1.0f);
}

bool AGloamsteadNightThreat::IsRepelledByLight(float LightOnThreat, float RepelledAtLightLevel)
{
	return ComputeAdvanceScale(LightOnThreat, RepelledAtLightLevel) <= 0.0f;
}

bool AGloamsteadNightThreat::ResolveTargetLocation(
	const UGloamsteadPCGSubsystem* PCG,
	FVector& OutLocation,
	int32& OutPointIndex) const
{
	OutPointIndex = INDEX_NONE;
	if (!PCG)
	{
		return false;
	}

	switch (Spec.TargetPreference)
	{
	case ENightThreatTarget::BrightestRestored:
		OutPointIndex = PCG->FindRestoredPointIndex(/*bMostLit*/ true);
		break;
	case ENightThreatTarget::ObjectivePoint:
	case ENightThreatTarget::EdgeOfLight:
	default:
		OutPointIndex = BoundPointIndex;
		break;
	}

	if (OutPointIndex == INDEX_NONE)
	{
		// A thief with nothing left to steal has nowhere to be. Falling back to the bound point keeps
		// it in the night instead of silently freezing it at the world origin.
		OutPointIndex = BoundPointIndex;
	}

	FPCGPoint Point;
	if (OutPointIndex == INDEX_NONE || !PCG->GetPointByIndex(OutPointIndex, Point))
	{
		return false;
	}

	OutLocation = Point.Transform.GetLocation();
	return true;
}

void AGloamsteadNightThreat::StepBehaviour(
	float DeltaSeconds,
	float LightOnThreat,
	float DistanceToTarget,
	UGloamsteadPCGSubsystem* PCG)
{
	if (ThreatState == ENightThreatState::Resolved || DeltaSeconds <= 0.0f)
	{
		return;
	}

	if (DisruptionRemaining > 0.0f)
	{
		DisruptionRemaining = FMath::Max(0.0f, DisruptionRemaining - DeltaSeconds);
		if (DisruptionRemaining > 0.0f)
		{
			ThreatState = ENightThreatState::Disrupted;
			return;
		}
	}

	const float AdvanceScale = ComputeAdvanceScale(LightOnThreat, Spec.RepelledAtLightLevel);
	if (AdvanceScale <= 0.0f)
	{
		// Held off by the player's own light. A Gatherer standing in a lit segment is the single
		// clearest statement the game makes about what restoration is for.
		ThreatState = ENightThreatState::Repelled;
		return;
	}

	// A Bargainer never closes. It stands at the edge of the light for the whole night, which is
	// what makes it a test of judgement rather than of positioning.
	if (Spec.Archetype == ENightThreatArchetype::Bargainer)
	{
		ThreatState = ENightThreatState::Approaching;
		return;
	}

	if (DistanceToTarget > WorkingRadius)
	{
		ThreatState = ENightThreatState::Approaching;
		return;
	}

	ThreatState = ENightThreatState::Working;

	if (LightDrainPerSecond <= 0.0f || BoundPointIndex == INDEX_NONE || !PCG)
	{
		return;
	}

	// Working scales with how dark it is, exactly like approaching. A threat that reached a still-lit
	// structure drains it slowly enough that the player can answer without a perfect reaction.
	const float Drain = LightDrainPerSecond * AdvanceScale * DeltaSeconds;
	LightDrained += Drain;

	// Taking light out is expressed as corruption at the point, because that is the one mutation the
	// PCG subsystem exposes and the one the night's own outcome already reads. A threat that "puts
	// out a lantern" and a night that scores corruption must not be two different bookkeepings.
	PCG->AddCorruptionAtIndex(BoundPointIndex, Drain);
}

void AGloamsteadNightThreat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ThreatState == ENightThreatState::Resolved || CurrentHP <= 0.0f)
	{
		return;
	}

	if (!CachedPCG)
	{
		if (const UWorld* World = GetWorld())
		{
			CachedPCG = World->GetSubsystem<UGloamsteadPCGSubsystem>();
		}
		if (!CachedPCG)
		{
			return;
		}
	}

	FVector TargetLocation = FVector::ZeroVector;
	int32 TargetIndex = INDEX_NONE;
	if (!ResolveTargetLocation(CachedPCG, TargetLocation, TargetIndex))
	{
		return;
	}

	// A threat is lit by the point it is heading for. Sampling the destination rather than the
	// sanctuary average is what makes a single restored lantern locally meaningful - a bright
	// sanctuary average must not excuse one dark corner, and one dark corner must not doom a bright
	// sanctuary.
	const float LightOnThreat = CachedPCG->GetLightLevel(TargetIndex);
	const FVector ToTarget = TargetLocation - GetActorLocation();
	const float DistanceToTarget = ToTarget.Size();

	StepBehaviour(DeltaSeconds, LightOnThreat, DistanceToTarget, CachedPCG);

	if (ThreatState == ENightThreatState::Approaching && DistanceToTarget > KINDA_SMALL_NUMBER)
	{
		const float AdvanceScale = ComputeAdvanceScale(LightOnThreat, Spec.RepelledAtLightLevel);
		const FVector Direction = ToTarget.GetSafeNormal();
		AddMovementInput(Direction, AdvanceScale);
		SetActorRotation(Direction.Rotation());
	}
}

void AGloamsteadNightThreat::ApplyDamage(
	float Damage,
	AActor* DamageCauser,
	const FVector& DamageLocation,
	const FVector& DamageImpulse)
{
	Super::ApplyDamage(Damage, DamageCauser, DamageLocation, DamageImpulse);

	// Interrupt even on a hit that did not kill: the point of striking a threat is time, not damage.
	if (CurrentHP > 0.0f)
	{
		Disrupt();
	}
}

bool AGloamsteadNightThreat::Disrupt()
{
	if (ThreatState == ENightThreatState::Resolved)
	{
		return false;
	}

	DisruptionRemaining = DisruptionSeconds;
	ThreatState = ENightThreatState::Disrupted;

	UE_LOG(LogTemp, Log, TEXT("NightThreat: %s disrupted for %.1fs."),
		*GetNightThreatArchetypeDisplayName(Spec.Archetype), DisruptionSeconds);
	return true;
}

bool AGloamsteadNightThreat::Cleanse()
{
	if (ThreatState == ENightThreatState::Resolved)
	{
		return false;
	}

	// The Borrowed is the one archetype cleansing cannot simply end. Its tether has to be visible
	// first, and the only thing that exposes a tether is a mirror the player faced correctly on a
	// previous day. That is the entire Cycle IV lesson expressed as one precondition.
	if (Spec.Archetype == ENightThreatArchetype::Borrowed && !bTetherExposed)
	{
		UE_LOG(LogTemp, Log,
			TEXT("NightThreat: The Borrowed cannot be cleansed while its tether is hidden. "
				 "The mirror must be facing the stolen light."));
		return false;
	}

	ThreatState = ENightThreatState::Resolved;
	DisruptionRemaining = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("NightThreat: %s cleansed."),
		*GetNightThreatArchetypeDisplayName(Spec.Archetype));
	return true;
}
