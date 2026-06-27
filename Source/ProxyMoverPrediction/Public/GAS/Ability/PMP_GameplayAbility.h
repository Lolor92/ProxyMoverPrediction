#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "PMP_GameplayAbility.generated.h"

UCLASS()
class PROXYMOVERPREDICTION_API UPMP_GameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPMP_GameplayAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
};
