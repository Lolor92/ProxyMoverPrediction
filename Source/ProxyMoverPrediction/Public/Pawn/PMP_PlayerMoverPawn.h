#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "PMP_MoverPawn.h"
#include "PMP_PlayerMoverPawn.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class PROXYMOVERPREDICTION_API APMP_PlayerMoverPawn : public APMP_MoverPawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APMP_PlayerMoverPawn();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Assign your Input Mapping Context asset in the Blueprint child.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMappingContext = nullptr;

	// Expected value type: Axis2D. X = right/left, Y = forward/back.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction = nullptr;

	// Expected value type: Axis2D. X = yaw, Y = pitch.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction = nullptr;

	void HandleMoveInput(const FInputActionValue& Value);
	void HandleMoveCompleted(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);

	// Camera boom. It follows the pawn and rotates from controller input.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArmComponent = nullptr;

	// Actual view camera. It sits at the end of the spring arm.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComponent = nullptr;

private:
	void InitializeAbilitySystemFromPlayerState();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;
};
