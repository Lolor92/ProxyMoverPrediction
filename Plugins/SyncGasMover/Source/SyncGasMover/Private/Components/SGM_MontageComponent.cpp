#include "Components/SGM_MontageComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "LayeredMoves/SGM_AnimRootMotionLayeredMove.h"
#include "MoverComponent.h"
#include "Net/UnrealNetwork.h"
#include "Tags/SGM_NativeTags.h"

namespace
{
	constexpr float ContactInitialProbeInflation = 5.0f;
	constexpr float ContactStillBlockingSlack = 12.0f;

	FString SGMLogActorState(const UObject* WorldContextObject, const AActor* Actor)
	{
		const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		const APawn* Pawn = Cast<APawn>(Actor);

		return FString::Printf(TEXT("World=%s NetMode=%d Actor=%s Role=%d RemoteRole=%d Local=%d Auth=%d Loc=%s"),
			World ? *World->GetName() : TEXT("None"),
			World ? static_cast<int32>(World->GetNetMode()) : -1,
			*GetNameSafe(Actor),
			Actor ? static_cast<int32>(Actor->GetLocalRole()) : -1,
			Actor ? static_cast<int32>(Actor->GetRemoteRole()) : -1,
			Pawn ? Pawn->IsLocallyControlled() : false,
			Actor ? Actor->HasAuthority() : false,
			Actor ? *Actor->GetActorLocation().ToString() : TEXT("None"));
	}

	float NormalizeReleasePercent(float InReleasePercent)
	{
		const float NormalizedPercent = InReleasePercent > 1.0f
			? InReleasePercent / 100.0f
			: InReleasePercent;

		return FMath::Clamp(NormalizedPercent, 0.0f, 1.0f);
	}
}

USGM_MontageComponent::USGM_MontageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void USGM_MontageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USGM_MontageComponent, RepMontageState);
	DOREPLIFETIME(USGM_MontageComponent, bCanBlendUpperAndLowerBody);
}

void USGM_MontageComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveMeshComponent();
}

void USGM_MontageComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateRootMotionControl(DeltaTime);
}

bool USGM_MontageComponent::PlayMontageLocal(UAnimMontage* InMontage, float InPlayRate, float InStartTimeSeconds,
	FName InStartSection)
{
	if (!InMontage) return false;

	ResolveMeshComponent();
	if (!MontageMeshComponent) return false;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance) return false;

	const float MontageLength = InMontage->GetPlayLength();
	const float ClampedStartTime = FMath::Clamp(
		InStartTimeSeconds,
		0.0f,
		FMath::Max(0.0f, MontageLength - KINDA_SMALL_NUMBER));

	const float PlayedLength = AnimInstance->Montage_Play(InMontage, InPlayRate,
		EMontagePlayReturnType::MontageLength, ClampedStartTime, true);

	if (PlayedLength <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG PlayMontageLocal FAIL PlayedLength=%.3f %s Montage=%s Start=%.3f PlayRate=%.3f"),
			PlayedLength, *SGMLogActorState(this, GetOwner()), *GetNameSafe(InMontage), ClampedStartTime, InPlayRate);
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG PlayMontageLocal SUCCESS %s Montage=%s Start=%.3f PlayRate=%.3f Length=%.3f HasRootMotion=%d"),
		*SGMLogActorState(this, GetOwner()), *GetNameSafe(InMontage), ClampedStartTime, InPlayRate,
		InMontage->GetPlayLength(), InMontage->HasRootMotion());

	if (InStartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(InStartSection, InMontage);
	}

	if (InPlayRate != 0.0f && InMontage->HasRootMotion())
	{
		if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(InMontage))
		{
			MontageInstance->PushDisableRootMotion();
			QueueRootMotionMove(InMontage, InPlayRate, MontageInstance->GetPosition());
		}
	}

	SetComponentTickEnabled(true);
	return true;
}

bool USGM_MontageComponent::StopMontageLocal(UAnimMontage* InMontage)
{
	if (!InMontage) return false;

	ResolveMeshComponent();
	if (!MontageMeshComponent) return false;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(InMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG StopMontageLocal SKIP %s Montage=%s HasAnim=%d IsPlaying=%d"),
			*SGMLogActorState(this, GetOwner()), *GetNameSafe(InMontage), AnimInstance != nullptr,
			AnimInstance ? AnimInstance->Montage_IsPlaying(InMontage) : false);
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG StopMontageLocal STOP %s Montage=%s Pos=%.3f BlendOut=%.3f"),
		*SGMLogActorState(this, GetOwner()), *GetNameSafe(InMontage),
		AnimInstance->Montage_GetPosition(InMontage), InMontage->GetDefaultBlendOutTime());

	AnimInstance->Montage_Stop(InMontage->GetDefaultBlendOutTime(), InMontage);
	ResetLocalRootMotionControlState();
	SetComponentTickEnabled(false);
	return true;
}

