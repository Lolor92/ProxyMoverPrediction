#include "AbilityTasks/SGM_PlayMoverMontageFromAbilityTask.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SGM_MontageComponent.h"

USGM_PlayMoverMontageFromAbilityTask* USGM_PlayMoverMontageFromAbilityTask::PlaySGMMoverMontageAndWait(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	UAnimMontage* Montage,
	float PlayRate,
	float StartTimeSeconds,
	FName StartSection,
	float RootMotionContactBlockHalfAngleDegrees,
	float RootMotionReleasePercent)
{
	USGM_PlayMoverMontageFromAbilityTask* Task =
		NewAbilityTask<USGM_PlayMoverMontageFromAbilityTask>(OwningAbility, TaskInstanceName);

	Task->MontageToPlay = Montage;
	Task->PlayRateToUse = PlayRate;
	Task->StartTimeSecondsToUse = StartTimeSeconds;
	Task->StartSectionToUse = StartSection;
	Task->ContactBlockHalfAngleDegrees = RootMotionContactBlockHalfAngleDegrees;
	Task->ReleasePercent = RootMotionReleasePercent;
	return Task;
}

void USGM_PlayMoverMontageFromAbilityTask::Activate()
{
	Super::Activate();

	MontageComponent = FindMontageComponent();
	if (!MontageComponent || !MontageToPlay)
	{
		BroadcastCancelledAndEnd();
		return;
	}

	// Configure root-motion control before playing the montage.
	// PlayMontageLocal queues the Mover layered move immediately, so these settings must already
	// be in place or the task will queue once, then requeue after contact/release setup.
	MontageComponent->SetRootMotionContactBlockingAngleDegrees(ContactBlockHalfAngleDegrees);

	if (ReleasePercent >= 0.0f)
	{
		MontageComponent->StartRootMotionReleaseAtMontagePercent(ReleasePercent);
	}
	else
	{
		MontageComponent->ClearRootMotionRelease();
	}

	const bool bStarted = MontageComponent->PlayPredictedReplicatedMontage(
		MontageToPlay,
		PlayRateToUse,
		StartTimeSecondsToUse,
		StartSectionToUse);

	if (!bStarted)
	{
		BroadcastCancelledAndEnd();
		return;
	}

	MeshComponent = MontageComponent->GetResolvedMontageMeshComponent();
	AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		BroadcastCancelledAndEnd();
		return;
	}

	FOnMontageBlendingOutStarted BlendOutDelegate;
	BlendOutDelegate.BindUObject(this, &USGM_PlayMoverMontageFromAbilityTask::OnMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, MontageToPlay);

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &USGM_PlayMoverMontageFromAbilityTask::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);

	bPlayedSuccessfully = true;
}

void USGM_PlayMoverMontageFromAbilityTask::ExternalCancel()
{
	StopPlayingMontage();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast();
	}

	Super::ExternalCancel();
}

void USGM_PlayMoverMontageFromAbilityTask::OnDestroy(bool bInOwnerFinished)
{
	ClearMontageDelegates();
	Super::OnDestroy(bInOwnerFinished);
}

void USGM_PlayMoverMontageFromAbilityTask::OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted)
{
	if (InMontage != MontageToPlay || !ShouldBroadcastAbilityTaskDelegates())
	{
		return;
	}

	if (bInterrupted)
	{
		OnInterrupted.Broadcast();
		return;
	}

	OnBlendOut.Broadcast();
}

void USGM_PlayMoverMontageFromAbilityTask::OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (InMontage != MontageToPlay)
	{
		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates() && !bInterrupted)
	{
		OnCompleted.Broadcast();
	}

	EndTask();
}

bool USGM_PlayMoverMontageFromAbilityTask::StopPlayingMontage()
{
	if (!MontageComponent || !MontageToPlay)
	{
		return false;
	}

	return MontageComponent->StopMontageLocal(MontageToPlay);
}

void USGM_PlayMoverMontageFromAbilityTask::ClearMontageDelegates()
{
	if (!AnimInstance || !MontageToPlay)
	{
		return;
	}

	FOnMontageBlendingOutStarted EmptyBlendOutDelegate;
	AnimInstance->Montage_SetBlendingOutDelegate(EmptyBlendOutDelegate, MontageToPlay);

	FOnMontageEnded EmptyEndDelegate;
	AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, MontageToPlay);
}

void USGM_PlayMoverMontageFromAbilityTask::BroadcastCancelledAndEnd()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast();
	}

	EndTask();
}

USGM_MontageComponent* USGM_PlayMoverMontageFromAbilityTask::FindMontageComponent() const
{
	const UGameplayAbility* GameplayAbility = Ability.Get();
	if (!GameplayAbility)
	{
		return nullptr;
	}

	AActor* AvatarActor = GameplayAbility->GetAvatarActorFromActorInfo();
	return AvatarActor ? AvatarActor->FindComponentByClass<USGM_MontageComponent>() : nullptr;
}
