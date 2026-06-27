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

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage = nullptr;

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
	
	UPROPERTY()
	int32 DisableRootMotionSerial = 0;

	UPROPERTY()
	bool bRootMotionDisabled = false;

	UPROPERTY()
	int32 RootMotionScaleSerial = 0;

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
	
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool PlayMontageLocal(UAnimMontage* InMontage, float InPlayRate = 1.0f,
		float InStartTimeSeconds = 0.0f, FName InStartSection = NAME_None);
	
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool StopMontageLocal(UAnimMontage* InMontage);

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool PlayMontageVisualOnlyLocal(UAnimMontage* InMontage, float InPlayRate = 1.0f,
		float InStartTimeSeconds = 0.0f, FName InStartSection = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool PlayPredictedReplicatedMontage(UAnimMontage* InMontage, float InPlayRate = 1.0f,
		float InStartTimeSeconds = 0.0f, FName InStartSection = NAME_None);
	
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool DisableRootMotionForMontage(UAnimMontage* InMontage);
	
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool DisableRootMotionForReplicatedMontage();

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool DisableRootMotionPredictedReplicated();

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	USkeletalMeshComponent* GetResolvedMontageMeshComponent();

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Root Motion")
	void StartRootMotionReleaseAtMontagePercent(float ReleasePercent);

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Root Motion")
	void ClearRootMotionRelease();

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Root Motion")
	void SetRootMotionContactBlockingEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Root Motion")
	bool IsRootMotionContactBlocked() const { return bRootMotionBlockedByContact; }

	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Animation")
	void SetCanBlendUpperAndLowerBody(bool bInCanBlend);

	UFUNCTION(BlueprintPure, Category = "SyncGasMover|Animation")
	bool GetCanBlendUpperAndLowerBody() const { return bCanBlendUpperAndLowerBody; }
	
	UFUNCTION(BlueprintCallable, Category = "SyncGasMover|Montage")
	bool StartReplicatedMontage(UAnimMontage* InMontage, float InPlayRate = 1.0f,
		float InStartTimeSeconds = 0.0f, FName InStartSection = NAME_None);

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
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage", meta = (UseComponentPicker))
	FComponentReference MontageMeshComponentReference;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> MontageMeshComponent = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_RepMontageState)
	FSGMRepMontageState RepMontageState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root Motion|Contact")
	bool bEnableRootMotionContactBlocking = false;

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
	
	int32 LastAppliedMontageSerial = INDEX_NONE;
	int32 LastAppliedDisableRootMotionSerial = INDEX_NONE;
	int32 LastAppliedRootMotionScaleSerial = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> BoundContactCapsule = nullptr;

	TSet<TWeakObjectPtr<AActor>> ContactBlockingActors;

	float RootMotionReleasePercent = -1.0f;
	bool bRootMotionReleasedByPercent = false;
	bool bRootMotionBlockedByContact = false;
};
