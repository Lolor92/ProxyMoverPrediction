#include "Animation/SGM_MoverAnimInstance.h"

#include "Animation/SGM_UpperLowerBlendProviderInterface.h"
#include "Components/ActorComponent.h"
#include "Debug/SGM_DebugLog.h"
#include "GameFramework/Actor.h"

void USGM_MoverAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	bHasCachedCanBlendUpperAndLowerBody = false;
	UpdateCanBlendUpperAndLowerBody();
}

void USGM_MoverAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	UpdateCanBlendUpperAndLowerBody();
}

void USGM_MoverAnimInstance::UpdateCanBlendUpperAndLowerBody()
{
	const bool bNewValue = ReadCanBlendUpperAndLowerBodyFromOwner();
	if (bHasCachedCanBlendUpperAndLowerBody && bCanBlendUpperAndLowerBody == bNewValue)
	{
		return;
	}

	const bool bOldValue = bCanBlendUpperAndLowerBody;
	bCanBlendUpperAndLowerBody = bNewValue;
	bHasCachedCanBlendUpperAndLowerBody = true;

	SGM_BLEND_BOOL_LOG(TEXT("SGM_BLEND_BOOL AnimInstance=%s Owner=%s CanBlendUpperAndLowerBody %d -> %d"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwningActor()),
		bOldValue,
		bCanBlendUpperAndLowerBody);
}

bool USGM_MoverAnimInstance::ReadCanBlendUpperAndLowerBodyFromOwner() const
{
	AActor* OwningActor = GetOwningActor();
	if (!OwningActor)
	{
		return false;
	}

	if (OwningActor->GetClass()->ImplementsInterface(USGM_UpperLowerBlendProviderInterface::StaticClass()))
	{
		return ISGM_UpperLowerBlendProviderInterface::Execute_GetCanBlendUpperAndLowerBodyForAnimation(OwningActor);
	}

	TArray<UActorComponent*> Components;
	OwningActor->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->GetClass()->ImplementsInterface(USGM_UpperLowerBlendProviderInterface::StaticClass()))
		{
			return ISGM_UpperLowerBlendProviderInterface::Execute_GetCanBlendUpperAndLowerBodyForAnimation(Component);
		}
	}

	return false;
}
