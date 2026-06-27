#include "Components/SGM_MontageComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "LayeredMoves/SGM_AnimRootMotionLayeredMove.h"
#include "MoverComponent.h"
#include "Net/UnrealNetwork.h"
#include "SyncGasMover.h"
#include "Tags/SGM_NativeTags.h"

namespace SyncGasMoverLog
{
	const TCHAR* RoleToString(const ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_Authority:
			return TEXT("Authority");
		case ROLE_AutonomousProxy:
			return TEXT("AutonomousProxy");
		case ROLE_SimulatedProxy:
			return TEXT("SimulatedProxy");
		case ROLE_None:
		default:
			return TEXT("None");
		}
	}

	const TCHAR* NetModeToString(const ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Client:
			return TEXT("Client");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Standalone:
		default:
			return TEXT("Standalone");
		}
	}

	FString OwnerContext(const UActorComponent* Component)
	{
		const AActor* Owner = Component ? Component->GetOwner() : nullptr;
		const UWorld* World = Component ? Component->GetWorld() : nullptr;
		return FString::Printf(TEXT("Owner=%s Role=%s NetMode=%s"),
			Owner ? *Owner->GetName() : TEXT("None"),
			Owner ? RoleToString(Owner->GetLocalRole()) : TEXT("None"),
			World ? NetModeToString(World->GetNetMode()) : TEXT("None"));
	}

	FString MontageName(const UAnimMontage* Montage)
	{
		return Montage ? Montage->GetName() : TEXT("None");
	}
}


USGM_MontageComponent::USGM_MontageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// This component owns replicated montage commands.
	SetIsReplicatedByDefault(true);
}

void USGM_MontageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(USGM_MontageComponent, RepMontageState);
}

void USGM_MontageComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveMeshComponent();

	UE_LOG(LogSyncGasMover, Log, TEXT("BeginPlay: %s Mesh=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		MontageMeshComponent ? *MontageMeshComponent->GetName() : TEXT("None"));
}

bool USGM_MontageComponent::PlayMontageLocal(UAnimMontage* InMontage, float InPlayRate, float InStartTimeSeconds,
	FName InStartSection)
{
	UE_LOG(LogSyncGasMover, Warning, TEXT("PlayMontageLocal requested: %s Montage=%s PlayRate=%.3f StartTime=%.3f Section=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage),
		InPlayRate,
		InStartTimeSeconds,
		*InStartSection.ToString());

	if (!InMontage)
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("PlayMontageLocal failed: Montage is null. %s"),
			*SyncGasMoverLog::OwnerContext(this));
		return false;
	}

	ResolveMeshComponent();

	if (!MontageMeshComponent)
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("PlayMontageLocal failed: Mesh is null. %s Montage=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			*SyncGasMoverLog::MontageName(InMontage));
		return false;
	}

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("PlayMontageLocal failed: AnimInstance is null. %s Mesh=%s Montage=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			*MontageMeshComponent->GetName(),
			*SyncGasMoverLog::MontageName(InMontage));
		return false;
	}

	const float MontageLength = InMontage->GetPlayLength();
	const float ClampedStartTime = FMath::Clamp(
		InStartTimeSeconds,
		0.0f,
		FMath::Max(0.0f, MontageLength - KINDA_SMALL_NUMBER));

	// bStopAllMontages = true because this is a deliberate retrigger/play command.
	// Later we can make this configurable if layered montage slots need different behavior.
	const float PlayedLength = AnimInstance->Montage_Play(InMontage, InPlayRate,
		EMontagePlayReturnType::MontageLength, ClampedStartTime, true);

	UE_LOG(LogSyncGasMover, Warning, TEXT("PlayMontageLocal Montage_Play result: %s Montage=%s PlayedLength=%.3f HasRootMotion=%s ClampedStart=%.3f"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage),
		PlayedLength,
		InMontage->HasRootMotion() ? TEXT("true") : TEXT("false"),
		ClampedStartTime);

	if (PlayedLength <= 0.0f) return false;

	if (InStartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(InStartSection, InMontage);
	}

	if (InPlayRate != 0.0f && InMontage->HasRootMotion())
	{
		if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(InMontage))
		{
			UE_LOG(LogSyncGasMover, Warning, TEXT("PlayMontageLocal disabling montage root motion and queueing Mover root motion: %s Montage=%s Position=%.3f"),
				*SyncGasMoverLog::OwnerContext(this),
				*SyncGasMoverLog::MontageName(InMontage),
				MontageInstance->GetPosition());

			// Mover should own the movement. The montage keeps only the visual pose.
			MontageInstance->PushDisableRootMotion();
			QueueRootMotionMove(InMontage, InPlayRate, MontageInstance->GetPosition());
		}
		else
		{
			UE_LOG(LogSyncGasMover, Warning, TEXT("PlayMontageLocal could not find active montage instance for root motion: %s Montage=%s"),
				*SyncGasMoverLog::OwnerContext(this),
				*SyncGasMoverLog::MontageName(InMontage));
		}
	}

	return true;
}

