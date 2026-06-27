#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PMP_MoverAnimInstance.generated.h"

class UCharacterMoverComponent;
class USGM_MontageComponent;

UCLASS()
class PROXYMOVERPREDICTION_API UPMP_MoverAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	
protected:
	// 2D speed from Mover velocity. Use this for idle/run blendspaces.
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	float GroundSpeed = 0.0f;

	// Simple bool for state machines or cached poses.
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	bool bIsMoving = false;

	// Direction the pawn is actually moving in world space.
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	FRotator MovementRotation = FRotator::ZeroRotator;

	// Difference between actor facing and movement direction.
	// Useful for strafing blendspaces.
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	float MovementOffsetYaw = 0.0f;
	
	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	bool bIsInAir = false;
	
	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	bool bIsOnGround = false;

	// True when root motion has been released and the AnimBP should layer upper/lower body.
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Root Motion", meta=(AllowPrivateAccess="true"))
	bool bCanBlendUpperAndLowerBody = false;
	
	// The pawn that owns this anim instance.
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Owner")
	TObjectPtr<APawn> PawnOwner = nullptr;
	
	// Mover component we read velocity from.
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	TObjectPtr<UCharacterMoverComponent> MoverComponent = nullptr;

	// Montage component we read root-motion animation state from.
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Root Motion")
	TObjectPtr<USGM_MontageComponent> MontageComponent = nullptr;
};