bool USGM_MontageComponent::PlayMontageVisualOnlyLocal(UAnimMontage* InMontage, float InPlayRate,
	float InStartTimeSeconds, FName InStartSection)
{
	if (!InMontage) return false;

	ResolveMeshComponent();
	if (!MontageMeshComponent) return false;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance) return false;

	const float MontageLength = InMontage->GetPlayLength();
	const float ClampedStartTime = FMath::Clamp(
		InStartTimeSeconds,
		0.0f,
		FMath::Max(0.0f, MontageLength - KINDA_SMALL_NUMBER));

	const float PlayedLength = AnimInstance->Montage_Play(InMontage, InPlayRate,
		EMontagePlayReturnType::MontageLength, ClampedStartTime, true);

	if (PlayedLength <= 0.0f) return false;

	if (InStartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(InStartSection, InMontage);
	}

	if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(InMontage))
	{
		MontageInstance->PushDisableRootMotion();
	}

	SetComponentTickEnabled(true);
	return true;
}

bool USGM_MontageComponent::PlayPredictedReplicatedMontage(UAnimMontage* InMontage, float InPlayRate,
	float InStartTimeSeconds, FName InStartSection)
{
	if (!InMontage) return false;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return false;

	if (OwnerActor->HasAuthority())
	{
		return StartReplicatedMontage(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
	}

	UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG StartReplicatedMontage ENTER %s Montage=%s Start=%.3f PlayRate=%.3f OldSerial=%d"),
		*SGMLogActorState(this, GetOwner()), *GetNameSafe(InMontage), InStartTimeSeconds, InPlayRate, RepMontageState.Serial);

	const bool bPlayedLocally = PlayMontageLocal(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
	if (!bPlayedLocally)
	{
		UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG StartReplicatedMontage FAIL local play %s Montage=%s"),
			*SGMLogActorState(this, GetOwner()), *GetNameSafe(InMontage));
		return false;
	}

	RepMontageState.Montage = InMontage;
	RepMontageState.PlayRate = InPlayRate;
	RepMontageState.StartTimeSeconds = InStartTimeSeconds;
	RepMontageState.StartSection = InStartSection;
	RepMontageState.bIsPlaying = true;
	RepMontageState.bRootMotionDisabled = false;
	RepMontageState.RootMotionScale = 1.0f;
	ResetLocalRootMotionControlState();
	SetCanBlendUpperAndLowerBody(false);

	BindContactBlockingEvents();
	RefreshInitialContactBlockState();

	ServerPlayReplicatedMontage(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
	return true;
}

bool USGM_MontageComponent::DisableRootMotionForMontage(UAnimMontage* InMontage)
{
	if (!InMontage) return false;

	ResolveMeshComponent();
	if (!MontageMeshComponent) return false;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance) return false;

	FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(InMontage);
	if (!MontageInstance) return false;

	MontageInstance->PushDisableRootMotion();

	if (GetMoverComponent())
	{
		QueueRootMotionMove(InMontage, AnimInstance->Montage_GetPlayRate(InMontage), MontageInstance->GetPosition(), 0.0f);
	}

	return true;
}

bool USGM_MontageComponent::DisableRootMotionPredictedReplicated()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return false;

	const bool bDisabledLocally = RepMontageState.Montage
		? DisableRootMotionForMontage(RepMontageState.Montage)
		: false;

	if (OwnerActor->HasAuthority())
	{
		return DisableRootMotionForReplicatedMontage();
	}

	if (OwnerActor->GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerDisableRootMotionForReplicatedMontage();
	}

	return bDisabledLocally;
}

USkeletalMeshComponent* USGM_MontageComponent::GetResolvedMontageMeshComponent()
{
	ResolveMeshComponent();
	return MontageMeshComponent;
}

void USGM_MontageComponent::StartRootMotionReleaseAtMontagePercent(float ReleasePercent)
{
	RootMotionReleasePercent = NormalizeReleasePercent(ReleasePercent);
	bRootMotionReleasedByPercent = false;
	SetCanBlendUpperAndLowerBody(false);
	SetComponentTickEnabled(true);
}

