#include "LayeredMoves/SGM_AnimRootMotionLayeredMove.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "MotionWarpingComponent.h"
#include "MoverComponent.h"
#include "MoverTypes.h"
#include "Tags/SGM_NativeTags.h"

bool FSGM_AnimRootMotionLayeredMove::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	const FGameplayTag RootMotionTag = TAG_SyncGasMover_RootMotion.GetTag();
	const bool bMatchesRootMotionTag = bExactMatch
		? RootMotionTag.MatchesTagExact(TagToFind)
		: RootMotionTag.MatchesTag(TagToFind);

	if (bIgnoreRetriggerCancellationWhileQueued && StartSimTimeMs < 0.0)
	{
		return false;
	}

	return bMatchesRootMotionTag;
}

bool FSGM_AnimRootMotionLayeredMove::GenerateMove(const FMoverTickStartData& SimState,
	const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard,
	FProposedMove& OutProposedMove)
{
	// Match Epic's move lifetime rule: if the montage is interrupted, this move ends too.
	if (!TimeStep.bIsResimulating)
	{
		bool bIsMontageStillPlaying = false;

		if (const USkeletalMeshComponent* MeshComp = Cast<USkeletalMeshComponent>(MoverComp->GetPrimaryVisualComponent()))
		{
			if (const UAnimInstance* MeshAnimInstance = MeshComp->GetAnimInstance())
			{
				bIsMontageStillPlaying = MontageState.Montage && MeshAnimInstance->Montage_IsPlaying(MontageState.Montage);
			}
		}

		if (!bIsMontageStillPlaying)
		{
			DurationMs = 0.0f;
			return false;
		}
	}

	const float DeltaSeconds = TimeStep.StepMs / 1000.0f;
	const FMoverDefaultSyncState* SyncState =
		SimState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

	if (!SyncState)
	{
		return false;
	}

	const double SecondsSinceMontageStarted = (TimeStep.BaseSimTimeMs - StartSimTimeMs) / 1000.0;
	const double ScaledSecondsSinceMontageStarted = SecondsSinceMontageStarted * MontageState.PlayRate;

	const float ExtractionStartPosition =
		MontageState.StartingMontagePosition + ScaledSecondsSinceMontageStarted;
	const float ExtractionEndPosition = ExtractionStartPosition + (DeltaSeconds * MontageState.PlayRate);

	const FTransform LocalRootMotion = MontageState.Montage
		? UMotionWarpingUtilities::ExtractRootMotionFromAnimation(MontageState.Montage,
			ExtractionStartPosition, ExtractionEndPosition)
		: FTransform::Identity;

	FMotionWarpingUpdateContext WarpingContext;
	WarpingContext.Animation = MontageState.Montage;
	WarpingContext.CurrentPosition = ExtractionEndPosition;
	WarpingContext.PreviousPosition = ExtractionStartPosition;
	WarpingContext.PlayRate = MontageState.PlayRate;
	WarpingContext.Weight = 1.0f;

	const FTransform SimActorTransform(
		SyncState->GetOrientation_WorldSpace().Quaternion(),
		SyncState->GetLocation_WorldSpace());

	const FTransform WorldSpaceRootMotion =
		MoverComp->ConvertLocalRootMotionToWorld(LocalRootMotion, DeltaSeconds, &SimActorTransform, &WarpingContext);

	OutProposedMove = FProposedMove();
	OutProposedMove.MixMode = MixMode;

	if (DeltaSeconds > UE_KINDA_SMALL_NUMBER)
	{
		// Scaling the output lets us keep montage sync active while giving movement back to locomotion.
		OutProposedMove.LinearVelocity =
			(WorldSpaceRootMotion.GetTranslation() * RootMotionScale) / DeltaSeconds;
		OutProposedMove.AngularVelocityDegrees =
			FMath::RadiansToDegrees((WorldSpaceRootMotion.GetRotation().ToRotationVector() * RootMotionScale) / DeltaSeconds);
	}

	MontageState.CurrentPosition = ExtractionStartPosition;
	return true;
}

FLayeredMoveBase* FSGM_AnimRootMotionLayeredMove::Clone() const
{
	return new FSGM_AnimRootMotionLayeredMove(*this);
}

void FSGM_AnimRootMotionLayeredMove::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << RootMotionScale;
}

UScriptStruct* FSGM_AnimRootMotionLayeredMove::GetScriptStruct() const
{
	return FSGM_AnimRootMotionLayeredMove::StaticStruct();
}

FString FSGM_AnimRootMotionLayeredMove::ToSimpleString() const
{
	return TEXT("SGM_AnimRootMotion");
}
