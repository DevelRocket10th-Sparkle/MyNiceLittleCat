// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Enemy/Datas/EnemyConfigData.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "AIController.h"
#include "BrainComponent.h"

// Sets default values
AEnemyCharacter::AEnemyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));

	AttributeSet = CreateDefaultSubobject<UEnemyAttributeSet>(TEXT("Stat"));

}

UAbilitySystemComponent* AEnemyCharacter::GetAbilitySystemComponent() const
{
	return ASC;
}

bool AEnemyCharacter::IsTargetAlive_Implementation() const
{
	return LifeState == EEnemyLifeState::Alive;
}

// Called when the game starts or when spawned
void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(ASC))
	{
		ASC->InitAbilityActorInfo(this, this);	// GAS �����ڿ� ���� �ൿ ������ ��ġ
		
		// Attributes �ʱ�ȭ
		if (EnemyConfig)
		{
			ApplyEnemyConfig();
		}
	}

	// Health ���� ��������Ʈ ����
	AttributeSet->OnHealthChange.AddUObject(this, &AEnemyCharacter::HandleHealthChanged);
	
}

// Called every frame
void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AEnemyCharacter::ApplyEnemyConfig()
{

	ASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetMaxHealthAttribute(),
		EnemyConfig->MaxHealth
	);

	// Health�� MaxHealth�� ���� �״�� ���
	ASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetHealthAttribute(),
		EnemyConfig->MaxHealth
	);

	ASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetAttackPowerAttribute(),
		EnemyConfig->AttackPower
	);

	GetCharacterMovement()->MaxWalkSpeed = EnemyConfig->MoveSpeed;
}

void AEnemyCharacter::BeginDeath()
{
	if (LifeState != EEnemyLifeState::Alive)
		return;

	LifeState = EEnemyLifeState::Dying;	// �ߺ� ������ ���� �ֿ켱 ó�� �ʿ�

	SetWantsToMove(false);

	// AI �ߴ�
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Death"));
		}

		AI->StopMovement();
	}

	// ���� Ability ���
	if (IsValid(ASC))
	{
		ASC->CancelAllAbilities();
	}

	// ��� �� �̵� �浹 �� �޽� �ǰ� �浹 ����
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetCharacterMovement()->StopMovementImmediately();	// ���� ������ ����

	// ��� ����
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	// AnimInstance(ABP)�� DeathMontage�� ������ ���� ���� ��� ��� ó��
	if (!IsValid(AnimInstance) || !IsValid(DeathMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Death animation unavailable"), *GetName());

		FinishDeath();
		return;
	}

	const float Result = AnimInstance->Montage_Play(DeathMontage);

	// ��� ���� �� ���� ���� ��� ��� ó��
	if (Result <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Death montage failed to start"), *GetName());

		FinishDeath();
		return;
	}

	// ��Ÿ�� ���� ��������Ʈ ����
	// ��� ���� ���� ó���� Notify ������� ����������, Ȥ�� �� �ߴ�/���� �������� ��������Ʈ ���� ����
	FOnMontageEnded OnDeathMontageEnd;
	OnDeathMontageEnd.BindUObject(this, &AEnemyCharacter::HandleDeathMontageEnded);
	// ����� ������ ��� ��Ÿ���� ���� ��������Ʈ�� AnimInstance�� ����
	AnimInstance->Montage_SetEndDelegate(OnDeathMontageEnd, DeathMontage);

	UE_LOG(LogTemp, Log, TEXT("[%s] BeginDeath"), *GetName());

}

void AEnemyCharacter::FinishDeath()
{
	if (LifeState != EEnemyLifeState::Dying)
		return;
	
	LifeState = EEnemyLifeState::Dead;

	// Capture root-motion/character velocity before stopping CharacterMovement.
	const FVector RagdollVelocity = GetVelocity();

	GetCharacterMovement()->StopMovementImmediately();	// ���� ������ ����
	GetCharacterMovement()->DisableMovement();	// ������ ���� ����

	// ĸ�� �浹 ����
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	USkeletalMeshComponent* MeshComp = GetMesh();

	// MeshComp �浹 Ragdoll ����
	MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));

	// MeshComp �浹�� �÷��̾�, Enemy�� �����ϵ��� ����
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// MeshComp ���� ���� (���߿��� �׾����� ���� �������� �ϱ� ����)
	MeshComp->SetEnableGravity(true);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetAllPhysicsLinearVelocity(RagdollVelocity);
	MeshComp->WakeAllRigidBodies();

	UE_LOG(LogTemp, Log, TEXT("[%s] FinishDeath"), *GetName());

	// ��ü ���� ���� (�ʴ���)
	SetLifeSpan(3.f);
}

void AEnemyCharacter::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	UE_LOG(LogTemp, Log, TEXT("[%s] Health Changed: %.1f / %.1f"), *GetName(), NewHealth, MaxHealth);

	if (NewHealth <= 0.f)
	{
		BeginDeath();
	}
}

void AEnemyCharacter::HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != DeathMontage || LifeState != EEnemyLifeState::Dying)
		return;

	UE_LOG(LogTemp, Log, TEXT("[%s] Death montage ended. Interrupted=%d"), *GetName(), bInterrupted);

	FinishDeath();
}
