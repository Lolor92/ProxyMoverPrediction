#include "Notifies/SGM_PredictedCollisionNotifyState.h"
#include "Components/SGM_ProxyPredictionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void USGM_PredictedCollisionNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;

	// A new notify window means a new swing/hit window, so clear the per-target hit list.
	FSGM_PredictedCollisionRuntimeWindow& Window = ActiveWindowsByMesh.FindOrAdd(MeshComp);
	Window.ProcessedTargets.Reset();

	AActor* OwnerActor = MeshComp->GetOwner();
	const bool bShouldRunCollision = ShouldRunCollision(OwnerActor);

	UE_LOG(LogTemp, Warning,
		TEXT("SGM_COLLISION_NOTIFY_BEGIN_RAW Mesh=%s Owner=%s NetMode=%d Auth=%d LocalPawn=%d ShouldRun=%d"),
		*GetNameSafe(MeshComp),
		*GetNameSafe(OwnerActor),
		MeshComp->GetWorld() ? static_cast<int32>(MeshComp->GetWorld()->GetNetMode()) : -1,
		OwnerActor ? OwnerActor->HasAuthority() : false,
		Cast<APawn>(OwnerActor) ? Cast<APawn>(OwnerActor)->IsLocallyControlled() : false,
		bShouldRunCollision);

	if (!bShouldRunCollision) return;
	
	Window.PredictionKey = NextPredictionKey++;
	if (Window.PredictionKey <= 0)
	{
		Window.PredictionKey = 1;
		NextPredictionKey = 2;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("SGM_COLLISION_KEY BEGIN Owner=%s NetMode=%d Auth=%d Key=%d"),
		*GetNameSafe(OwnerActor),
		MeshComp->GetWorld() ? static_cast<int32>(MeshComp->GetWorld()->GetNetMode()) : -1,
		OwnerActor ? OwnerActor->HasAuthority() : false,
		Window.PredictionKey);

	FTransform CurrentTransform;
	if (!BuildTraceTransform(MeshComp, CurrentTransform)) return;

	// Store the first transform. Ticks will sweep from this position to the next position.
	PreviousTransforms.Add(MeshComp, CurrentTransform);
}

void USGM_PredictedCollisionNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp) return;

	FTransform* PreviousTransform = PreviousTransforms.Find(MeshComp);
	if (!PreviousTransform) return;

	FTransform CurrentTransform;
	if (!BuildTraceTransform(MeshComp, CurrentTransform)) return;

	SweepCollision(MeshComp, *PreviousTransform, CurrentTransform);

	// The next tick starts from where this tick ended.
	*PreviousTransform = CurrentTransform;
}

void USGM_PredictedCollisionNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	PreviousTransforms.Remove(MeshComp);
	ActiveWindowsByMesh.Remove(MeshComp);
}

FString USGM_PredictedCollisionNotifyState::GetNotifyName_Implementation() const
{
	return TEXT("PredictedCollisionNotify");
}

void USGM_PredictedCollisionNotifyState::HandlePredictedCollisionHit(AActor* OwningActor, AActor* HitActor,
	const FHitResult& HitResult, int32 PredictionKey)
{
	if (bPlayPredictedReactionOnClient && PredictedReactionTag.IsValid())
	{
		// The notify only carries the tag. The component owns the rules for whether a proxy can be animated locally.
		if (USGM_ProxyPredictionComponent* PredictionComponent =
			OwningActor ? OwningActor->FindComponentByClass<USGM_ProxyPredictionComponent>() : nullptr)
		{
			PredictionComponent->PlayPredictedReactionOnTargetProxy(HitActor, PredictedReactionTag, PredictionKey);
		}
	}

	// Keep the Blueprint hook for hit effects, logs, and quick iteration.
	OnPredictedCollisionHit(OwningActor, HitActor, HitResult, PredictedReactionTag, PredictionKey);
	
	UE_LOG(LogTemp, Warning,
	TEXT("SGM_COLLISION_KEY HIT Owner=%s Target=%s Tag=%s NetMode=%d Auth=%d Key=%d"),
	*GetNameSafe(OwningActor),
	*GetNameSafe(HitActor),
	*PredictedReactionTag.ToString(),
	OwningActor && OwningActor->GetWorld() ? static_cast<int32>(OwningActor->GetWorld()->GetNetMode()) : -1,
	OwningActor ? OwningActor->HasAuthority() : false,
	PredictionKey);
}

