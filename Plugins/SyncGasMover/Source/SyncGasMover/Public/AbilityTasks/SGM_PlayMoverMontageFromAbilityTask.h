#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "SGM_PlayMoverMontageFromAbilityTask.generated.h"

class UAnimMontage;
class USGM_MontageComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSGMPlayMoverMontageTaskDelegate);

UCLASS()
class SYNCGASMOVER_API USGM_PlayMoverMontageFromAbilityTask : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FSGMPlayMoverMontageTaskDelegate OnStarted;

	UPROPERTY(BlueprintAssignable)
	FSGMPlayMoverMontageTaskDelegate OnFailed;

	// Plays a predicted replicated SGM/Mover montage from a Gameplay Ability and optionally enables
	// contact blocking plus root-motion release in one clean Blueprint node.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Ability", meta = (DisplayName = "Play SGM Mover Montage From Ability", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static USGM_PlayMoverMontageFromAbilityTask* PlaySGMMoverMontageFromAbility(
		UGameplayAbility* OwningAbility,
		UAnimMontage* Montage,
		float PlayRate = 1.0f,
		float StartTimeSeconds = 0.0f,
		FName StartSection = NAME_None,
		bool bEnableRootMotionContactBlocking = false,
		float RootMotionReleasePercent = -1.0f);

	virtual void Activate() override;

private:
	UPROPERTY()
	TObjectPtr<UAnimMontage> MontageToPlay = nullptr;

	float PlayRateToUse = 1.0f;
	float StartTimeSecondsToUse = 0.0f;
	FName StartSectionToUse = NAME_None;
	bool bEnableContactBlocking = false;
	float ReleasePercent = -1.0f;

	USGM_MontageComponent* FindMontageComponent() const;
};
