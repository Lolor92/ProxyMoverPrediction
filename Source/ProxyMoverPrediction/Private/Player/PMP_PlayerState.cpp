#include "Player/PMP_PlayerState.h"

#include "GAS/ASC/PMP_AbilitySystemComponent.h"

APMP_PlayerState::APMP_PlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UPMP_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// PlayerStates replicate less often by default. GAS benefits from a higher update rate.
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* APMP_PlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