bool USGM_MontageComponent::StopMontageLocal(UAnimMontage* InMontage)
{
	if (!InMontage) return false;

	ResolveMeshComponent();

	if (!MontageMeshComponent) return false;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(InMontage)) return false;

	// Stop only this montage. Do not kill unrelated montage slots.
	AnimInstance->Montage_Stop(InMontage->GetDefaultBlendOutTime(), InMontage);
	return true;
}

bool USGM_MontageComponent::PlayMontageVisualOnlyLocal(UAnimMontage* InMontage, float InPlayRate,
	float InStartTimeSeconds, FName InStartSection)
{
	if (!InMontage) return false;

	ResolveMeshComponent();

	if (!MontageMeshComponent) return false;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance) return false;

	const float MontageLength = InMontage->GetPlayLength();
	const float ClampedStartTime = FMath::Clamp(
		InStartTimeSeconds,
		0.0f,
		FMath::Max(0.0f, MontageLength - KINDA_SMALL_NUMBER));

	const float PlayedLength = AnimInstance->Montage_Play(InMontage, InPlayRate,
		EMontagePlayReturnType::MontageLength, ClampedStartTime, true);

	if (PlayedLength <= 0.0f) return false;

	if (InStartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(InStartSection, InMontage);
	}

	if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(InMontage))
	{
		// The montage should keep posing only; movement has returned to Mover locomotion.
		MontageInstance->PushDisableRootMotion();
	}

	return true;
}

bool USGM_MontageComponent::PlayPredictedReplicatedMontage(UAnimMontage* InMontage, float InPlayRate,
	float InStartTimeSeconds, FName InStartSection)
{
	UE_LOG(LogSyncGasMover, Warning, TEXT("PlayPredictedReplicatedMontage requested: %s Montage=%s PlayRate=%.3f StartTime=%.3f Section=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage),
		InPlayRate,
		InStartTimeSeconds,
		*InStartSection.ToString());

	if (!InMontage) return false;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return false;

	if (OwnerActor->HasAuthority())
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("PlayPredictedReplicatedMontage authority path: %s Montage=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			*SyncGasMoverLog::MontageName(InMontage));

		// Server path plays locally once and writes replicated state for simulated clients.
		return StartReplicatedMontage(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
	}

	// Client path plays immediately for prediction, then asks the server to replicate.
	const bool bPlayedLocally = PlayMontageLocal(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
	if (!bPlayedLocally)
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("PlayPredictedReplicatedMontage local prediction failed: %s Montage=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			*SyncGasMoverLog::MontageName(InMontage));
		return false;
	}

	UE_LOG(LogSyncGasMover, Warning, TEXT("PlayPredictedReplicatedMontage sending server RPC: %s Montage=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage));
	ServerPlayReplicatedMontage(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
	return true;
}

bool USGM_MontageComponent::DisableRootMotionForMontage(UAnimMontage* InMontage)
{
	UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionForMontage requested: %s Montage=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage));

	if (!InMontage) return false;

	ResolveMeshComponent();

	if (!MontageMeshComponent) return false;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance) return false;

	FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(InMontage);
	if (!MontageInstance)
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionForMontage failed: active montage instance missing. %s Montage=%s IsPlaying=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			*SyncGasMoverLog::MontageName(InMontage),
			AnimInstance->Montage_IsPlaying(InMontage) ? TEXT("true") : TEXT("false"));
		return false;
	}

	UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionForMontage active instance found: %s Montage=%s Position=%.3f IsPlaying=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage),
		MontageInstance->GetPosition(),
		AnimInstance->Montage_IsPlaying(InMontage) ? TEXT("true") : TEXT("false"));

	// This is the important part:
	// the montage keeps playing visually, but root motion extraction stops.
	MontageInstance->PushDisableRootMotion();

	if (GetMoverComponent())
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionForMontage queueing zero-root-motion montage provider: %s Montage=%s Position=%.3f Tag=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			*SyncGasMoverLog::MontageName(InMontage),
			MontageInstance->GetPosition(),
			*TAG_SyncGasMover_RootMotion.GetTag().ToString());

		// Keep Mover's montage provider alive, but stop contributing root motion.
		QueueRootMotionMove(InMontage, AnimInstance->Montage_GetPlayRate(InMontage), MontageInstance->GetPosition(), 0.0f);
	}
	else
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionForMontage no MoverComponent found: %s Montage=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			*SyncGasMoverLog::MontageName(InMontage));
	}

	return true;
}

