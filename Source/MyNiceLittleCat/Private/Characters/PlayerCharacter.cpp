// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/PlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

// Sets default values
APlayerCharacter::APlayerCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);

	// 카메라 붐
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 700.f;
	CameraBoom->SetRelativeRotation(FRotator(-40.f, 0.f, 0.f));

    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritYaw = false;
    CameraBoom->bInheritRoll = false;

    // 위치 추적 보간
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 8.f;

    //CameraBoom->bUseCameraLagSubstepping = true;
    //CameraBoom->CameraLagMaxTimeStep = 1.f / 60.f;

	// 팔로우 카메라	
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

    // 락온 컴포넌트
    LockOnComponent = CreateDefaultSubobject<ULockOnComponent>(TEXT("LockOnComponent"));

}

// Called when the game starts or when spawned
void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

    APlayerController* PC = Cast<APlayerController>(GetController());

    if (!PC || !IMC) return;
    ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();

	if (!LocalPlayer) return;
    UEnhancedInputLocalPlayerSubsystem* Subsystem =LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

    if (Subsystem)
    {
        Subsystem->AddMappingContext(IMC, 0);
    }

}

// Called every frame
void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput =
        Cast<UEnhancedInputComponent>(PlayerInputComponent);

    if (!EnhancedInput) return;

    if (IA_Move)
    {
        EnhancedInput->BindAction(
            IA_Move,
            ETriggerEvent::Triggered,
            this,
            &APlayerCharacter::Move
        );
    }
	if (IA_TargetLock)
	{
		EnhancedInput->BindAction(
			IA_TargetLock,
			ETriggerEvent::Started,
			this,
			&APlayerCharacter::ToggleTargetLock
		);
	}
	if (IA_TargetSwitch)
	{
		EnhancedInput->BindAction(
			IA_TargetSwitch,
			ETriggerEvent::Triggered,
			this,
			&APlayerCharacter::SwitchTarget
		);
	}
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();

    if (Input.IsNearlyZero()) return;

    // 카메라의 수평 방향을 기준으로 이동 방향 계산
    const FRotator CameraRotation =
        FollowCamera->GetComponentRotation();

    const FRotator YawRotation(
        0.f,
        CameraRotation.Yaw,
        0.f
    );

    const FVector ForwardDirection =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

    const FVector RightDirection =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    AddMovementInput(ForwardDirection, Input.Y);
    AddMovementInput(RightDirection, Input.X);
}

void APlayerCharacter::ToggleTargetLock()
{
    if (LockOnComponent)
    {
        LockOnComponent->ToggleTargetLock();
    }
}

void APlayerCharacter::SwitchTarget(const FInputActionValue& Value)
{
    if (LockOnComponent)
    {
        LockOnComponent->SwitchTarget(Value.Get<float>());
    }
}

