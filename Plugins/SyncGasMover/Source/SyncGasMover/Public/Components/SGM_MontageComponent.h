#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SGM_MontageComponent.generated.h"

class UAnimMontage;
class UMoverComponent;
class UPrimitiveComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct FSGMRepMontageState
{
	GENERATED_BODY()

	// Montage currently being replicated.
	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage = nullptr;

	// Incremented every time we start/stop.
	// This makes replaying the same montage replicate as a new event.
	UPROPERTY()
	int32 Serial = 0;

	UPROPERTY()
	float PlayRate = 1.0f;

	UPROPERTY()
	float StartTimeSeconds = 0.0f;

	UPROPERTY()
	FName StartSection = NAME_None;

	UPROPERTY()
	bool bIsPlaying = false;
	
	// Incremented when root motion is disabled for the current montage.
	UPROPERTY()
	int32 DisableRootMotionSerial = 0;

	UPROPERTY()
	bool bRootMotionDisabled = false;

	// Incremented when the current montage root-motion contribution changes without stopping the montage.
	UPROPERTY()
	int32 RootMotionScaleSerial = 0;

	// 1 = normal montage root motion through Mover. 0 = montage provider stays alive but contributes no movement.
	UPROPERTY()
	float RootMotionScale = 1.0f;
};

UCLASS(ClassGroup = (SyncGasMover), meta = (BlueprintSpawnableComponent))
class SYNCGASMOVER_API USGM_MontageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USGM_MontageComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	// Plays immediately on this machine. Use this for the owning client/server.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool PlayMontageLocal(UAnimMontage* InMontage, float InPlayRate = 1.0f,
		float InStartTimeSeconds = 0.0f, FName InStartSection = NAME_None);
	
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool StopMontageLocal(UAnimMontage* InMontage);

	// Visual-only montage play. This does not queue Mover root motion.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool PlayMontageVisualOnlyLocal(UAnimMontage* InMontage, float InPlayRate = 1.0f,
		float InStartTimeSeconds = 0.0f, FName InStartSection = NAME_None);

	// High-level play path for abilities/Blueprints:
	// play immediately here, then replicate from the server to other clients.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool PlayPredictedReplicatedMontage(UAnimMontage* InMontage, float InPlayRate = 1.0f,
		float InStartTimeSeconds = 0.0f, FName InStartSection = NAME_None);
	
	// Keeps the montage playing, but stops this montage instance from extracting root motion.
	// Use this when an attack should transition from root-motion movement back to locomotion.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool DisableRootMotionForMontage(UAnimMontage* InMontage);
	
	// Server-side command: disables root motion locally and replicates that disable to clients.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool DisableRootMotionForReplicatedMontage();

	// High-level disable path for abilities/Blueprints:
	// disable locally now, then replicate/request the same disable from the server.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool DisableRootMotionPredictedReplicated();

	// Begin watching the active replicated montage and release root motion after a percent of the montage has played.
	// Example: 0.70 means release at 70% montage progress.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Root Motion")
	void StartRootMotionReleaseAtMontagePercent(float ReleasePercent);

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Root Motion")
	void ClearRootMotionRelease();

	// Enables/disables capsule contact-based root-motion pausing while the montage is still in its root-motion phase.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Root Motion")
	void SetRootMotionContactBlockingEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Root Motion")
	bool IsRootMotionContactBlocked() const { return bRootMotionBlockedByContact; }

	// Replicated flag for AnimBP upper/lower body blending after root-motion release.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Animation")
	void SetCanBlendUpperAndLowerBody(bool bInCanBlend);

	UFUNCTION(BlueprintPure, Category = "SyncGasMover|Animation")
	bool GetCanBlendUpperAndLowerBody() const { return bCanBlendUpperAndLowerBody; }
	
	// Server-side command: replicate a montage play event to simulated clients.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool StartReplicatedMontage(UAnimMontage* InMontage, float InPlayRate = 1.0f,
		float InStartTimeSeconds = 0.0f, FName InStartSection = NAME_None);

	// Server-side command: replicate a montage stop event to simulated clients.
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	void StopReplicatedMontage();

	UFUNCTION(Server, Reliable)
	void ServerPlayReplicatedMontage(UAnimMontage* InMontage, float InPlayRate,
		float InStartTimeSeconds, FName InStartSection);

	UFUNCTION(Server, Reliable)
	void ServerDisableRootMotionForReplicatedMontage();

	UFUNCTION(Server, Reliable)
	void ServerSetCanBlendUpperAndLowerBody(bool bInCanBlend);

	UFUNCTION(Server, Reliable)
	void ServerSetReplicatedRootMotionScale(float InRootMotionScale);

protected:
	virtual void BeginPlay() override;
	
	// Mesh that owns the AnimInstance where montages play.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage", meta = (UseComponentPicker))
	FComponentReference MontageMeshComponentReference;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> MontageMeshComponent = nullptr;

	// Replicated montage command state.
	UPROPERTY(ReplicatedUsing = OnRep_RepMontageState)
	FSGMRepMontageState RepMontageState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root Motion|Contact")
	bool bEnableRootMotionContactBlocking = false;

	// Half-angle of the front collision acceptance cone. 40 means 40 degrees left/right.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root Motion|Contact", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ContactBlockHalfAngleDegrees = 40.0f;

	UPROPERTY(ReplicatedUsing = OnRep_CanBlendUpperAndLowerBody, BlueprintReadOnly, Category = "Animation")
	bool bCanBlendUpperAndLowerBody = false;
	
	UFUNCTION()
	void OnRep_RepMontageState();

	UFUNCTION()
	void OnRep_CanBlendUpperAndLowerBody();

private:
	void ResolveMeshComponent();
	UMoverComponent* GetMoverComponent() const;
	UCapsuleComponent* GetOwnerCapsuleComponent() const;
	void QueueRootMotionMove(UAnimMontage* InMontage, float InPlayRate, float InStartingMontagePosition,
		float InRootMotionScale = 1.0f);
	
	void UpdateRootMotionControl(float DeltaSeconds);
	void UpdateMontagePercentRelease();
	void UpdateBlockedContactResume();
	bool ShouldDrivePredictedRootMotionControl() const;
	bool IsActorWithinContactBlockAngle(const AActor* OtherActor) const;
	void RefreshInitialContactBlockState();
	void SetContactRootMotionBlocked(bool bInBlocked);
	void BindContactBlockingEvents();
	void UnbindContactBlockingEvents();
	bool ApplyRootMotionScaleToCurrentMontage(float InRootMotionScale);
	void SetReplicatedRootMotionScale(float InRootMotionScale);
	void ResetLocalRootMotionControlState();

	UFUNCTION()
	void OnOwnerCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOwnerCapsuleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnOwnerCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	// Tracks which replicated play/stop command this client already applied.
	int32 LastAppliedMontageSerial = INDEX_NONE;

	// Tracks which replicated root-motion-disable command this client already applied.
	int32 LastAppliedDisableRootMotionSerial = INDEX_NONE;

	// Tracks which replicated root-motion scale command this client already applied.
	int32 LastAppliedRootMotionScaleSerial = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> BoundContactCapsule = nullptr;

	TSet<TWeakObjectPtr<AActor>> ContactBlockingActors;

	float RootMotionReleasePercent = -1.0f;
	bool bRootMotionReleasedByPercent = false;
	bool bRootMotionBlockedByContact = false;
};