bool USGM_MontageComponent::DisableRootMotionPredictedReplicated()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionPredictedReplicated requested: %s RepMontage=%s Serial=%d DisableSerial=%d IsPlaying=%s RootMotionDisabled=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(RepMontageState.Montage),
		RepMontageState.Serial,
		RepMontageState.DisableRootMotionSerial,
		RepMontageState.bIsPlaying ? TEXT("true") : TEXT("false"),
		RepMontageState.bRootMotionDisabled ? TEXT("true") : TEXT("false"));

	const bool bDisabledLocally = RepMontageState.Montage
		? DisableRootMotionForMontage(RepMontageState.Montage)
		: false;

	if (OwnerActor->HasAuthority())
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionPredictedReplicated authority path: %s LocalResult=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			bDisabledLocally ? TEXT("true") : TEXT("false"));

		return DisableRootMotionForReplicatedMontage();
	}

	UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionPredictedReplicated sending server RPC: %s LocalResult=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		bDisabledLocally ? TEXT("true") : TEXT("false"));

	ServerDisableRootMotionForReplicatedMontage();
	return bDisabledLocally;
}

bool USGM_MontageComponent::DisableRootMotionForReplicatedMontage()
{
	UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionForReplicatedMontage requested: %s RepMontage=%s Serial=%d DisableSerial=%d IsPlaying=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(RepMontageState.Montage),
		RepMontageState.Serial,
		RepMontageState.DisableRootMotionSerial,
		RepMontageState.bIsPlaying ? TEXT("true") : TEXT("false"));

	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

	if (!RepMontageState.Montage || !RepMontageState.bIsPlaying) return false;

	const bool bDisabledLocally = DisableRootMotionForMontage(RepMontageState.Montage);
	if (!bDisabledLocally)
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionForReplicatedMontage failed locally on authority: %s RepMontage=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			*SyncGasMoverLog::MontageName(RepMontageState.Montage));
		return false;
	}

	RepMontageState.bRootMotionDisabled = true;

	// Serial makes repeated disable events replicate even if the montage did not change.
	RepMontageState.DisableRootMotionSerial++;

	UE_LOG(LogSyncGasMover, Warning, TEXT("DisableRootMotionForReplicatedMontage wrote replicated state: %s RepMontage=%s Serial=%d DisableSerial=%d RootMotionDisabled=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(RepMontageState.Montage),
		RepMontageState.Serial,
		RepMontageState.DisableRootMotionSerial,
		RepMontageState.bRootMotionDisabled ? TEXT("true") : TEXT("false"));

	return true;
}

