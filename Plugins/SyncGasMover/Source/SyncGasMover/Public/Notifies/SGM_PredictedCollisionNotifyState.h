#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "SGM_PredictedCollisionNotifyState.generated.h"

class AActor;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class ESGM_PredictedCollisionShape : uint8
{
	Sphere,
	Box,
	Capsule
};

// Runtime data for one active notify window.
// We track already-hit targets so one sword swing does not hit the same actor every tick.
struct FSGM_PredictedCollisionRuntimeWindow
{
	FName NotifyWindowId = NAME_None;
	TSet<TWeakObjectPtr<AActor>> ProcessedTargets;
};

UCLASS(DisplayName = "PredictedCollisionNotify")
class SYNCGASMOVER_API USGM_PredictedCollisionNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Predicted Reaction")
	bool bPlayPredictedReactionOnClient = true;

	// This is only an id for the reaction. A component/data asset can resolve it into montage/effects later.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Predicted Reaction",
		meta = (EditCondition = "bPlayPredictedReactionOnClient"))
	FGameplayTag PredictedReactionTag;

	// Stable id for this specific notify window inside a montage.
	// Example: ShieldBash_Hit_01, ShieldBash_Hit_02.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Prediction")
	FName NotifyWindowId = NAME_None;

	// Socket to sweep from. For a weapon attack this should usually be a weapon or hand socket.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Socket")
	FName SourceSocketName = TEXT("MainHandWeaponSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Shape")
	ESGM_PredictedCollisionShape CollisionShape = ESGM_PredictedCollisionShape::Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Shape",
		meta = (EditCondition = "CollisionShape==ESGM_PredictedCollisionShape::Sphere", EditConditionHides, ClampMin = "1.0"))
	float SphereRadius = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Shape",
		meta = (EditCondition = "CollisionShape==ESGM_PredictedCollisionShape::Box", EditConditionHides))
	FVector BoxExtent = FVector(20.0f, 45.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Shape",
		meta = (EditCondition = "CollisionShape==ESGM_PredictedCollisionShape::Capsule", EditConditionHides, ClampMin = "1.0"))
	float CapsuleRadius = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Shape",
		meta = (EditCondition = "CollisionShape==ESGM_PredictedCollisionShape::Capsule", EditConditionHides, ClampMin = "1.0"))
	float CapsuleHalfHeight = 45.0f;

	// Offset from the source socket. This lets one notify support different weapon sizes/positions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Shape")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Shape")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	// Long frame steps can skip thin targets, so we split the sweep into smaller pieces.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Collision", meta = (ClampMin = "1.0"))
	float MaxSweepStepDistance = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Collision")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Collision")
	bool bOnlyHitPawns = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Debug")
	bool bDrawDebug = false;

	virtual void HandlePredictedCollisionHit(AActor* OwningActor, AActor* HitActor, const FHitResult& HitResult,
		FName InNotifyWindowId);

	// This fires on the server and on the locally controlled attacking client.
	// Later we can replace or extend this with predicted reaction/correction code.
	UFUNCTION(BlueprintImplementableEvent, Category = "SyncGasMover|Collision", meta = (DisplayName = "On Predicted Collision Hit"))
	void OnPredictedCollisionHit(AActor* OwningActor, AActor* HitActor, const FHitResult& HitResult,
		FGameplayTag ReactionTag, FName NotifyWindowId) const;

private:
	bool ShouldRunCollision(const AActor* OwnerActor) const;
	bool BuildTraceTransform(const USkeletalMeshComponent* MeshComp, FTransform& OutTransform) const;
	FCollisionShape MakeCollisionShape() const;

	void SweepCollision(USkeletalMeshComponent* MeshComp, const FTransform& PreviousTransform,
		const FTransform& CurrentTransform);

	void DrawDebugSweep(UWorld* World, const FVector& StepStart, const FVector& StepEnd,
		const FQuat& Rotation, const FColor& Color) const;

	bool HasAlreadyProcessedTarget(const USkeletalMeshComponent* MeshComp, const AActor* TargetActor) const;
	void MarkTargetProcessed(USkeletalMeshComponent* MeshComp, AActor* TargetActor);
	
	// Notify state objects are shared, so we key runtime state by mesh.
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FTransform> PreviousTransforms;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FSGM_PredictedCollisionRuntimeWindow> ActiveWindowsByMesh;
};