bool USGM_PredictedCollisionNotifyState::ShouldRunCollision(const AActor* OwnerActor) const
{
	if (!OwnerActor) return false;

	// Server runs the authoritative collision.
	if (OwnerActor->HasAuthority()) return true;

	// Owning client runs the same sweep locally for instant feedback against proxy targets.
	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

bool USGM_PredictedCollisionNotifyState::BuildTraceTransform(
	const USkeletalMeshComponent* MeshComp,
	FTransform& OutTransform) const
{
	if (!MeshComp) return false;

	const bool bHasSocket = !SourceSocketName.IsNone() && MeshComp->DoesSocketExist(SourceSocketName);

	const FTransform SourceTransform = bHasSocket
		? MeshComp->GetSocketTransform(SourceSocketName, RTS_World)
		: MeshComp->GetComponentTransform();

	const FTransform RelativeTransform(RelativeRotation, RelativeLocation);

	// Relative first, socket/component second: this applies the offset in socket space.
	OutTransform = RelativeTransform * SourceTransform;
	return true;
}

FCollisionShape USGM_PredictedCollisionNotifyState::MakeCollisionShape() const
{
	switch (CollisionShape)
	{
	case ESGM_PredictedCollisionShape::Sphere:
		return FCollisionShape::MakeSphere(FMath::Max(SphereRadius, 1.0f));

	case ESGM_PredictedCollisionShape::Capsule:
		return FCollisionShape::MakeCapsule(
			FMath::Max(CapsuleRadius, 1.0f),
			FMath::Max(CapsuleHalfHeight, 1.0f));

	case ESGM_PredictedCollisionShape::Box:
	default:
		return FCollisionShape::MakeBox(BoxExtent.ComponentMax(FVector(1.0f)));
	}
}

void USGM_PredictedCollisionNotifyState::SweepCollision(USkeletalMeshComponent* MeshComp, const FTransform& PreviousTransform,
	const FTransform& CurrentTransform)
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;

	if (!OwnerActor || !World) return;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SGM_PredictedCollisionSweep), false, OwnerActor);
	QueryParams.AddIgnoredActor(OwnerActor);

	const FVector PreviousLocation = PreviousTransform.GetLocation();
	const FVector CurrentLocation = CurrentTransform.GetLocation();

	const float SweepDistance = FVector::Dist(PreviousLocation, CurrentLocation);
	const float SafeStepDistance = FMath::Max(MaxSweepStepDistance, 1.0f);
	const int32 NumSteps = FMath::Max(1, FMath::CeilToInt(SweepDistance / SafeStepDistance));

	const FCollisionShape Shape = MakeCollisionShape();
	const FQuat SweepRotation = CurrentTransform.GetRotation();

	for (int32 StepIndex = 0; StepIndex < NumSteps; ++StepIndex)
	{
		const float StartAlpha = static_cast<float>(StepIndex) / static_cast<float>(NumSteps);
		const float EndAlpha = static_cast<float>(StepIndex + 1) / static_cast<float>(NumSteps);

		const FVector StepStart = FMath::Lerp(PreviousLocation, CurrentLocation, StartAlpha);
		const FVector StepEnd = FMath::Lerp(PreviousLocation, CurrentLocation, EndAlpha);

		TArray<FHitResult> Hits;
		const bool bHit = World->SweepMultiByChannel(Hits, StepStart, StepEnd, SweepRotation,
			TraceChannel.GetValue(), Shape, QueryParams);

		if (bDrawDebug)
		{
			DrawDebugSweep(World, StepStart, StepEnd, SweepRotation, bHit ? FColor::Red : FColor::Green);
		}

		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();

			UE_LOG(LogTemp, Warning,
				TEXT("SGM_COLLISION_RAW_HIT Owner=%s HitActor=%s HitComponent=%s Blocking=%d bOnlyHitPawns=%d IsPawn=%d AlreadyProcessed=%d"),
				*GetNameSafe(OwnerActor),
				*GetNameSafe(HitActor),
				*GetNameSafe(Hit.GetComponent()),
				Hit.bBlockingHit,
				bOnlyHitPawns,
				HitActor ? HitActor->IsA<APawn>() : false,
				HitActor ? HasAlreadyProcessedTarget(MeshComp, HitActor) : true);

			if (!HitActor)
			{
				UE_LOG(LogTemp, Warning, TEXT("SGM_COLLISION_SKIP Null HitActor"));
				continue;
			}

			if (HitActor == OwnerActor)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("SGM_COLLISION_SKIP Self HitActor=%s"),
					*GetNameSafe(HitActor));
				continue;
			}

			if (bOnlyHitPawns && !HitActor->IsA<APawn>())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("SGM_COLLISION_SKIP NotPawn HitActor=%s Class=%s"),
					*GetNameSafe(HitActor),
					*GetNameSafe(HitActor->GetClass()));
				continue;
			}

			if (HasAlreadyProcessedTarget(MeshComp, HitActor))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("SGM_COLLISION_SKIP AlreadyProcessed HitActor=%s"),
					*GetNameSafe(HitActor));
				continue;
			}

			MarkTargetProcessed(MeshComp, HitActor);

			const FSGM_PredictedCollisionRuntimeWindow* Window = ActiveWindowsByMesh.Find(MeshComp);
			const int32 PredictionKey = Window ? Window->PredictionKey : 0;

			UE_LOG(LogTemp, Warning,
				TEXT("SGM_COLLISION_ACCEPT Owner=%s Target=%s Key=%d"),
				*GetNameSafe(OwnerActor),
				*GetNameSafe(HitActor),
				PredictionKey);

			HandlePredictedCollisionHit(OwnerActor, HitActor, Hit, PredictionKey);
		}
	}
}

