#include "Components/SGM_ProxyPredictionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "MotionWarpingComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSGMProxyPrediction, Log, All);

USGM_ProxyPredictionComponent::USGM_ProxyPredictionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

void USGM_ProxyPredictionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePredictedProxyRootMotion();
}

bool USGM_ProxyPredictionComponent::PlayPredictedReactionOnTargetProxy(AActor* TargetActor, FGameplayTag ReactionTag)
{
	if (!ReactionData || !ReactionTag.IsValid())
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("PlayPredictedReaction rejected: ReactionData=%s Tag=%s"),
			*GetNameSafe(ReactionData), *ReactionTag.ToString());
		return false;
	}

	FSGM_ReactionDataEntry Reaction;
	if (!ReactionData->FindReaction(ReactionTag, Reaction))
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("PlayPredictedReaction rejected: no reaction entry for Tag=%s Data=%s"),
			*ReactionTag.ToString(), *GetNameSafe(ReactionData));
		return false;
	}

	if (!CanPlayPredictedReactionOnTargetProxy(TargetActor, Reaction))
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("PlayPredictedReaction rejected by CanPlay: Owner=%s Target=%s Tag=%s Montage=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(TargetActor), *ReactionTag.ToString(), *GetNameSafe(Reaction.Montage));
		return false;
	}

	const bool bPlayed = PlayReactionMontageOnActor(TargetActor, Reaction);
	if (!bPlayed)
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("PlayPredictedReaction failed to play montage: Target=%s Tag=%s Montage=%s"),
			*GetNameSafe(TargetActor), *ReactionTag.ToString(), *GetNameSafe(Reaction.Montage));
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		LastReactionTimeByTarget.FindOrAdd(TargetActor) = World->GetTimeSeconds();
	}

	UE_LOG(LogSGMProxyPrediction, Log, TEXT("PlayPredictedReaction success: Owner=%s Target=%s Tag=%s Montage=%s HasRootMotion=%d"),
		*GetNameSafe(GetOwner()), *GetNameSafe(TargetActor), *ReactionTag.ToString(),
		*GetNameSafe(Reaction.Montage), Reaction.Montage ? Reaction.Montage->HasRootMotion() : false);

	return true;
}

bool USGM_ProxyPredictionComponent::CanPlayPredictedReactionOnTargetProxy(AActor* TargetActor,
	const FSGM_ReactionDataEntry& Reaction) const
{
	if (!TargetActor || !Reaction.Montage)
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("CanPlay false: missing target or montage Target=%s Montage=%s"),
			*GetNameSafe(TargetActor), *GetNameSafe(Reaction.Montage));
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("CanPlay false: invalid world or dedicated server World=%s"),
			*GetNameSafe(World));
		return false;
	}

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || OwnerActor->HasAuthority())
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("CanPlay false: owner missing or authority Owner=%s Auth=%d"),
			*GetNameSafe(OwnerActor), OwnerActor ? OwnerActor->HasAuthority() : false);
		return false;
	}

	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("CanPlay false: owner not locally controlled Owner=%s Local=%d"),
			*GetNameSafe(OwnerActor), OwnerPawn ? OwnerPawn->IsLocallyControlled() : false);
		return false;
	}

	// This is proxy prediction, so never drive the local player's own pawn as the target here.
	const APawn* TargetPawn = Cast<APawn>(TargetActor);
	if (TargetPawn && TargetPawn->IsLocallyControlled())
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("CanPlay false: target is locally controlled Target=%s"),
			*GetNameSafe(TargetActor));
		return false;
	}

	const double Now = World->GetTimeSeconds();
	if (const double* LastTime = LastReactionTimeByTarget.Find(TargetActor))
	{
		if (Now - *LastTime < Reaction.MinReplayInterval)
		{
			UE_LOG(LogSGMProxyPrediction, Warning, TEXT("CanPlay false: replay interval Target=%s Delta=%.3f Min=%.3f"),
				*GetNameSafe(TargetActor), Now - *LastTime, Reaction.MinReplayInterval);
			return false;
		}
	}

	return true;
}

