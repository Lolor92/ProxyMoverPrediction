#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "PMP_PlayerState.generated.h"

class UPMP_AbilitySystemComponent;

UCLASS()
class PROXYMOVERPREDICTION_API APMP_PlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APMP_PlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPMP_AbilitySystemComponent* GetPMPAbilitySystemComponent() const { return AbilitySystemComponent; }

protected:
	// PlayerState owns the ASC so abilities/effects survive pawn respawn.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability")
	TObjectPtr<UPMP_AbilitySystemComponent> AbilitySystemComponent = nullptr;
};
