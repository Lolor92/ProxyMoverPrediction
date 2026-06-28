#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SGM_ReactionData.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct FSGM_ReactionDataEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Reaction")
	FGameplayTag ReactionTag;

	// Server-side gameplay event tag used to wake an already-granted reaction ability on the target ASC.
	// Leave invalid if this reaction should stay visual-only for now.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Reaction|Ability")
	FGameplayTag ReactionTriggerTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Reaction")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Reaction")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Reaction")
	FName StartSection = NAME_None;

	// Prevents repeated local replays if more than one hit window or overlap reports the same target quickly.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Reaction", meta = (ClampMin = "0.0", Units = "Seconds"))
	float MinReplayInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Reaction")
	bool bForceRestart = false;
};

UCLASS(BlueprintType)
class SYNCGASMOVER_API USGM_ReactionData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SyncGasMover|Reaction", meta = (TitleProperty = "ReactionTag"))
	TArray<FSGM_ReactionDataEntry> Reactions;

	UFUNCTION(BlueprintPure, Category = "SyncGasMover|Reaction")
	bool FindReaction(FGameplayTag ReactionTag, FSGM_ReactionDataEntry& OutReaction) const;

	const FSGM_ReactionDataEntry* FindReactionPtr(FGameplayTag ReactionTag) const;
};
