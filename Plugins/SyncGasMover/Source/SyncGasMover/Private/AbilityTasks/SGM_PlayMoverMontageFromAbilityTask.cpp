#include "AbilityTasks/SGM_PlayMoverMontageFromAbilityTask.h"

#include "Abilities/GameplayAbility.h"
#include "Components/SGM_MontageComponent.h"

USGM_PlayMoverMontageFromAbilityTask* USGM_PlayMoverMontageFromAbilityTask::PlaySGMMoverMontageFromAbility(
	UGameplayAbility* OwningAbility,
	UAnimMontage* Montage,
	float PlayRate,
	float StartTimeSeconds,
	FName StartSection,
	bool bEnableRootMotionContactBlocking,
	float RootMotionReleasePercent)
{
	USGM_PlayMoverMontageFromAbilityTask* Task = NewAbilityTask<USGM_PlayMoverMontageFromAbilityTask>(OwningAbility);
	Task->MontageToPlay = Montage;
	Task->PlayRateToUse = PlayRate;
	Task->StartTimeSecondsToUse = StartTimeSeconds;
	Task->StartSectionToUse = StartSection;
	Task->bEnableContactBlocking = bEnableRootMotionContactBlocking;
	Task->ReleasePercent = RootMotionReleasePercent;
	return Task;
}

void USGM_PlayMoverMontageFromAbilityTask::Activate()
{
	USGM_MontageComponent* MontageComponent = FindMontageComponent();
	if (!MontageComponent || !MontageToPlay)
	{
		OnFailed.Broadcast();
		EndTask();
		return;
	}

	const bool bStarted = MontageComponent->PlayPredictedReplicatedMontage(
		MontageToPlay,
		PlayRateToUse,
		StartTimeSecondsToUse,
		StartSectionToUse);

	if (!bStarted)
	{
		OnFailed.Broadcast();
		EndTask();
		return;
	}

	MontageComponent->SetRootMotionContactBlockingEnabled(bEnableContactBlocking);

	if (ReleasePercent >= 0.0f)
	{
		MontageComponent->StartRootMotionReleaseAtMontagePercent(ReleasePercent);
	}
	else
	{
		MontageComponent->ClearRootMotionRelease();
	}

	OnStarted.Broadcast();
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
