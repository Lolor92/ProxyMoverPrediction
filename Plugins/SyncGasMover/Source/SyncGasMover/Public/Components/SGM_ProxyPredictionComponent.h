#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SGM_ReactionData.h"
#include "SGM_ProxyPredictionComponent.generated.h"

class AActor;
class UAnimMontage;
class USkeletalMeshComponent;

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
};
