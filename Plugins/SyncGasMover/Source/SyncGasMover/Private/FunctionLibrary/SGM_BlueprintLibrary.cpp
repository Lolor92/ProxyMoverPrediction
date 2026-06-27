#include "FunctionLibrary/SGM_BlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "Components/SGM_MontageComponent.h"
#include "GameFramework/Actor.h"

bool USGM_BlueprintLibrary::PlayMoverMontageFromAbility(UGameplayAbility* Ability, UAnimMontage* Montage,
	float PlayRate, float StartTimeSeconds, FName StartSection)
{
	if (!Ability) return false;

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	return PlayMoverMontage(AvatarActor, Montage, PlayRate, StartTimeSeconds, StartSection);
}

bool USGM_BlueprintLibrary::PlayMoverMontage(AActor* AvatarActor, UAnimMontage* Montage,
	float PlayRate, float StartTimeSeconds, FName StartSection)
{
	if (!AvatarActor || !Montage) return false;

	USGM_MontageComponent* MontageComponent = AvatarActor->FindComponentByClass<USGM_MontageComponent>();
	if (!MontageComponent) return false;

	return MontageComponent->PlayPredictedReplicatedMontage(Montage, PlayRate, StartTimeSeconds, StartSection);
}
