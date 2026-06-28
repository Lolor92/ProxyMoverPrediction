#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SGM_UpperLowerBlendProviderInterface.generated.h"

UINTERFACE(BlueprintType)
class SYNCGASMOVER_API USGM_UpperLowerBlendProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class SYNCGASMOVER_API ISGM_UpperLowerBlendProviderInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SyncGasMover|Animation")
	bool GetCanBlendUpperAndLowerBodyForAnimation() const;
};
