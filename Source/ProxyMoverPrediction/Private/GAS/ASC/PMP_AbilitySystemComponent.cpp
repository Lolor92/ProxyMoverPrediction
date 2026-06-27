#include "GAS/ASC/PMP_AbilitySystemComponent.h"

UPMP_AbilitySystemComponent::UPMP_AbilitySystemComponent()
{
	// PlayerState-owned ASC usually uses Mixed replication:
	// full GameplayEffect data to the owner, lighter data to simulated clients.
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}
