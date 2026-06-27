#include "ProxyMoverPrediction/Public/Pawn/PMP_PlayerMoverPawn.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/PMP_PlayerState.h"

APMP_PlayerMoverPawn::APMP_PlayerMoverPawn()
{
	// The spring arm uses controller rotation, so mouse/gamepad look rotates the camera.
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->SetupAttachment(GetMeshComponent());
	SpringArmComponent->TargetArmLength = 750.0f;
	SpringArmComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	SpringArmComponent->bUsePawnControlRotation = true;

	// The camera itself does not rotate independently; it inherits from the spring arm.
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);
	CameraComponent->bUsePawnControlRotation = false;
}

UAbilitySystemComponent* APMP_PlayerMoverPawn::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APMP_PlayerMoverPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server initializes the ASC actor info when this pawn becomes the avatar.
	InitializeAbilitySystemFromPlayerState();
}

void APMP_PlayerMoverPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Owning client receives PlayerState through replication, then initializes locally.
	InitializeAbilitySystemFromPlayerState();
}

void APMP_PlayerMoverPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController) return;

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (LocalPlayer && InputMappingContext)
	{
		// Enhanced Input mapping contexts are registered per local player.
		// This is what makes your Input Actions actually fire on this client.
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(InputMappingContext, 0);
		}
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent) return;

	if (MoveAction)
	{
		// Triggered updates movement while keys/stick are active.
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this,
			&APMP_PlayerMoverPawn::HandleMoveInput);

		// Completed clears movement when the input returns to zero.
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this,
			&APMP_PlayerMoverPawn::HandleMoveCompleted);
	}

	if (LookAction)
	{
		// Look is normal controller rotation input, not Mover simulation input.
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this,
			&APMP_PlayerMoverPawn::HandleLookInput);
	}
}

void APMP_PlayerMoverPawn::HandleMoveInput(const FInputActionValue& Value)
{
	const FVector2D MoveValue = Value.Get<FVector2D>();

	// Enhanced Input gives us 2D input.
	// X is right/left, Y is forward/back.
	const FVector LocalMoveIntent(MoveValue.Y, MoveValue.X, 0.0f);

	// We do not move here. We cache intent on the base pawn.
	// Mover consumes it later in ProduceInput_Implementation.
	RequestMoveIntent(LocalMoveIntent);
}

void APMP_PlayerMoverPawn::HandleMoveCompleted(const FInputActionValue& Value)
{
	// When input stops, clear cached intent so Mover stops accelerating.
	ClearMoveIntent();
}

void APMP_PlayerMoverPawn::HandleLookInput(const FInputActionValue& Value)
{
	const FVector2D LookValue = Value.Get<FVector2D>();

	// Look is controller rotation, not Mover movement input.
	// The spring arm reads this controller rotation because bUsePawnControlRotation is true.
	AddControllerYawInput(LookValue.X);
	AddControllerPitchInput(LookValue.Y);
}

void APMP_PlayerMoverPawn::InitializeAbilitySystemFromPlayerState()
{
	APMP_PlayerState* PMPPlayerState = GetPlayerState<APMP_PlayerState>();
	if (!PMPPlayerState)
	{
		AbilitySystemComponent = nullptr;
		return;
	}

	AbilitySystemComponent = PMPPlayerState->GetAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	// OwnerActor is PlayerState, AvatarActor is this pawn.
	// That is the standard setup for player-owned GAS on respawnable pawns.
	AbilitySystemComponent->InitAbilityActorInfo(PMPPlayerState, this);
}