void USGM_MontageComponent::ClearRootMotionRelease()
{
	RootMotionReleasePercent = -1.0f;
	bRootMotionReleasedByPercent = false;
	SetCanBlendUpperAndLowerBody(false);
}

void USGM_MontageComponent::SetRootMotionContactBlockingAngleDegrees(float HalfAngleDegrees)
{
	ContactBlockHalfAngleDegrees = FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f);
	bEnableRootMotionContactBlocking = ContactBlockHalfAngleDegrees > UE_KINDA_SMALL_NUMBER;

	if (bEnableRootMotionContactBlocking && ShouldDrivePredictedRootMotionControl())
	{
		BindContactBlockingEvents();
		ApplyRootMotionScaleToCurrentMontage(RepMontageState.RootMotionScale);
		RefreshInitialContactBlockState();
	}
	else
	{
		ContactBlockingActors.Reset();
		UnbindContactBlockingEvents();
		SetContactRootMotionBlocked(false);
	}

	SetComponentTickEnabled(RootMotionReleasePercent >= 0.0f || RepMontageState.bIsPlaying || bRootMotionBlockedByContact);
}

bool USGM_MontageComponent::ShouldBlockMovementInputDuringRootMotion() const
{
	if (!RepMontageState.Montage || !RepMontageState.bIsPlaying)
	{
		return false;
	}

	if (RepMontageState.bRootMotionDisabled || bCanBlendUpperAndLowerBody)
	{
		return false;
	}

	return !bRootMotionReleasedByPercent;
}

bool USGM_MontageComponent::TryReleaseRootMotionForMovementInput()
{
	if (!RepMontageState.Montage || !RepMontageState.bIsPlaying)
	{
		return false;
	}

	if (ShouldBlockMovementInputDuringRootMotion())
	{
		return false;
	}

	if (!bRootMotionReleasedByPercent)
	{
		return false;
	}

	ContactBlockingActors.Reset();
	SetContactRootMotionBlocked(false);
	SetCanBlendUpperAndLowerBody(true);

	if (RepMontageState.bRootMotionDisabled)
	{
		return true;
	}

	return DisableRootMotionPredictedReplicated();
}

void USGM_MontageComponent::SetCanBlendUpperAndLowerBody(bool bInCanBlend)
{
	if (bCanBlendUpperAndLowerBody == bInCanBlend)
	{
		return;
	}

	bCanBlendUpperAndLowerBody = bInCanBlend;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || OwnerActor->HasAuthority()) return;

	if (OwnerActor->GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerSetCanBlendUpperAndLowerBody(bInCanBlend);
	}
}

bool USGM_MontageComponent::DisableRootMotionForReplicatedMontage()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	if (!RepMontageState.Montage || !RepMontageState.bIsPlaying) return false;

	const bool bDisabledLocally = DisableRootMotionForMontage(RepMontageState.Montage);
	if (!bDisabledLocally) return false;

	RepMontageState.bRootMotionDisabled = true;
	RepMontageState.RootMotionScale = 0.0f;
	RepMontageState.DisableRootMotionSerial++;
	RepMontageState.RootMotionScaleSerial++;

	return true;
}

bool USGM_MontageComponent::StartReplicatedMontage(UAnimMontage* InMontage, float InPlayRate,
	float InStartTimeSeconds, FName InStartSection)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	if (!InMontage) return false;

	const bool bPlayedLocally = PlayMontageLocal(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
	if (!bPlayedLocally) return false;

	RepMontageState.Montage = InMontage;
	RepMontageState.PlayRate = InPlayRate;
	RepMontageState.StartTimeSeconds = InStartTimeSeconds;
	RepMontageState.StartSection = InStartSection;
	RepMontageState.bIsPlaying = true;
	RepMontageState.bRootMotionDisabled = false;
	RepMontageState.RootMotionScale = 1.0f;

	ResetLocalRootMotionControlState();
	SetCanBlendUpperAndLowerBody(false);
	RepMontageState.Serial++;

	BindContactBlockingEvents();
	RefreshInitialContactBlockState();

	SetComponentTickEnabled(true);
	UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG StartReplicatedMontage SUCCESS %s Montage=%s NewSerial=%d RootMotionScale=%.3f"),
		*SGMLogActorState(this, GetOwner()), *GetNameSafe(InMontage), RepMontageState.Serial, RepMontageState.RootMotionScale);

	return true;
}