bool USGM_ProxyPredictionComponent::PlayReactionMontageOnActor(AActor* TargetActor,
	const FSGM_ReactionDataEntry& Reaction)
{
	if (!TargetActor || !Reaction.Montage) return false;

	const float StartPosition = GetReactionStartPosition(Reaction);

	USkeletalMeshComponent* Mesh = TargetActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh)
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("PlayMontage false: no skeletal mesh Target=%s"),
			*GetNameSafe(TargetActor));
		return false;
	}

	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("PlayMontage false: no anim instance Target=%s Mesh=%s"),
			*GetNameSafe(TargetActor), *GetNameSafe(Mesh));
		return false;
	}

	if (!Reaction.bForceRestart && AnimInstance->Montage_IsPlaying(Reaction.Montage))
	{
		UE_LOG(LogSGMProxyPrediction, Log, TEXT("PlayMontage already playing: Target=%s Montage=%s"),
			*GetNameSafe(TargetActor), *GetNameSafe(Reaction.Montage));
		StartPredictedProxyRootMotion(TargetActor, Reaction, AnimInstance->Montage_GetPosition(Reaction.Montage));
		return true;
	}

	const float PlayedLength = AnimInstance->Montage_Play(Reaction.Montage, Reaction.PlayRate);
	if (PlayedLength <= 0.0f)
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("PlayMontage false: Montage_Play returned %.3f Target=%s Montage=%s"),
			PlayedLength, *GetNameSafe(TargetActor), *GetNameSafe(Reaction.Montage));
		return false;
	}

	if (StartPosition > KINDA_SMALL_NUMBER)
	{
		AnimInstance->Montage_SetPosition(Reaction.Montage, StartPosition);
	}
	else if (Reaction.StartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(Reaction.StartSection, Reaction.Montage);
	}

	StartPredictedProxyRootMotion(TargetActor, Reaction, StartPosition);
	UE_LOG(LogSGMProxyPrediction, Log, TEXT("PlayMontage started: Target=%s Montage=%s Start=%.3f PlayRate=%.3f HasRootMotion=%d"),
		*GetNameSafe(TargetActor), *GetNameSafe(Reaction.Montage), StartPosition,
		Reaction.PlayRate, Reaction.Montage->HasRootMotion());
	return true;
}

float USGM_ProxyPredictionComponent::GetReactionStartPosition(const FSGM_ReactionDataEntry& Reaction) const
{
	if (!Reaction.Montage || Reaction.StartSection == NAME_None) return 0.0f;

	const int32 SectionIndex = Reaction.Montage->GetSectionIndex(Reaction.StartSection);
	if (SectionIndex == INDEX_NONE) return 0.0f;

	float SectionStartTime = 0.0f;
	float SectionEndTime = 0.0f;
	Reaction.Montage->GetSectionStartAndEndTime(SectionIndex, SectionStartTime, SectionEndTime);

	return SectionStartTime;
}

void USGM_ProxyPredictionComponent::StartPredictedProxyRootMotion(AActor* TargetActor,
	const FSGM_ReactionDataEntry& Reaction, float StartPosition)
{
	if (!TargetActor || !Reaction.Montage)
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("StartRootMotion skipped: missing target or montage Target=%s Montage=%s"),
			*GetNameSafe(TargetActor), *GetNameSafe(Reaction.Montage));
		return;
	}

	if (!Reaction.Montage->HasRootMotion())
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("StartRootMotion skipped: montage reports no root motion Target=%s Montage=%s"),
			*GetNameSafe(TargetActor), *GetNameSafe(Reaction.Montage));
		return;
	}

	USkeletalMeshComponent* Mesh = TargetActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh)
	{
		UE_LOG(LogSGMProxyPrediction, Warning, TEXT("StartRootMotion skipped: no mesh Target=%s"),
			*GetNameSafe(TargetActor));
		return;
	}

	for (int32 Index = ActivePredictedProxyRootMotion.Num() - 1; Index >= 0; --Index)
	{
		const FSGM_ActivePredictedProxyRootMotion& ActiveRootMotion = ActivePredictedProxyRootMotion[Index];
		if (ActiveRootMotion.TargetActor.Get() == TargetActor)
		{
			if (USkeletalMeshComponent* PreviousMesh = ActiveRootMotion.TargetMesh.Get())
			{
				PreviousMesh->SetRelativeTransform(ActiveRootMotion.InitialMeshRelativeTransform);
			}

			ActivePredictedProxyRootMotion.RemoveAtSwap(Index);
		}
	}

	FSGM_ActivePredictedProxyRootMotion& ActiveRootMotion = ActivePredictedProxyRootMotion.AddDefaulted_GetRef();
	ActiveRootMotion.TargetActor = TargetActor;
	ActiveRootMotion.TargetMesh = Mesh;
	ActiveRootMotion.Montage = Reaction.Montage;
	ActiveRootMotion.PlayRate = Reaction.PlayRate;
	ActiveRootMotion.PreviousPosition = StartPosition;
	ActiveRootMotion.InitialMeshRelativeTransform = Mesh->GetRelativeTransform();
	ActiveRootMotion.InitialMeshWorldTransform = Mesh->GetComponentTransform();
	ActiveRootMotion.AccumulatedWorldTranslation = FVector::ZeroVector;

	SetComponentTickEnabled(true);
	UE_LOG(LogSGMProxyPrediction, Log, TEXT("StartRootMotion: Target=%s Mesh=%s Montage=%s Start=%.3f ActiveCount=%d"),
		*GetNameSafe(TargetActor), *GetNameSafe(Mesh), *GetNameSafe(Reaction.Montage), StartPosition,
		ActivePredictedProxyRootMotion.Num());
}

