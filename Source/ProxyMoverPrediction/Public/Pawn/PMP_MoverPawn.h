#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"
#include "PMP_MoverPawn.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UCharacterMoverComponent;

UCLASS()
class PROXYMOVERPREDICTION_API APMP_MoverPawn : public APawn, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	APMP_MoverPawn();
	
	UCharacterMoverComponent* GetMoverComponent() const { return MoverComponent; }
	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }
	USkeletalMeshComponent* GetMeshComponent() const { return MeshComponent; }
	
	// Input code calls this later. We cache intent because Mover consumes input during its simulation tick.
	UFUNCTION(BlueprintCallable, Category = "Mover")
	void RequestMoveIntent(const FVector& MoveIntent);

	// Useful when input stops, pawn is stunned, or ability code wants movement cleared.
	UFUNCTION(BlueprintCallable, Category = "Mover")
	void ClearMoveIntent();
	
protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	
	// Mover calls this during its prediction simulation to collect this frame's movement input.
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	
	// Mover replaces CharacterMovementComponent. It will own simulation and network prediction.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mover")
	TObjectPtr<UCharacterMoverComponent> MoverComponent = nullptr;

	// The capsule is the actual collision body Mover will move.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> CapsuleComponent = nullptr;
	
	// The mesh is only visual. It should not block movement or attacks.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;
	
private:
	void ApplyDefaultMovementSettings();
	
	// Local desired movement direction.
	FVector CachedMoveInputIntent = FVector::ZeroVector;
};
