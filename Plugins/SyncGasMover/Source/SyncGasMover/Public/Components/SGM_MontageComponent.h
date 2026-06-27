#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SGM_MontageComponent.generated.h"

class UAnimMontage;
class UMoverComponent;
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
};

UCLASS(ClassGroup = (SyncGasMover), meta = (BlueprintSpawnableComponent))
class SYNCGASMOVER_API USGM_MontageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USGM_MontageComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
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
	
	UFUNCTION()
	void OnRep_RepMontageState();

private:
	void ResolveMeshComponent();
	UMoverComponent* GetMoverComponent() const;
	void QueueRootMotionMove(UAnimMontage* InMontage, float InPlayRate, float InStartingMontagePosition,
		float InRootMotionScale = 1.0f);
	
	// Tracks which replicated play/stop command this client already applied.
	int32 LastAppliedMontageSerial = INDEX_NONE;

	// Tracks which replicated root-motion-disable command this client already applied.
	int32 LastAppliedDisableRootMotionSerial = INDEX_NONE;
};