void USGM_MontageComponent::ServerPlayReplicatedMontage_Implementation(UAnimMontage* InMontage, float InPlayRate,
	float InStartTimeSeconds, FName InStartSection)
{
	UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG ServerPlayReplicatedMontage ENTER %s Montage=%s Start=%.3f PlayRate=%.3f CurrentSerial=%d IsPlaying=%d CurrentMontage=%s"),
		*SGMLogActorState(this, GetOwner()), *GetNameSafe(InMontage), InStartTimeSeconds, InPlayRate,
		RepMontageState.Serial, RepMontageState.bIsPlaying, *GetNameSafe(RepMontageState.Montage));

	if (RepMontageState.bIsPlaying
		&& RepMontageState.Montage == InMontage
		&& FMath::IsNearlyEqual(RepMontageState.PlayRate, InPlayRate)
		&& FMath::IsNearlyEqual(RepMontageState.StartTimeSeconds, InStartTimeSeconds)
		&& RepMontageState.StartSection == InStartSection)
	{
		ResolveMeshComponent();

		float CurrentServerMontagePosition = 0.0f;
		bool bServerAlreadyPlayingSameMontage = false;

		if (MontageMeshComponent)
		{
			if (UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance())
			{
				if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(InMontage))
				{
					bServerAlreadyPlayingSameMontage = true;
					CurrentServerMontagePosition = MontageInstance->GetPosition();
				}
			}
		}

		constexpr float DuplicatePredictedPlayRejectWindowSeconds = 0.75f;
		if (bServerAlreadyPlayingSameMontage && CurrentServerMontagePosition <= DuplicatePredictedPlayRejectWindowSeconds)
		{
			UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG ServerPlayReplicatedMontage DUPLICATE_REJECT %s Montage=%s ServerPos=%.3f Window=%.3f Serial=%d"),
				*SGMLogActorState(this, GetOwner()), *GetNameSafe(InMontage),
				CurrentServerMontagePosition, DuplicatePredictedPlayRejectWindowSeconds, RepMontageState.Serial);
			return;
		}
	}

	StartReplicatedMontage(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
}

void USGM_MontageComponent::ServerDisableRootMotionForReplicatedMontage_Implementation()
{
	DisableRootMotionForReplicatedMontage();
}

void USGM_MontageComponent::ServerSetCanBlendUpperAndLowerBody_Implementation(bool bInCanBlend)
{
	bCanBlendUpperAndLowerBody = bInCanBlend;
}

void USGM_MontageComponent::ServerSetReplicatedRootMotionScale_Implementation(float InRootMotionScale)
{
	SetReplicatedRootMotionScale(InRootMotionScale);
}

void USGM_MontageComponent::StopReplicatedMontage()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	StopMontageLocal(RepMontageState.Montage);
	RepMontageState.bIsPlaying = false;
	RepMontageState.Serial++;
}

