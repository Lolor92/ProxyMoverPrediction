#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "SGM_AnimRootMotionLayeredMove.generated.h"

USTRUCT(BlueprintType)
struct SYNCGASMOVER_API FSGM_AnimRootMotionLayeredMove : public FLayeredMove_AnimRootMotion
{
	GENERATED_BODY()

	bool bIgnoreRetriggerCancellationWhileQueued = false;

	// 1 means normal montage root motion. 0 keeps montage sync alive but contributes no root-motion movement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float RootMotionScale = 1.0f;

	// Prevent root motion from driving the capsule into a pawn in front of the owner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bStopRootMotionOnPawnContact = false;

	// Half-angle of the forward contact cone. 40 means 40 degrees left/right.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float PawnContactBlockHalfAngleDegrees = 40.0f;

	// Prevents hit/no-hit flicker during resimulation. Cleared automatically when the pawn moves away/out of cone.
	TWeakObjectPtr<AActor> StickyPawnContactBlockActor;

	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
		const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

template<>
struct TStructOpsTypeTraits<FSGM_AnimRootMotionLayeredMove> : public TStructOpsTypeTraitsBase2<FSGM_AnimRootMotionLayeredMove>
{
	enum
	{
		WithCopy = true
	};
};