void USGM_ProxyPredictionComponent::UpdatePredictedProxyRootMotion()
{
	for (int32 Index = ActivePredictedProxyRootMotion.Num() - 1; Index >= 0; --Index)
	{
		FSGM_ActivePredictedProxyRootMotion& ActiveRootMotion = ActivePredictedProxyRootMotion[Index];
		AActor* TargetActor = ActiveRootMotion.TargetActor.Get();
		USkeletalMeshComponent* Mesh = ActiveRootMotion.TargetMesh.Get();
		UAnimMontage* Montage = ActiveRootMotion.Montage;

		if (!TargetActor || !Mesh || !Montage)
		{
			ActivePredictedProxyRootMotion.RemoveAtSwap(Index);
			continue;
		}

		UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!AnimInstance || !AnimInstance->Montage_IsPlaying(Montage))
		{
			Mesh->SetRelativeTransform(ActiveRootMotion.InitialMeshRelativeTransform);
			UE_LOG(LogSGMProxyPrediction, Log, TEXT("RootMotion ended: reset mesh Target=%s Mesh=%s"),
				*GetNameSafe(TargetActor), *GetNameSafe(Mesh));
			ActivePredictedProxyRootMotion.RemoveAtSwap(Index);
			continue;
		}

		const float CurrentPosition = AnimInstance->Montage_GetPosition(Montage);
		if (CurrentPosition <= ActiveRootMotion.PreviousPosition)
		{
			UE_LOG(LogSGMProxyPrediction, Verbose, TEXT("RootMotion tick skipped non-forward position Target=%s Prev=%.3f Current=%.3f"),
				*GetNameSafe(TargetActor), ActiveRootMotion.PreviousPosition, CurrentPosition);
			ActiveRootMotion.PreviousPosition = CurrentPosition;
			continue;
		}

		const FTransform LocalRootMotion = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(
			Montage, ActiveRootMotion.PreviousPosition, CurrentPosition);

		ActiveRootMotion.PreviousPosition = CurrentPosition;

		// Montage root motion is authored in the mesh/animation space. The pawn actor may have a different facing
		// because the mesh is rotated relative to the capsule, so use the mesh rotation for visual proxy prediction.
		const FVector WorldTranslation = ActiveRootMotion.InitialMeshWorldTransform.GetRotation().RotateVector(
			LocalRootMotion.GetTranslation());
		const FVector BeforeMeshLocation = Mesh->GetComponentLocation();
		ActiveRootMotion.AccumulatedWorldTranslation += WorldTranslation;

		// Mover can restore the proxy visual component every frame, so apply the full accumulated visual offset.
		Mesh->SetWorldLocation(ActiveRootMotion.InitialMeshWorldTransform.GetLocation()
			+ ActiveRootMotion.AccumulatedWorldTranslation, false, nullptr, ETeleportType::None);

		const FVector AfterMeshLocation = Mesh->GetComponentLocation();

		UE_LOG(LogSGMProxyPrediction, Log,
			TEXT("RootMotion visual apply: Target=%s Mesh=%s Prev=%.3f Current=%.3f LocalDelta=%s WorldDelta=%s Accumulated=%s MeshBefore=%s MeshAfter=%s AppliedDelta=%s ActorLoc=%s"),
			*GetNameSafe(TargetActor), *GetNameSafe(Mesh), ActiveRootMotion.PreviousPosition, CurrentPosition,
			*LocalRootMotion.GetTranslation().ToString(), *WorldTranslation.ToString(),
			*ActiveRootMotion.AccumulatedWorldTranslation.ToString(),
			*BeforeMeshLocation.ToString(), *AfterMeshLocation.ToString(), *(AfterMeshLocation - BeforeMeshLocation).ToString(),
			*TargetActor->GetActorLocation().ToString());
	}

	if (ActivePredictedProxyRootMotion.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}