void USGM_MontageComponent::OnRep_RepMontageState()
{
	ResolveMeshComponent();
	if (!MontageMeshComponent) return;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance) return;

	const bool bHasNewMontageCommand = LastAppliedMontageSerial != RepMontageState.Serial;
	UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG OnRep_RepMontageState ENTER %s Montage=%s Serial=%d LastSerial=%d NewCmd=%d IsPlaying=%d Disabled=%d Scale=%.3f DisableSerial=%d LastDisable=%d ScaleSerial=%d LastScale=%d"),
		*SGMLogActorState(this, GetOwner()), *GetNameSafe(RepMontageState.Montage),
		RepMontageState.Serial, LastAppliedMontageSerial, bHasNewMontageCommand,
		RepMontageState.bIsPlaying, RepMontageState.bRootMotionDisabled, RepMontageState.RootMotionScale,
		RepMontageState.DisableRootMotionSerial, LastAppliedDisableRootMotionSerial,
		RepMontageState.RootMotionScaleSerial, LastAppliedRootMotionScaleSerial);

	if (bHasNewMontageCommand)
	{
		LastAppliedMontageSerial = RepMontageState.Serial;

		if (!RepMontageState.bIsPlaying)
		{
			if (RepMontageState.Montage && AnimInstance->Montage_IsPlaying(RepMontageState.Montage))
			{
				AnimInstance->Montage_Stop(RepMontageState.Montage->GetDefaultBlendOutTime(), RepMontageState.Montage);
			}

			ResetLocalRootMotionControlState();
			SetComponentTickEnabled(false);
			return;
		}

		if (RepMontageState.Montage)
		{
			const AActor* OwnerActor = GetOwner();
			const bool bIsAutonomousProxy = OwnerActor && OwnerActor->GetLocalRole() == ROLE_AutonomousProxy;
			const bool bAlreadyPlayingSameMontage = AnimInstance->Montage_IsPlaying(RepMontageState.Montage);

			UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG OnRep PLAY_CMD %s Montage=%s Autonomous=%d AlreadyPlaying=%d AnimPos=%.3f RepStart=%.3f"),
				*SGMLogActorState(this, OwnerActor), *GetNameSafe(RepMontageState.Montage),
				bIsAutonomousProxy, bAlreadyPlayingSameMontage,
				bAlreadyPlayingSameMontage ? AnimInstance->Montage_GetPosition(RepMontageState.Montage) : -1.0f,
				RepMontageState.StartTimeSeconds);

			if (!(bIsAutonomousProxy && bAlreadyPlayingSameMontage))
			{
				PlayMontageLocal(RepMontageState.Montage, RepMontageState.PlayRate,
					RepMontageState.StartTimeSeconds, RepMontageState.StartSection);
			}

			if (ShouldDrivePredictedRootMotionControl())
			{
				BindContactBlockingEvents();
				RefreshInitialContactBlockState();
			}

			SetComponentTickEnabled(true);
		}
	}

	const bool bHasNewRootMotionScaleCommand = LastAppliedRootMotionScaleSerial != RepMontageState.RootMotionScaleSerial;
	if (bHasNewRootMotionScaleCommand)
	{
		LastAppliedRootMotionScaleSerial = RepMontageState.RootMotionScaleSerial;

		if (!RepMontageState.bRootMotionDisabled && RepMontageState.Montage)
		{
			ApplyRootMotionScaleToCurrentMontage(RepMontageState.RootMotionScale);
		}
	}

	const bool bHasNewDisableRootMotionCommand = LastAppliedDisableRootMotionSerial != RepMontageState.DisableRootMotionSerial;
	if (bHasNewDisableRootMotionCommand)
	{
		LastAppliedDisableRootMotionSerial = RepMontageState.DisableRootMotionSerial;

		if (RepMontageState.bRootMotionDisabled && RepMontageState.Montage)
		{
			DisableRootMotionForMontage(RepMontageState.Montage);
		}
	}
}

void USGM_MontageComponent::OnRep_CanBlendUpperAndLowerBody()
{
}

void USGM_MontageComponent::ResolveMeshComponent()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	MontageMeshComponent = Cast<USkeletalMeshComponent>(MontageMeshComponentReference.GetComponent(OwnerActor));

	if (!MontageMeshComponent)
	{
		MontageMeshComponent = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	}
}

UMoverComponent* USGM_MontageComponent::GetMoverComponent() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->FindComponentByClass<UMoverComponent>() : nullptr;
}

UCapsuleComponent* USGM_MontageComponent::GetOwnerCapsuleComponent() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->FindComponentByClass<UCapsuleComponent>() : nullptr;
}

void USGM_MontageComponent::QueueRootMotionMove(UAnimMontage* InMontage, float InPlayRate,
	float InStartingMontagePosition, float InRootMotionScale)
{
	UMoverComponent* MoverComponent = GetMoverComponent();
	if (!MoverComponent || !InMontage || InPlayRate == 0.0f) return;

	UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG QueueRootMotionMove ENTER %s Montage=%s StartPos=%.3f PlayRate=%.3f Scale=%.3f CancelExisting=1"),
		*SGMLogActorState(this, OwnerActor), *GetNameSafe(InMontage), InStartingMontagePosition, InPlayRate, InRootMotionScale);

	MoverComponent->CancelFeaturesWithTag(TAG_SyncGasMover_RootMotion, true);

	TSharedPtr<FSGM_AnimRootMotionLayeredMove> AnimRootMotionMove = MakeShared<FSGM_AnimRootMotionLayeredMove>();
	AnimRootMotionMove->MontageState.Montage = InMontage;
	AnimRootMotionMove->MontageState.PlayRate = InPlayRate;
	AnimRootMotionMove->MontageState.StartingMontagePosition = InStartingMontagePosition;
	AnimRootMotionMove->MontageState.CurrentPosition = InStartingMontagePosition;
	AnimRootMotionMove->RootMotionScale = InRootMotionScale;
	AnimRootMotionMove->bStopRootMotionOnPawnContact = bEnableRootMotionContactBlocking
		&& ShouldDrivePredictedRootMotionControl()
		&& InRootMotionScale > UE_KINDA_SMALL_NUMBER;
	AnimRootMotionMove->PawnContactBlockHalfAngleDegrees = ContactBlockHalfAngleDegrees;
	AnimRootMotionMove->MixMode = FMath::IsNearlyZero(InRootMotionScale)
		? EMoveMixMode::AdditiveVelocity
		: EMoveMixMode::OverrideAll;
	AnimRootMotionMove->bIgnoreRetriggerCancellationWhileQueued = true;

	const float RemainingUnscaledMontageSeconds = InPlayRate > 0.0f
		? InMontage->GetPlayLength() - InStartingMontagePosition
		: InStartingMontagePosition;

	AnimRootMotionMove->DurationMs = (RemainingUnscaledMontageSeconds / FMath::Abs(InPlayRate)) * 1000.0f;
	MoverComponent->QueueLayeredMove(AnimRootMotionMove);

	UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG QueueRootMotionMove QUEUED %s Montage=%s DurationMs=%.3f StopOnContact=%d HalfAngle=%.3f MixMode=%d"),
		*SGMLogActorState(this, OwnerActor), *GetNameSafe(InMontage), AnimRootMotionMove->DurationMs,
		AnimRootMotionMove->bStopRootMotionOnPawnContact, AnimRootMotionMove->PawnContactBlockHalfAngleDegrees,
		static_cast<int32>(AnimRootMotionMove->MixMode));
}