bool USGM_MontageComponent::StartReplicatedMontage(UAnimMontage* InMontage, float InPlayRate,
                                                   float InStartTimeSeconds, FName InStartSection)
{
	UE_LOG(LogSyncGasMover, Warning, TEXT("StartReplicatedMontage requested: %s Montage=%s PlayRate=%.3f StartTime=%.3f Section=%s OldSerial=%d"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage),
		InPlayRate,
		InStartTimeSeconds,
		*InStartSection.ToString(),
		RepMontageState.Serial);

	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	if (!InMontage) return false;

	const bool bPlayedLocally = PlayMontageLocal(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
	if (!bPlayedLocally)
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("StartReplicatedMontage failed to play locally on authority: %s Montage=%s"),
			*SyncGasMoverLog::OwnerContext(this),
			*SyncGasMoverLog::MontageName(InMontage));
		return false;
	}

	RepMontageState.Montage = InMontage;
	RepMontageState.PlayRate = InPlayRate;
	RepMontageState.StartTimeSeconds = InStartTimeSeconds;
	RepMontageState.StartSection = InStartSection;
	RepMontageState.bIsPlaying = true;
	RepMontageState.bRootMotionDisabled = false;

	// Critical: this changes even when the same montage is played again.
	// That forces clients to treat it as a fresh play command.
	RepMontageState.Serial++;

	UE_LOG(LogSyncGasMover, Warning, TEXT("StartReplicatedMontage wrote replicated state: %s Montage=%s Serial=%d DisableSerial=%d IsPlaying=%s RootMotionDisabled=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(RepMontageState.Montage),
		RepMontageState.Serial,
		RepMontageState.DisableRootMotionSerial,
		RepMontageState.bIsPlaying ? TEXT("true") : TEXT("false"),
		RepMontageState.bRootMotionDisabled ? TEXT("true") : TEXT("false"));

	return true;
}

void USGM_MontageComponent::ServerPlayReplicatedMontage_Implementation(UAnimMontage* InMontage, float InPlayRate,
	float InStartTimeSeconds, FName InStartSection)
{
	UE_LOG(LogSyncGasMover, Warning, TEXT("ServerPlayReplicatedMontage RPC received: %s Montage=%s PlayRate=%.3f StartTime=%.3f Section=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage),
		InPlayRate,
		InStartTimeSeconds,
		*InStartSection.ToString());

	// In a predicted GAS ability, the owning client runs the ability immediately,
	// and the server runs the same ability authoritatively. The client's montage RPC
	// can arrive after the server ability already called PlayPredictedReplicatedMontage.
	// Treat that late RPC as confirmation, not as a second montage start.
	if (RepMontageState.bIsPlaying
		&& RepMontageState.Montage == InMontage
		&& FMath::IsNearlyEqual(RepMontageState.PlayRate, InPlayRate)
		&& FMath::IsNearlyEqual(RepMontageState.StartTimeSeconds, InStartTimeSeconds)
		&& RepMontageState.StartSection == InStartSection)
	{
		ResolveMeshComponent();

		float CurrentServerMontagePosition = 0.0f;
		bool bServerAlreadyPlayingSameMontage = false;

		if (MontageMeshComponent)
		{
			if (UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance())
			{
				if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(InMontage))
				{
					bServerAlreadyPlayingSameMontage = true;
					CurrentServerMontagePosition = MontageInstance->GetPosition();
				}
			}
		}

		constexpr float DuplicatePredictedPlayRejectWindowSeconds = 0.75f;
		if (bServerAlreadyPlayingSameMontage && CurrentServerMontagePosition <= DuplicatePredictedPlayRejectWindowSeconds)
		{
			UE_LOG(LogSyncGasMover, Warning, TEXT("ServerPlayReplicatedMontage duplicate predicted RPC ignored: %s Montage=%s Serial=%d Position=%.3f"),
				*SyncGasMoverLog::OwnerContext(this),
				*SyncGasMoverLog::MontageName(InMontage),
				RepMontageState.Serial,
				CurrentServerMontagePosition);
			return;
		}
	}

	StartReplicatedMontage(InMontage, InPlayRate, InStartTimeSeconds, InStartSection);
}

void USGM_MontageComponent::ServerDisableRootMotionForReplicatedMontage_Implementation()
{
	UE_LOG(LogSyncGasMover, Warning, TEXT("ServerDisableRootMotionForReplicatedMontage RPC received: %s RepMontage=%s Serial=%d DisableSerial=%d"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(RepMontageState.Montage),
		RepMontageState.Serial,
		RepMontageState.DisableRootMotionSerial);

	DisableRootMotionForReplicatedMontage();
}

void USGM_MontageComponent::StopReplicatedMontage()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	StopMontageLocal(RepMontageState.Montage);

	RepMontageState.bIsPlaying = false;

	// Critical: stop also needs a serial so clients receive same-montage stop/replay sequences.
	RepMontageState.Serial++;
}

