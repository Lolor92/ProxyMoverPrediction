#include "Components/SGM_ProxyPredictionComponent.h"

#include "Components/SGM_MontageComponent.h"
#include "GameFramework/Pawn.h"

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
	TEXT("SGM_REACTION_WINDOW LOOKUP Owner=%s Target=%s Tag=%s NetMode=%d Auth=%d NotifyWindowId=%s"),
	*GetNameSafe(GetOwner()),
	*GetNameSafe(InActor),
	*ReactionTag.ToString(),
	GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1,
	GetOwner() ? GetOwner()->HasAuthority() : false,
	*NotifyWindowId.ToString());

	if (!InActor || !ReactionData || !ReactionTag.IsValid())
	{
	return false;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
	UE_LOG(LogTemp, Warning,
	TEXT("SGM_REACTION_SKIP NotClient Target=%s NetMode=%d NotifyWindowId=%s"),
	*GetNameSafe(InActor),
	World ? static_cast<int32>(World->GetNetMode()) : -1,
	*NotifyWindowId.ToString());
	return false;
	}

	APawn* TargetPawn = Cast<APawn>(InActor);
	if (!TargetPawn || TargetPawn->IsLocallyControlled() || TargetPawn->GetLocalRole() != ROLE_SimulatedProxy)
	{
	UE_LOG(LogTemp, Warning,
	TEXT("SGM_REACTION_SKIP NotTargetProxy Target=%s Local=%d Role=%d NotifyWindowId=%s"),
	*GetNameSafe(InActor),
	TargetPawn ? TargetPawn->IsLocallyControlled() : false,
	TargetPawn ? static_cast<int32>(TargetPawn->GetLocalRole()) : -1,
	*NotifyWindowId.ToString());
	return false;
	}

	FSGM_ReactionDataEntry Reaction;
	if (!ReactionData->FindReaction(ReactionTag, Reaction))
	{
	UE_LOG(LogTemp, Warning,
	TEXT("SGM_REACTION_SKIP MissingReactionData Target=%s Tag=%s NotifyWindowId=%s"),
	*GetNameSafe(InActor),
	*ReactionTag.ToString(),
	*NotifyWindowId.ToString());
	return false;
	}

	if (!Reaction.Montage)
	{
	UE_LOG(LogTemp, Warning,
	TEXT("SGM_REACTION_SKIP MissingMontage Target=%s Tag=%s NotifyWindowId=%s"),
	*GetNameSafe(InActor),
	*ReactionTag.ToString(),
	*NotifyWindowId.ToString());
	return false;
	}

	USGM_MontageComponent* MontageComponent = InActor->FindComponentByClass<USGM_MontageComponent>();
	if (!MontageComponent)
	{
	UE_LOG(LogTemp, Warning,
	TEXT("SGM_REACTION_SKIP MissingMontageComponent Target=%s Tag=%s Montage=%s NotifyWindowId=%s"),
	*GetNameSafe(InActor),
	*ReactionTag.ToString(),
	*GetNameSafe(Reaction.Montage),
	*NotifyWindowId.ToString());
	return false;
	}

		const bool bPlayed = MontageComponent->PlayPredictedProxyReactionMontage(
			Reaction.Montage,
			Reaction.PlayRate,
			0.0f,
			Reaction.StartSection);

	UE_LOG(LogTemp, Warning,
	TEXT("SGM_REACTION_PROXY_PLAY Target=%s Tag=%s Montage=%s PlayRate=%.3f Section=%s Played=%d NotifyWindowId=%s"),
	*GetNameSafe(InActor),
	*ReactionTag.ToString(),
	*GetNameSafe(Reaction.Montage),
	Reaction.PlayRate,
	*Reaction.StartSection.ToString(),
	bPlayed,
	*NotifyWindowId.ToString());

	return bPlayed;
}