void USGM_MontageComponent::UpdateRootMotionControl(float DeltaSeconds)
{
	if (!RepMontageState.Montage || !RepMontageState.bIsPlaying)
	{
		ResetLocalRootMotionControlState();
		SetComponentTickEnabled(false);
		return;
	}

	ResolveMeshComponent();

	UAnimInstance* AnimInstance = MontageMeshComponent ? MontageMeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(RepMontageState.Montage))
	{
		const float LastPosition = AnimInstance && RepMontageState.Montage
			? AnimInstance->Montage_GetPosition(RepMontageState.Montage)
			: -1.0f;

		UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG UpdateRootMotionControl MONTAGE_ENDED %s Montage=%s LastPos=%.3f StatePlaying=%d Disabled=%d Scale=%.3f Blend=%d Released=%d"),
			*SGMLogActorState(this, GetOwner()), *GetNameSafe(RepMontageState.Montage), LastPosition,
			RepMontageState.bIsPlaying, RepMontageState.bRootMotionDisabled, RepMontageState.RootMotionScale,
			bCanBlendUpperAndLowerBody, bRootMotionReleasedByPercent);

		// Natural montage end must clear the tracked playing state too.
		// Otherwise ShouldBlockMovementInputDuringRootMotion() keeps blocking after the animation is gone.
		RepMontageState.bIsPlaying = false;
		if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
		{
			RepMontageState.Serial++;
		}

		ResetLocalRootMotionControlState();
		SetCanBlendUpperAndLowerBody(false);
		SetComponentTickEnabled(false);
		return;
	}

	if (ShouldDrivePredictedRootMotionControl())
	{
		UpdateMontagePercentRelease();
		UpdateBlockedContactResume();
	}
}

void USGM_MontageComponent::UpdateMontagePercentRelease()
{
	if (!ShouldDrivePredictedRootMotionControl() || RootMotionReleasePercent < 0.0f || bRootMotionReleasedByPercent || RepMontageState.bRootMotionDisabled)
	{
		return;
	}

	ResolveMeshComponent();

	UAnimInstance* AnimInstance = MontageMeshComponent ? MontageMeshComponent->GetAnimInstance() : nullptr;
	FAnimMontageInstance* MontageInstance = AnimInstance && RepMontageState.Montage
		? AnimInstance->GetActiveInstanceForMontage(RepMontageState.Montage)
		: nullptr;

	if (!MontageInstance || !RepMontageState.Montage) return;

	const float MontageLength = FMath::Max(RepMontageState.Montage->GetPlayLength(), KINDA_SMALL_NUMBER);
	const float MontageProgress = MontageInstance->GetPosition() / MontageLength;

	if (MontageProgress < RootMotionReleasePercent) return;

	// The release percent opens the movement-cancel window only.
	// Root motion keeps playing until movement input calls TryReleaseRootMotionForMovementInput().
	bRootMotionReleasedByPercent = true;
	ContactBlockingActors.Reset();
	SetContactRootMotionBlocked(false);
}

