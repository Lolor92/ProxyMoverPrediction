#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SGM_ReactionData.h"
#include "SGM_ProxyPredictionComponent.generated.h"

class AActor;
class UAnimMontage;
class USkeletalMeshComponent;

USTRUCT()
struct FSGM_ActivePredictedProxyRootMotion
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> TargetMesh;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> Montage = nullptr;

	float PlayRate = 1.0f;
	float PreviousPosition = 0.0f;
	FTransform InitialMeshRelativeTransform = FTransform::Identity;
	FTransform InitialMeshWorldTransform = FTransform::Identity;
	FVector AccumulatedWorldTranslation = FVector::ZeroVector;
};

UCLASS(ClassGroup = (SyncGasMover), meta = (BlueprintSpawnableComponent))
class SYNCGASMOVER_API USGM_ProxyPredictionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USGM_ProxyPredictionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Reaction")
	bool PlayPredictedReactionOnTargetProxy(AActor* TargetActor, FGameplayTag ReactionTag);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Reaction")
	TObjectPtr<USGM_ReactionData> ReactionData = nullptr;

private:
	bool CanPlayPredictedReactionOnTargetProxy(AActor* TargetActor, const FSGM_ReactionDataEntry& Reaction) const;
	bool PlayReactionMontageOnActor(AActor* TargetActor, const FSGM_ReactionDataEntry& Reaction);
	float GetReactionStartPosition(const FSGM_ReactionDataEntry& Reaction) const;
	void StartPredictedProxyRootMotion(AActor* TargetActor, const FSGM_ReactionDataEntry& Reaction,
		float StartPosition);
	void UpdatePredictedProxyRootMotion();

	UPROPERTY(Transient)
	mutable TMap<TWeakObjectPtr<AActor>, double> LastReactionTimeByTarget;

	UPROPERTY(Transient)
	TArray<FSGM_ActivePredictedProxyRootMotion> ActivePredictedProxyRootMotion;
};
