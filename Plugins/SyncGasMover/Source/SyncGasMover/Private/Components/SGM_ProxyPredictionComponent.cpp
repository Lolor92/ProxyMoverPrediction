#include "Components/SGM_ProxyPredictionComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SGM_MontageComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "MotionWarpingComponent.h"


USGM_ProxyPredictionComponent::USGM_ProxyPredictionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true);
}

void USGM_ProxyPredictionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool USGM_ProxyPredictionComponent::PlayPredictedReactionOnTargetProxy(AActor* TargetActor, FGameplayTag ReactionTag)
{
	if (!ReactionData || !ReactionTag.IsValid()) return false;

	FSGM_ReactionDataEntry Reaction;
	if (!ReactionData->FindReaction(ReactionTag, Reaction)) return false;
	
	return true;
}
