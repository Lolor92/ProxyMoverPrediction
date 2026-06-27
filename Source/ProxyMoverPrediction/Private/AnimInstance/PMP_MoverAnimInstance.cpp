#include "ProxyMoverPrediction/Public/AnimInstance/PMP_MoverAnimInstance.h"
#include "Components/SGM_MontageComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/Pawn.h"

void UPMP_MoverAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	// TryGetPawnOwner is the normal AnimInstance way to find the pawn using this mesh.
	PawnOwner = TryGetPawnOwner();

	if (!PawnOwner) return;

	// We read movement from Mover, not CharacterMovementComponent.
	MoverComponent = PawnOwner->FindComponentByClass<UCharacterMoverComponent>();
	MontageComponent = USGM_MontageComponent::FindMontageComponentFromAnimInstance(this);
}

void UPMP_MoverAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (!PawnOwner)
	{
		PawnOwner = TryGetPawnOwner();
	}

	if (PawnOwner && !MoverComponent)
	{
		MoverComponent = PawnOwner->FindComponentByClass<UCharacterMoverComponent>();
	}

	if (!MontageComponent)
	{
		MontageComponent = USGM_MontageComponent::FindMontageComponentFromAnimInstance(this);
	}

	const bool bNewCanBlendUpperAndLowerBody = MontageComponent
		&& MontageComponent->GetCanBlendUpperAndLowerBody();

	if (bCanBlendUpperAndLowerBody != bNewCanBlendUpperAndLowerBody)
	{
		bCanBlendUpperAndLowerBody = bNewCanBlendUpperAndLowerBody;
	}

	if (!PawnOwner || !MoverComponent) return;

	const FVector Velocity = MoverComponent->GetVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);

	GroundSpeed = HorizontalVelocity.Size();
	bIsMoving = GroundSpeed > 3.0f;

	if (bIsMoving)
	{
		MovementRotation = HorizontalVelocity.GetSafeNormal().Rotation();

		// Actor yaw is where the body faces. Movement yaw is where it travels.
		// The difference tells the AnimBP forward/left/right/back movement.
		MovementOffsetYaw = FMath::FindDeltaAngleDegrees(
			PawnOwner->GetActorRotation().Yaw,
			MovementRotation.Yaw);
	}
	else
	{
		MovementOffsetYaw = 0.0f;
	}
}
