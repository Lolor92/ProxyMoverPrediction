#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SGM_MoverAnimInstance.generated.h"

UCLASS(Blueprintable, BlueprintType)
class SYNCGASMOVER_API USGM_MoverAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "SyncGasMover|Animation", Transient)
	bool bCanBlendUpperAndLowerBody = false;

private:
	void UpdateCanBlendUpperAndLowerBody();
	bool ReadCanBlendUpperAndLowerBodyFromOwner() const;

	bool bHasCachedCanBlendUpperAndLowerBody = false;
};
