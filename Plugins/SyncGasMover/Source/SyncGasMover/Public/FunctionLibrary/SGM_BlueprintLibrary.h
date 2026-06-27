#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SGM_BlueprintLibrary.generated.h"

class AActor;
class UAnimMontage;
class UGameplayAbility;

UCLASS()
class SYNCGASMOVER_API USGM_BlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Drop-in ability helper. Finds SGM_MontageComponent on the ability avatar and plays/replicates through it.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage", meta = (DefaultToSelf = "Ability"))
	static bool PlayMoverMontageFromAbility(UGameplayAbility* Ability, UAnimMontage* Montage,
		float PlayRate = 1.0f, float StartTimeSeconds = 0.0f, FName StartSection = NAME_None);

	// Actor helper for non-GAS Blueprint use.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	static bool PlayMoverMontage(AActor* AvatarActor, UAnimMontage* Montage,
		float PlayRate = 1.0f, float StartTimeSeconds = 0.0f, FName StartSection = NAME_None);
};