void USGM_PredictedCollisionNotifyState::DrawDebugSweep(UWorld* World, const FVector& StepStart, const FVector& StepEnd,
	const FQuat& Rotation, const FColor& Color) const
{
	if (!World) return;

	constexpr float DrawTime = 0.25f;

	DrawDebugLine(World, StepStart, StepEnd, Color, false, DrawTime, 0, 1.5f);

	switch (CollisionShape)
	{
	case ESGM_PredictedCollisionShape::Sphere:
		DrawDebugSphere(World, StepStart, FMath::Max(SphereRadius, 1.0f), 16, Color, false, DrawTime);
		DrawDebugSphere(World, StepEnd, FMath::Max(SphereRadius, 1.0f), 16, Color, false, DrawTime);
		break;

	case ESGM_PredictedCollisionShape::Capsule:
		DrawDebugCapsule(World, StepStart, FMath::Max(CapsuleHalfHeight, 1.0f), FMath::Max(CapsuleRadius, 1.0f), Rotation, Color, false, DrawTime);
		DrawDebugCapsule(World, StepEnd, FMath::Max(CapsuleHalfHeight, 1.0f), FMath::Max(CapsuleRadius, 1.0f), Rotation, Color, false, DrawTime);
		break;

	case ESGM_PredictedCollisionShape::Box:
	default:
		DrawDebugBox(World, StepStart, BoxExtent.ComponentMax(FVector(1.0f)), Rotation, Color, false, DrawTime);
		DrawDebugBox(World, StepEnd, BoxExtent.ComponentMax(FVector(1.0f)), Rotation, Color, false, DrawTime);
		break;
	}
}

bool USGM_PredictedCollisionNotifyState::HasAlreadyProcessedTarget(const USkeletalMeshComponent* MeshComp,
	const AActor* TargetActor) const
{
	if (!MeshComp || !TargetActor) return true;

	const FSGM_PredictedCollisionRuntimeWindow* Window = ActiveWindowsByMesh.Find(MeshComp);
	if (!Window) return false;

	return Window->ProcessedTargets.Contains(TargetActor);
}

void USGM_PredictedCollisionNotifyState::MarkTargetProcessed(USkeletalMeshComponent* MeshComp, AActor* TargetActor)
{
	if (!MeshComp || !TargetActor) return;

	FSGM_PredictedCollisionRuntimeWindow& Window = ActiveWindowsByMesh.FindOrAdd(MeshComp);
	Window.ProcessedTargets.Add(TargetActor);
}