void USGM_MontageComponent::OnRep_RepMontageState()
{
	UE_LOG(LogSyncGasMover, Warning, TEXT("OnRep_RepMontageState received: %s RepMontage=%s Serial=%d LastSerial=%d DisableSerial=%d LastDisableSerial=%d IsPlaying=%s RootMotionDisabled=%s PlayRate=%.3f StartTime=%.3f Section=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(RepMontageState.Montage),
		RepMontageState.Serial,
		LastAppliedMontageSerial,
		RepMontageState.DisableRootMotionSerial,
		LastAppliedDisableRootMotionSerial,
		RepMontageState.bIsPlaying ? TEXT("true") : TEXT("false"),
		RepMontageState.bRootMotionDisabled ? TEXT("true") : TEXT("false"),
		RepMontageState.PlayRate,
		RepMontageState.StartTimeSeconds,
		*RepMontageState.StartSection.ToString());

	ResolveMeshComponent();

	if (!MontageMeshComponent) return;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance) return;

	const bool bHasNewMontageCommand = LastAppliedMontageSerial != RepMontageState.Serial;
	if (bHasNewMontageCommand)
	{
		LastAppliedMontageSerial = RepMontageState.Serial;

		if (!RepMontageState.bIsPlaying)
		{
			if (RepMontageState.Montage && AnimInstance->Montage_IsPlaying(RepMontageState.Montage))
			{
				UE_LOG(LogSyncGasMover, Warning, TEXT("OnRep applying stop command: %s Montage=%s"),
					*SyncGasMoverLog::OwnerContext(this),
					*SyncGasMoverLog::MontageName(RepMontageState.Montage));

				AnimInstance->Montage_Stop(RepMontageState.Montage->GetDefaultBlendOutTime(),
					RepMontageState.Montage);
			}

			return;
		}

		if (RepMontageState.Montage)
		{
			const AActor* OwnerActor = GetOwner();
			const bool bIsAutonomousProxy = OwnerActor && OwnerActor->GetLocalRole() == ROLE_AutonomousProxy;
			const bool bAlreadyPlayingSameMontage = AnimInstance->Montage_IsPlaying(RepMontageState.Montage);

			if (bIsAutonomousProxy && bAlreadyPlayingSameMontage)
			{
				UE_LOG(LogSyncGasMover, Warning, TEXT("OnRep confirming predicted play command without replay: %s Montage=%s Serial=%d"),
					*SyncGasMoverLog::OwnerContext(this),
					*SyncGasMoverLog::MontageName(RepMontageState.Montage),
					RepMontageState.Serial);
			}
			else
			{
				UE_LOG(LogSyncGasMover, Warning, TEXT("OnRep applying play command: %s Montage=%s Serial=%d"),
					*SyncGasMoverLog::OwnerContext(this),
					*SyncGasMoverLog::MontageName(RepMontageState.Montage),
					RepMontageState.Serial);

				PlayMontageLocal(RepMontageState.Montage, RepMontageState.PlayRate,
					RepMontageState.StartTimeSeconds, RepMontageState.StartSection);
			}
		}
	}

	const bool bHasNewDisableRootMotionCommand =
		LastAppliedDisableRootMotionSerial != RepMontageState.DisableRootMotionSerial;

	if (bHasNewDisableRootMotionCommand)
	{
		LastAppliedDisableRootMotionSerial = RepMontageState.DisableRootMotionSerial;

		if (RepMontageState.bRootMotionDisabled && RepMontageState.Montage)
		{
			float CurrentMontagePosition = RepMontageState.StartTimeSeconds;
			if (UAnimInstance* CurrentAnimInstance = MontageMeshComponent->GetAnimInstance())
			{
				if (FAnimMontageInstance* MontageInstance =
					CurrentAnimInstance->GetActiveInstanceForMontage(RepMontageState.Montage))
				{
					CurrentMontagePosition = MontageInstance->GetPosition();
				}
			}

			UE_LOG(LogSyncGasMover, Warning, TEXT("OnRep applying disable-root-motion command: %s Montage=%s PositionBeforeDisable=%.3f DisableSerial=%d"),
				*SyncGasMoverLog::OwnerContext(this),
				*SyncGasMoverLog::MontageName(RepMontageState.Montage),
				CurrentMontagePosition,
				RepMontageState.DisableRootMotionSerial);

			DisableRootMotionForMontage(RepMontageState.Montage);
		}
	}
}

