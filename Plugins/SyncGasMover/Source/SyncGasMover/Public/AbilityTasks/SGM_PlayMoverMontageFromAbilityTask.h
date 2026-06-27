#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "SGM_PlayMoverMontageFromAbilityTask.generated.h"

class UAnimInstance;
class UAnimMontage;
class USGM_MontageComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSGMPlayMoverMontageTaskDelegate);

UCLASS()
class SYNCGASMOVER_API USGM_PlayMoverMontageFromAbilityTask : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FSGMPlayMoverMontageTaskDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FSGMPlayMoverMontageTaskDelegate OnBlendOut;

	UPROPERTY(BlueprintAssignable)
	FSGMPlayMoverMontageTaskDelegate OnInterrupted;

	UPROPERTY(BlueprintAssignable)
	FSGMPlayMoverMontageTaskDelegate OnCancelled;

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Ability", meta = (DisplayName = "Play SGM Mover Montage And Wait", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static USGM_PlayMoverMontageFromAbilityTask* PlaySGMMoverMontageAndWait(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		UAnimMontage* Montage,
		float PlayRate = 1.0f,
		float StartTimeSeconds = 0.0f,
		FName StartSection = NAME_None,
		float RootMotionContactBlockHalfAngleDegrees = 0.0f,
		float RootMotionReleasePercent = -1.0f);

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted);
	void OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted);
	bool StopPlayingMontage();
	void ClearMontageDelegates();
	void BroadcastCancelledAndEnd();
	USGM_MontageComponent* FindMontageComponent() const;

	UPROPERTY()
	TObjectPtr<USGM_MontageComponent> MontageComponent = nullptr;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimInstance> AnimInstance = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimMontage> MontageToPlay = nullptr;

	float PlayRateToUse = 1.0f;
	float StartTimeSecondsToUse = 0.0f;
	FName StartSectionToUse = NAME_None;
	float ContactBlockHalfAngleDegrees = 0.0f;
	float ReleasePercent = -1.0f;
	bool bPlayedSuccessfully = false;
};
