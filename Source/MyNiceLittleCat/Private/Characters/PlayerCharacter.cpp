// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/PlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"

#include "Characters/GAS/PlayerAttributeSet.h"
#include "Characters/GAS/GA_MeleeAttack.h"
#include "Characters/PlayerComponents/LockOnComponent.h"

// Sets default values
APlayerCharacter::APlayerCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 1080.f, 0.f);

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

    // GAS
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AttributeSet = CreateDefaultSubobject<UPlayerAttributeSet>(TEXT("AttributeSet"));

}

UAbilitySystemComponent* APlayerCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
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

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    if (AbilitySystemComponent && MeleeAttackAbilityClass)
    {
        AbilitySystemComponent->GiveAbility(
            FGameplayAbilitySpec(MeleeAttackAbilityClass, 1)
        );
    }

}

// Called every frame
void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (!IsValid(LockOnComponent) || !LockOnComponent->IsLockedOn()) return;

    AActor* Target = LockOnComponent->GetLockedTarget();
    if (!IsValid(Target)) return;

    FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
    ToTarget.Z = 0.0f;
    if (ToTarget.IsNearlyZero()) return;

    const float TargetYaw = ToTarget.Rotation().Yaw;

    const float DeltaYaw = FMath::FindDeltaAngleDegrees(
        GetActorRotation().Yaw,
        TargetYaw
    );

    // 상체 회전 한계를 넘으면 몸 전체를 타겟 방향으로 정렬
    if (FMath::Abs(DeltaYaw) > BodyTurnThreshold && !bIsDodging)
    {
        const FRotator DesiredRotation(0.0f, TargetYaw, 0.0f);

        const FRotator SmoothedRotation = FMath::RInterpTo(
            GetActorRotation(),
            DesiredRotation,
            DeltaTime,
            BodyTurnInterpSpeed
        );

        SetActorRotation(SmoothedRotation);
    }
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
	if (IA_Dodge)
	{
		EnhancedInput->BindAction(
			IA_Dodge,
			ETriggerEvent::Started,
			this,
			&APlayerCharacter::OnDodgeInput
		);
	}
    if (IA_MeleeAttack)
    {
        EnhancedInput->BindAction(
            IA_MeleeAttack,
            ETriggerEvent::Started,
            this,
            &APlayerCharacter::OnMeleeAttackInput
        );
    }
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
    if (bIsDodging) return;
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

void APlayerCharacter::StartDodge()
{
    if (bIsDodging || !DodgeMontage || !bIsAbleToDodge) return;

    bIsDodging = true;

    FVector Direction = GetLastMovementInputVector();

    if (Direction.IsNearlyZero())
    {
        Direction = GetActorForwardVector();
    }
    else
    {
        Direction.Z = 0.0f;
    }

	DodgeDirection = Direction.GetSafeNormal();

    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        SetActorRotation(DodgeDirection.Rotation());
        AnimInstance->Montage_Play(DodgeMontage);
    }

    UCharacterMovementComponent* MoveComp = GetCharacterMovement();

    if (MoveComp)
    {
        OriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;

        MoveComp->SetMovementMode(MOVE_Falling);
        MoveComp->Velocity.Z = 300.0f;
    }

    bIsAbleToAttack = false;

    GetWorldTimerManager().SetTimer(
        DodgeUpdateTimer,
        this,
        &APlayerCharacter::UpdateDodge,
        0.02f,
        true
    );

    GetWorldTimerManager().SetTimer(
        DodgeEndTimer,
        this,
        &APlayerCharacter::EndDodge,
        0.52f,
        false
    );
}

void APlayerCharacter::UpdateDodge()
{
    if (!bIsDodging) return;

    constexpr float DodgeSpeed = 1100.0f;

    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    FVector Velocity = MoveComp->Velocity;

    Velocity.X = DodgeDirection.X * DodgeSpeed;
    Velocity.Y = DodgeDirection.Y * DodgeSpeed;

    // Z 속도는 유지해서 살짝 뜨는 동작과 중력 적용
    MoveComp->Velocity = Velocity;
}

void APlayerCharacter::EndDodge()
{
    bIsDodging = false;
	bIsAbleToAttack = true;
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed;
    }
}

void APlayerCharacter::OnDodgeInput(const FInputActionValue& Value)
{
    StartDodge();
}

void APlayerCharacter::OnMeleeAttackInput(const FInputActionValue& Value)
{
    bool bIsMeleeAttackActive = false;

	if (bIsAbleToAttack == false) return;

    for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
    {
        if (Spec.Ability &&
            Spec.Ability->GetClass() == MeleeAttackAbilityClass &&
            Spec.IsActive())
        {
            bIsMeleeAttackActive = true;
            break;
        }
    }

    if (bIsMeleeAttackActive)
    {
        const FGameplayTag ComboInputTag =
            FGameplayTag::RequestGameplayTag(
                FName("Event.Attack.Input"));

        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
            this,
            ComboInputTag,
            FGameplayEventData()
        );

        return;
    }

    AbilitySystemComponent->TryActivateAbilityByClass(MeleeAttackAbilityClass);
}
