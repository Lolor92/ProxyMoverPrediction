#include "Components/SGM_ProxyPredictionComponent.h"

USGM_ProxyPredictionComponent::USGM_ProxyPredictionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true);
}

void USGM_ProxyPredictionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool USGM_ProxyPredictionComponent::PlayPredictedReactionOnTargetProxy(AActor* InActor, FGameplayTag ReactionTag, FName NotifyWindowId)
{
	UE_LOG(LogTemp, Warning,
		TEXT("SGM_REACTION_WINDOW LOOKUP Owner=%s Tag=%s NetMode=%d Auth=%d NotifyWindowId=%s"),
		*GetNameSafe(GetOwner()),
		*ReactionTag.ToString(),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1,
		GetOwner() ? GetOwner()->HasAuthority() : false,
		*NotifyWindowId.ToString());

	if (!InActor || !ReactionData || !ReactionTag.IsValid()) return false;

	FSGM_ReactionDataEntry Reaction;
	if (!ReactionData->FindReaction(ReactionTag, Reaction)) return false;

	return true;
}
