#include "LayeredMoves/SGM_AnimRootMotionLayeredMove.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
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

	FVector ScaledTranslation = WorldSpaceRootMotion.GetTranslation() * RootMotionScale;
	FVector ScaledRotationVector = WorldSpaceRootMotion.GetRotation().ToRotationVector() * RootMotionScale;

	if (bStopRootMotionOnPawnContact && RootMotionScale > UE_KINDA_SMALL_NUMBER && !ScaledTranslation.IsNearlyZero())
	{
		const AActor* OwnerActor = MoverComp ? MoverComp->GetOwner() : nullptr;
		const UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
		const UCapsuleComponent* OwnerCapsule = OwnerActor ? OwnerActor->FindComponentByClass<UCapsuleComponent>() : nullptr;

		if (OwnerActor && World && OwnerCapsule)
		{
			FVector SweepDelta = ScaledTranslation;
			SweepDelta.Z = 0.0f;

			if (!SweepDelta.IsNearlyZero())
			{
				const FVector StartLocation = SyncState->GetLocation_WorldSpace();
				const FVector EndLocation = StartLocation + SweepDelta;
				const FCollisionShape SweepShape = FCollisionShape::MakeCapsule(
					OwnerCapsule->GetScaledCapsuleRadius(),
					OwnerCapsule->GetScaledCapsuleHalfHeight());

				FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SGMRootMotionPawnContactSweep), false, OwnerActor);
				FCollisionObjectQueryParams ObjectQueryParams;
				ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

				FHitResult Hit;
				const bool bHitPawn = World->SweepSingleByObjectType(
					Hit,
					StartLocation,
					EndLocation,
					SyncState->GetOrientation_WorldSpace().Quaternion(),
					ObjectQueryParams,
					SweepShape,
					QueryParams);

				const AActor* HitActor = Hit.GetActor();
				if (bHitPawn && HitActor && HitActor != OwnerActor && HitActor->IsA<APawn>())
				{
					FVector ToHitActor = HitActor->GetActorLocation() - StartLocation;
					ToHitActor.Z = 0.0f;

					const FVector Forward = SyncState->GetOrientation_WorldSpace().Vector().GetSafeNormal2D();
					const float MinForwardDot = FMath::Cos(FMath::DegreesToRadians(PawnContactBlockHalfAngleDegrees));
					const bool bWithinBlockAngle = ToHitActor.IsNearlyZero()
						|| FVector::DotProduct(Forward, ToHitActor.GetSafeNormal()) >= MinForwardDot;

					if (bWithinBlockAngle)
					{
						ScaledTranslation = FVector::ZeroVector;
						ScaledRotationVector = FVector::ZeroVector;
					}
				}
			}
		}
	}

	OutProposedMove = FProposedMove();
	OutProposedMove.MixMode = MixMode;

	if (DeltaSeconds > UE_KINDA_SMALL_NUMBER)
	{
		OutProposedMove.LinearVelocity = ScaledTranslation / DeltaSeconds;
		OutProposedMove.AngularVelocityDegrees = FMath::RadiansToDegrees(ScaledRotationVector / DeltaSeconds);
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
	Ar << bStopRootMotionOnPawnContact;
	Ar << PawnContactBlockHalfAngleDegrees;
}

UScriptStruct* FSGM_AnimRootMotionLayeredMove::GetScriptStruct() const
{
	return FSGM_AnimRootMotionLayeredMove::StaticStruct();
}

FString FSGM_AnimRootMotionLayeredMove::ToSimpleString() const
{
	return TEXT("SGM_AnimRootMotion");
}
