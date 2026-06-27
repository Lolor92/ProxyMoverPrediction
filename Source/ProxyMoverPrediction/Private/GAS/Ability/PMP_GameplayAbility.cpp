#include "GAS/Ability/PMP_GameplayAbility.h"

UPMP_GameplayAbility::UPMP_GameplayAbility()
{
	// This is the base setup we want for combat abilities:
	// the owning client can start them immediately, then the server confirms.
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;

	// For local prediction, we generally do not want remote cancellation to kill
	// the server ability unless we deliberately build that behavior later.
	bServerRespectsRemoteAbilityCancellation = false;

	// Allows repeated inputs to restart/retrigger an instanced ability.
	bRetriggerInstancedAbility = true;
}

void UPMP_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UPMP_GameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