void USGM_MontageComponent::UpdateBlockedContactResume()
{
	if (!ShouldDrivePredictedRootMotionControl() || !bRootMotionBlockedByContact) return;

	const AActor* OwnerActor = GetOwner();
	const UCapsuleComponent* OwnerCapsule = GetOwnerCapsuleComponent();
	if (!OwnerActor || !OwnerCapsule)
	{
		ContactBlockingActors.Reset();
		SetContactRootMotionBlocked(false);
		return;
	}

	const float OwnerRadius = OwnerCapsule->GetScaledCapsuleRadius();
	const FVector OwnerLocation = OwnerActor->GetActorLocation();

	for (auto It = ContactBlockingActors.CreateIterator(); It; ++It)
	{
		const TWeakObjectPtr<AActor> WeakActor = *It;
		const AActor* BlockingActor = WeakActor.Get();
		if (!BlockingActor || !IsActorWithinContactBlockAngle(BlockingActor))
		{
			It.RemoveCurrent();
			continue;
		}

		const UCapsuleComponent* OtherCapsule = BlockingActor->FindComponentByClass<UCapsuleComponent>();
		const float OtherRadius = OtherCapsule ? OtherCapsule->GetScaledCapsuleRadius() : OwnerRadius;
		const float MaxContactDistance = OwnerRadius + OtherRadius + ContactStillBlockingSlack;

		FVector ToOther = BlockingActor->GetActorLocation() - OwnerLocation;
		ToOther.Z = 0.0f;

		if (ToOther.SizeSquared() > FMath::Square(MaxContactDistance))
		{
			It.RemoveCurrent();
		}
	}

	SetContactRootMotionBlocked(ContactBlockingActors.Num() > 0);
}

bool USGM_MontageComponent::ShouldDrivePredictedRootMotionControl() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && (OwnerActor->HasAuthority() || OwnerActor->GetLocalRole() == ROLE_AutonomousProxy);
}

bool USGM_MontageComponent::IsActorWithinContactBlockAngle(const AActor* OtherActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OtherActor || OtherActor == OwnerActor) return false;

	FVector ToOther = OtherActor->GetActorLocation() - OwnerActor->GetActorLocation();
	ToOther.Z = 0.0f;

	if (ToOther.IsNearlyZero()) return true;

	const FVector OwnerForward = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
	const float MinForwardDot = FMath::Cos(FMath::DegreesToRadians(ContactBlockHalfAngleDegrees));
	return FVector::DotProduct(OwnerForward, ToOther.GetSafeNormal()) >= MinForwardDot;
}

void USGM_MontageComponent::RefreshInitialContactBlockState()
{
	if (!ShouldDrivePredictedRootMotionControl() || !bEnableRootMotionContactBlocking || bRootMotionReleasedByPercent || RepMontageState.bRootMotionDisabled)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	UCapsuleComponent* CapsuleComponent = GetOwnerCapsuleComponent();
	UWorld* World = GetWorld();
	if (!OwnerActor || !CapsuleComponent || !World) return;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SGMRootMotionInitialContactBlock), false, OwnerActor);

	const FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(
		CapsuleComponent->GetScaledCapsuleRadius() + ContactInitialProbeInflation,
		CapsuleComponent->GetScaledCapsuleHalfHeight() + ContactInitialProbeInflation);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		OwnerActor->GetActorLocation(),
		OwnerActor->GetActorQuat(),
		ObjectQueryParams,
		CollisionShape,
		QueryParams);

	ContactBlockingActors.Reset();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlappingActor = Overlap.GetActor();
		if (OverlappingActor && IsActorWithinContactBlockAngle(OverlappingActor))
		{
			ContactBlockingActors.Add(OverlappingActor);
		}
	}

	SetContactRootMotionBlocked(ContactBlockingActors.Num() > 0);
}

void USGM_MontageComponent::SetContactRootMotionBlocked(bool bInBlocked)
{
	if (!ShouldDrivePredictedRootMotionControl()) return;

	if (bRootMotionReleasedByPercent || RepMontageState.bRootMotionDisabled)
	{
		bInBlocked = false;
	}

	if (bRootMotionBlockedByContact == bInBlocked) return;

	bRootMotionBlockedByContact = bInBlocked;
	SetReplicatedRootMotionScale(bRootMotionBlockedByContact ? 0.0f : 1.0f);
	SetComponentTickEnabled(RootMotionReleasePercent >= 0.0f || RepMontageState.bIsPlaying || bRootMotionBlockedByContact);
}

