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

namespace
{
	FString SGMLayeredMoveActorState(const AActor* Actor)
	{
		const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
		const APawn* Pawn = Cast<APawn>(Actor);

		return FString::Printf(TEXT("World=%s NetMode=%d Actor=%s Role=%d RemoteRole=%d Local=%d Auth=%d Loc=%s"),
			World ? *World->GetName() : TEXT("None"),
			World ? static_cast<int32>(World->GetNetMode()) : -1,
			*GetNameSafe(Actor),
			Actor ? static_cast<int32>(Actor->GetLocalRole()) : -1,
			Actor ? static_cast<int32>(Actor->GetRemoteRole()) : -1,
			Pawn ? Pawn->IsLocallyControlled() : false,
			Actor ? Actor->HasAuthority() : false,
			Actor ? *Actor->GetActorLocation().ToString() : TEXT("None"));
	}
}

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
	const AActor* OwnerActor = MoverComp ? MoverComp->GetOwner() : nullptr;

	if (!TimeStep.bIsResimulating)
	{
		bool bIsMontageStillPlaying = false;
		float CurrentAnimMontagePosition = -1.0f;

		if (const USkeletalMeshComponent* MeshComp = MoverComp ? Cast<USkeletalMeshComponent>(MoverComp->GetPrimaryVisualComponent()) : nullptr)
		{
			if (const UAnimInstance* MeshAnimInstance = MeshComp->GetAnimInstance())
			{
				bIsMontageStillPlaying = MontageState.Montage && MeshAnimInstance->Montage_IsPlaying(MontageState.Montage);
				CurrentAnimMontagePosition = MontageState.Montage ? MeshAnimInstance->Montage_GetPosition(MontageState.Montage) : -1.0f;
			}
		}

		if (!bIsMontageStillPlaying)
		{
			UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG RootMotionMove STOP_MONTAGE_NOT_PLAYING %s Montage=%s BaseMs=%.3f StartMs=%.3f StepMs=%.3f AnimPos=%.3f DurationMs=%.3f RootScale=%.3f"),
				*SGMLayeredMoveActorState(OwnerActor), *GetNameSafe(MontageState.Montage),
				TimeStep.BaseSimTimeMs, StartSimTimeMs, TimeStep.StepMs, CurrentAnimMontagePosition,
				DurationMs, RootMotionScale);

			DurationMs = 0.0f;
			return false;
		}
	}

	const float DeltaSeconds = TimeStep.StepMs / 1000.0f;
	const FMoverDefaultSyncState* SyncState =
		SimState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

	if (!SyncState)
	{
		UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG RootMotionMove FAIL_NO_SYNC_STATE %s Montage=%s BaseMs=%.3f"),
			*SGMLayeredMoveActorState(OwnerActor), *GetNameSafe(MontageState.Montage), TimeStep.BaseSimTimeMs);
		return false;
	}

	const double SecondsSinceMontageStarted = (TimeStep.BaseSimTimeMs - StartSimTimeMs) / 1000.0;
	const double ScaledSecondsSinceMontageStarted = SecondsSinceMontageStarted * MontageState.PlayRate;

	const float ExtractionStartPosition =
		MontageState.StartingMontagePosition + ScaledSecondsSinceMontageStarted;
	const float ExtractionEndPosition = ExtractionStartPosition + (DeltaSeconds * MontageState.PlayRate);
	const float MontageLength = MontageState.Montage ? MontageState.Montage->GetPlayLength() : 0.0f;
	const bool bNearMontageEnd = MontageLength > 0.0f && ExtractionEndPosition >= MontageLength - 0.250f;

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
		const UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
		const UCapsuleComponent* OwnerCapsule = OwnerActor ? OwnerActor->FindComponentByClass<UCapsuleComponent>() : nullptr;

		if (OwnerActor && World && OwnerCapsule)
		{
			const FVector StartLocation = SyncState->GetLocation_WorldSpace();
			const FVector Forward = SyncState->GetOrientation_WorldSpace().Vector().GetSafeNormal2D();
			const float MinForwardDot = FMath::Cos(FMath::DegreesToRadians(PawnContactBlockHalfAngleDegrees));

			if (AActor* StickyActor = StickyPawnContactBlockActor.Get())
			{
				const UCapsuleComponent* StickyCapsule = StickyActor->FindComponentByClass<UCapsuleComponent>();
				const float OwnerRadius = OwnerCapsule->GetScaledCapsuleRadius();
				const float StickyRadius = StickyCapsule ? StickyCapsule->GetScaledCapsuleRadius() : OwnerRadius;

				// The sweep can still hit a pawn even when actor-center distance is larger than
				// radius+radius, especially during large resim root-motion steps. Scale the memory
				// leash by the current step delta so contact memory does not flicker off/on.
				const float StickyContactSlack = FMath::Max(45.0f, ScaledTranslation.Size2D() + 20.0f);
				const float MaxStickyDistance = OwnerRadius + StickyRadius + StickyContactSlack;

				FVector ToStickyActor = StickyActor->GetActorLocation() - StartLocation;
				ToStickyActor.Z = 0.0f;

				const bool bStickyStillClose = ToStickyActor.SizeSquared() <= FMath::Square(MaxStickyDistance);
				const bool bStickyStillInCone = ToStickyActor.IsNearlyZero()
					|| FVector::DotProduct(Forward, ToStickyActor.GetSafeNormal()) >= MinForwardDot;

				if (bStickyStillClose && bStickyStillInCone)
				{
					UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG RootMotionMove CONTACT_MEMORY_BLOCK %s Montage=%s Hit=%s BaseMs=%.3f Extract=%.3f->%.3f Delta=%s Dist=%.3f MaxDist=%.3f HalfAngle=%.3f"),
						*SGMLayeredMoveActorState(OwnerActor), *GetNameSafe(MontageState.Montage), *GetNameSafe(StickyActor),
						TimeStep.BaseSimTimeMs, ExtractionStartPosition, ExtractionEndPosition,
						*ScaledTranslation.ToString(), ToStickyActor.Size(), MaxStickyDistance, PawnContactBlockHalfAngleDegrees);

					ScaledTranslation = FVector::ZeroVector;
					ScaledRotationVector = FVector::ZeroVector;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG RootMotionMove CONTACT_MEMORY_RELEASE %s Montage=%s Hit=%s BaseMs=%.3f Dist=%.3f MaxDist=%.3f InCone=%d"),
						*SGMLayeredMoveActorState(OwnerActor), *GetNameSafe(MontageState.Montage), *GetNameSafe(StickyActor),
						TimeStep.BaseSimTimeMs, ToStickyActor.Size(), MaxStickyDistance, bStickyStillInCone);

					StickyPawnContactBlockActor.Reset();
				}
			}

			FVector SweepDelta = ScaledTranslation;
			SweepDelta.Z = 0.0f;

			if (!SweepDelta.IsNearlyZero())
			{
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

					const bool bWithinBlockAngle = ToHitActor.IsNearlyZero()
						|| FVector::DotProduct(Forward, ToHitActor.GetSafeNormal()) >= MinForwardDot;

					if (bWithinBlockAngle)
					{
						StickyPawnContactBlockActor = const_cast<AActor*>(HitActor);

						UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG RootMotionMove CONTACT_BLOCK %s Montage=%s Hit=%s BaseMs=%.3f Extract=%.3f->%.3f Delta=%s HalfAngle=%.3f"),
							*SGMLayeredMoveActorState(OwnerActor), *GetNameSafe(MontageState.Montage), *GetNameSafe(HitActor),
							TimeStep.BaseSimTimeMs, ExtractionStartPosition, ExtractionEndPosition,
							*ScaledTranslation.ToString(), PawnContactBlockHalfAngleDegrees);

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

	if (bNearMontageEnd || TimeStep.bIsResimulating || ScaledTranslation.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("SGM_DEBUG RootMotionMove GENERATE %s Montage=%s Resim=%d BaseMs=%.3f StartMs=%.3f StepMs=%.3f Extract=%.3f->%.3f Len=%.3f LocalDelta=%s WorldDelta=%s Scale=%.3f MixMode=%d OutVel=%s SimLoc=%s"),
			*SGMLayeredMoveActorState(OwnerActor), *GetNameSafe(MontageState.Montage), TimeStep.bIsResimulating,
			TimeStep.BaseSimTimeMs, StartSimTimeMs, TimeStep.StepMs,
			ExtractionStartPosition, ExtractionEndPosition, MontageLength,
			*LocalRootMotion.GetTranslation().ToString(), *ScaledTranslation.ToString(),
			RootMotionScale, static_cast<int32>(OutProposedMove.MixMode),
			*OutProposedMove.LinearVelocity.ToString(), *SyncState->GetLocation_WorldSpace().ToString());
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
