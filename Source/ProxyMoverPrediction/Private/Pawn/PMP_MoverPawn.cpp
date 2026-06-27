#include "ProxyMoverPrediction/Public/Pawn/PMP_MoverPawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SGM_MontageComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GameFramework/PlayerController.h"
#include "MoverDataModelTypes.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"

APMP_MoverPawn::APMP_MoverPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// The pawn exists on server and clients.
	SetReplicates(true);
	
	// Important: do not use normal Actor movement replication.
	// Mover has its own network prediction / correction path.
	SetReplicatingMovement(false);
	
	// This is the movement brain. Later we will tell it which component to move.
	MoverComponent = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("MoverComponent"));

	// This is the body that collides with the world and other pawns.
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	// The skeletal mesh follows the capsule. It should not participate in movement collision.
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CapsuleComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// Standard UE mannequin-style offset. We can adjust after assigning a mesh.
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	MeshComponent->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
}

void APMP_MoverPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	if (!MoverComponent) return;
	
	// This tells Mover which scene component it is allowed to simulate.
	// For a character-style pawn, that should be the collision capsule.
	MoverComponent->SetUpdatedComponent(CapsuleComponent);
	
	// This tells Mover which component is visual-only.
	// Mover can use it for visual smoothing without treating it as collision.
	MoverComponent->SetPrimaryVisualComponent(MeshComponent);
}

void APMP_MoverPawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	FCharacterDefaultInputs& CharacterInputs = 
		InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	
	// Default to no facing change. We only set facing when there is real movement input.
	CharacterInputs.OrientationIntent = FVector::ZeroVector;
	
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	
	if (!PlayerController)
	{
		// Non-Player controllers can still provide already-world-space movement intent later.
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, CachedMoveInputIntent);
		return;
	}
	
	const FRotator ControlRotation = PlayerController->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
	
	// Cached input is local input, like X/Y from WASD.
	// Rotating by controller yaw makes forward mean camera-forward.
	const FVector WorldMoveIntent = YawRotation.RotateVector(CachedMoveInputIntent);
	
	CharacterInputs.ControlRotation = ControlRotation;

	// This is the actual movement command Mover consumes.
	// Without this line, the pawn has input cached but Mover receives no movement.
	CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMoveIntent);
	
	// This gives strafe-style movement: move relative to camera, face camera yaw while moving.
	if (WorldMoveIntent.SizeSquared2D() > 0.01f)
	{
		CharacterInputs.OrientationIntent = YawRotation.Vector();
	}
}

void APMP_MoverPawn::RequestMoveIntent(const FVector& MoveIntent)
{
	if (USGM_MontageComponent* MontageComponent = FindComponentByClass<USGM_MontageComponent>())
	{
		if (MontageComponent->ShouldBlockMovementInputDuringRootMotion())
		{
			CachedMoveInputIntent = FVector::ZeroVector;
			return;
		}

		if (!MoveIntent.IsNearlyZero())
		{
			MontageComponent->TryReleaseRootMotionForMovementInput();
		}
	}

	// Store the latest input direction. We do not move here.
	// Movement happens only when Mover asks for input during simulation.
	CachedMoveInputIntent = MoveIntent;
}

void APMP_MoverPawn::ClearMoveIntent()
{
	// Zero intent means Mover should stop accelerating from player input.
	CachedMoveInputIntent = FVector::ZeroVector;
}

void APMP_MoverPawn::BeginPlay()
{
	Super::BeginPlay();
	
	// Shared settings are created by the Mover component.
	// We edit them at runtime so this C++ pawn has sane movement defaults.
	ApplyDefaultMovementSettings();
}

void APMP_MoverPawn::ApplyDefaultMovementSettings()
{
	if (!MoverComponent) return;
	
	UCommonLegacyMovementSettings* MovementSettings = 
		MoverComponent->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();
	
	if (!MovementSettings) return;
	
	MovementSettings->MaxSpeed = 400.0f;
	MovementSettings->Acceleration = 4000.0f;
	MovementSettings->Deceleration = 4000.0f;
	MovementSettings->TurningRate = 720.0f;
	MovementSettings->TurningBoost = 2.0f;
}