void USGM_MontageComponent::BindContactBlockingEvents()
{
	if (!bEnableRootMotionContactBlocking || !ShouldDrivePredictedRootMotionControl()) return;

	UCapsuleComponent* CapsuleComponent = GetOwnerCapsuleComponent();
	if (!CapsuleComponent) return;

	if (BoundContactCapsule && BoundContactCapsule != CapsuleComponent)
	{
		UnbindContactBlockingEvents();
	}

	BoundContactCapsule = CapsuleComponent;
	BoundContactCapsule->SetNotifyRigidBodyCollision(true);
	BoundContactCapsule->OnComponentHit.AddUniqueDynamic(this, &USGM_MontageComponent::OnOwnerCapsuleHit);
	BoundContactCapsule->OnComponentBeginOverlap.AddUniqueDynamic(this, &USGM_MontageComponent::OnOwnerCapsuleBeginOverlap);
	BoundContactCapsule->OnComponentEndOverlap.AddUniqueDynamic(this, &USGM_MontageComponent::OnOwnerCapsuleEndOverlap);
}

void USGM_MontageComponent::UnbindContactBlockingEvents()
{
	if (!BoundContactCapsule) return;

	BoundContactCapsule->OnComponentHit.RemoveDynamic(this, &USGM_MontageComponent::OnOwnerCapsuleHit);
	BoundContactCapsule->OnComponentBeginOverlap.RemoveDynamic(this, &USGM_MontageComponent::OnOwnerCapsuleBeginOverlap);
	BoundContactCapsule->OnComponentEndOverlap.RemoveDynamic(this, &USGM_MontageComponent::OnOwnerCapsuleEndOverlap);
	BoundContactCapsule = nullptr;
}

void USGM_MontageComponent::OnOwnerCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!ShouldDrivePredictedRootMotionControl() || !bEnableRootMotionContactBlocking || bRootMotionReleasedByPercent || RepMontageState.bRootMotionDisabled)
	{
		return;
	}

	if (IsActorWithinContactBlockAngle(OtherActor))
	{
		ContactBlockingActors.Add(OtherActor);
		SetContactRootMotionBlocked(true);
	}
}

void USGM_MontageComponent::OnOwnerCapsuleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!ShouldDrivePredictedRootMotionControl()) return;

	ContactBlockingActors.Remove(OtherActor);
	SetContactRootMotionBlocked(ContactBlockingActors.Num() > 0);
}

void USGM_MontageComponent::OnOwnerCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!ShouldDrivePredictedRootMotionControl() || !bEnableRootMotionContactBlocking || bRootMotionReleasedByPercent || RepMontageState.bRootMotionDisabled)
	{
		return;
	}

	if (OtherActor && OtherActor != GetOwner() && OtherActor->IsA<APawn>() && IsActorWithinContactBlockAngle(OtherActor))
	{
		ContactBlockingActors.Add(OtherActor);
		SetContactRootMotionBlocked(true);
	}
}

bool USGM_MontageComponent::ApplyRootMotionScaleToCurrentMontage(float InRootMotionScale)
{
	ResolveMeshComponent();

	UAnimInstance* AnimInstance = MontageMeshComponent ? MontageMeshComponent->GetAnimInstance() : nullptr;
	FAnimMontageInstance* MontageInstance = AnimInstance && RepMontageState.Montage
		? AnimInstance->GetActiveInstanceForMontage(RepMontageState.Montage)
		: nullptr;

	if (!AnimInstance || !MontageInstance || !RepMontageState.Montage) return false;

	MontageInstance->PushDisableRootMotion();
	QueueRootMotionMove(
		RepMontageState.Montage,
		AnimInstance->Montage_GetPlayRate(RepMontageState.Montage),
		MontageInstance->GetPosition(),
		FMath::Clamp(InRootMotionScale, 0.0f, 1.0f));

	return true;
}

void USGM_MontageComponent::SetReplicatedRootMotionScale(float InRootMotionScale)
{
	const float ClampedRootMotionScale = FMath::Clamp(InRootMotionScale, 0.0f, 1.0f);
	ApplyRootMotionScaleToCurrentMontage(ClampedRootMotionScale);

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	if (!OwnerActor->HasAuthority())
	{
		if (OwnerActor->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetReplicatedRootMotionScale(ClampedRootMotionScale);
		}
		return;
	}

	if (FMath::IsNearlyEqual(RepMontageState.RootMotionScale, ClampedRootMotionScale)) return;

	RepMontageState.RootMotionScale = ClampedRootMotionScale;
	RepMontageState.RootMotionScaleSerial++;
}

void USGM_MontageComponent::ResetLocalRootMotionControlState()
{
	ContactBlockingActors.Reset();

	if (ShouldDrivePredictedRootMotionControl())
	{
		SetContactRootMotionBlocked(false);
	}
	else
	{
		bRootMotionBlockedByContact = false;
	}

	UnbindContactBlockingEvents();
	bRootMotionReleasedByPercent = false;
	RootMotionReleasePercent = -1.0f;
}