void USGM_MontageComponent::ResolveMeshComponent()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	MontageMeshComponent = Cast<USkeletalMeshComponent>(
		MontageMeshComponentReference.GetComponent(OwnerActor));

	if (!MontageMeshComponent)
	{
		// Default fallback: use the first skeletal mesh on the owner.
		MontageMeshComponent = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	}
}

UMoverComponent* USGM_MontageComponent::GetMoverComponent() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->FindComponentByClass<UMoverComponent>() : nullptr;
}

void USGM_MontageComponent::QueueRootMotionMove(UAnimMontage* InMontage, float InPlayRate,
	float InStartingMontagePosition, float InRootMotionScale)
{
	UMoverComponent* MoverComponent = GetMoverComponent();
	if (!MoverComponent || !InMontage || InPlayRate == 0.0f)
	{
		UE_LOG(LogSyncGasMover, Warning, TEXT("QueueRootMotionMove skipped: %s Mover=%s Montage=%s PlayRate=%.3f StartPosition=%.3f Scale=%.3f"),
			*SyncGasMoverLog::OwnerContext(this),
			MoverComponent ? TEXT("Valid") : TEXT("None"),
			*SyncGasMoverLog::MontageName(InMontage),
			InPlayRate,
			InStartingMontagePosition,
			InRootMotionScale);
		return;
	}

	// Cancel previous SGM root motion when retriggering the same montage.
	UE_LOG(LogSyncGasMover, Warning, TEXT("QueueRootMotionMove canceling previous root-motion feature: %s Montage=%s Tag=%s"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage),
		*TAG_SyncGasMover_RootMotion.GetTag().ToString());
	MoverComponent->CancelFeaturesWithTag(TAG_SyncGasMover_RootMotion, true);

	TSharedPtr<FSGM_AnimRootMotionLayeredMove> AnimRootMotionMove = MakeShared<FSGM_AnimRootMotionLayeredMove>();
	AnimRootMotionMove->MontageState.Montage = InMontage;
	AnimRootMotionMove->MontageState.PlayRate = InPlayRate;
	AnimRootMotionMove->MontageState.StartingMontagePosition = InStartingMontagePosition;
	AnimRootMotionMove->MontageState.CurrentPosition = InStartingMontagePosition;
	AnimRootMotionMove->RootMotionScale = InRootMotionScale;
	AnimRootMotionMove->MixMode = FMath::IsNearlyZero(InRootMotionScale)
		? EMoveMixMode::AdditiveVelocity
		: EMoveMixMode::OverrideAll;
	AnimRootMotionMove->bIgnoreRetriggerCancellationWhileQueued = true;

	const float RemainingUnscaledMontageSeconds = InPlayRate > 0.0f
		? InMontage->GetPlayLength() - InStartingMontagePosition
		: InStartingMontagePosition;

	AnimRootMotionMove->DurationMs = (RemainingUnscaledMontageSeconds / FMath::Abs(InPlayRate)) * 1000.0f;

	UE_LOG(LogSyncGasMover, Warning, TEXT("QueueRootMotionMove queueing layered move: %s Montage=%s PlayRate=%.3f StartPosition=%.3f DurationMs=%.3f Scale=%.3f MixMode=%d"),
		*SyncGasMoverLog::OwnerContext(this),
		*SyncGasMoverLog::MontageName(InMontage),
		InPlayRate,
		InStartingMontagePosition,
		AnimRootMotionMove->DurationMs,
		InRootMotionScale,
		static_cast<int32>(AnimRootMotionMove->MixMode));

	MoverComponent->QueueLayeredMove(AnimRootMotionMove);
}
