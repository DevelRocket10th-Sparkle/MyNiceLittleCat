// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Enemy/EnemyCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Enemy/Datas/EnemyConfigData.h"
#include "Abilities/GameplayAbility.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Enemy/AI/EnemyAIController.h"

// Sets default values
AEnemyCharacter::AEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UEnemyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
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

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (IsValid(MeshComp))
	{
		// 생존 중에는 애니메이션으로만 메시 제어
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetAllBodiesSimulatePhysics(false);
		MeshComp->SetAllBodiesPhysicsBlendWeight(0.f);
	}

	if (IsValid(ASC))
	{
		ASC->InitAbilityActorInfo(this, this);	// GAS 소유자와 실제 행동 소유자 일치
		
		// Attributes 초기화
		if (EnemyConfig)
		{
			ApplyEnemyConfig();
		}
	}
	
	// Health 변경 델리게이트 구독
	AttributeSet->OnHealthChange.AddUObject(this, &AEnemyCharacter::HandleHealthChanged);
	
	// 초기 Ability 부여
	if (IsValid(ASC) && IsValid(EnemyConfig) && EnemyConfig->BasicAttackAbility)
	{
		BasicAttackHandle =ASC->GiveAbility(
			FGameplayAbilitySpec(
				EnemyConfig->BasicAttackAbility,
				1,
				INDEX_NONE,
				this
			)
		);
	}

	if (UEnemyCharacterMovementComponent* Movement = Cast<UEnemyCharacterMovementComponent>(GetCharacterMovement()))
	{
		Movement->InitializeHoverHeight();
	}

	bEnemyInitialized = true;	// 초기화 완료 기록

	// Controller가 먼저 빙의한 경우, 여기서 AI 시작 요청
	if (AEnemyAIController* EnemyAI = Cast<AEnemyAIController>(GetController()))
	{
		EnemyAI->TryStartEnemyAI();
	}
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

	// Health는 MaxHealth의 값을 그대로 사용
	ASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetHealthAttribute(),
		EnemyConfig->MaxHealth
	);

	ASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetAttackPowerAttribute(),
		EnemyConfig->AttackPower
	);

	GetCharacterMovement()->MaxWalkSpeed = EnemyConfig->MoveSpeed;
	GetCharacterMovement()->MaxFlySpeed = EnemyConfig->MoveSpeed;
}

void AEnemyCharacter::BeginDeath()
{
	if (LifeState != EEnemyLifeState::Alive)
		return;

	LifeState = EEnemyLifeState::Dying;	// 중복 방지를 위해 최우선 처리 필요

	SetWantsToMove(false);

	// AI 중단
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Death"));
		}

		AI->StopMovement();
	}

	// 기존 Ability 취소
	if (IsValid(ASC))
	{
		ASC->CancelAllAbilities();
	}

	// 사망 중 이동 충돌 및 메시 피격 충돌 해제
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetCharacterMovement()->StopMovementImmediately();	// 현재 움직임 제거

	// 사망 연출
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	// AnimInstance(ABP)나 DeathMontage가 없으면 연출 없이 즉시 사망 처리
	if (!IsValid(AnimInstance) || !IsValid(DeathMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Death animation unavailable"), *GetName());

		FinishDeath();
		return;
	}

	const float Result = AnimInstance->Montage_Play(DeathMontage);

	// 재생 실패 시 연출 없이 즉시 사망 처리
	if (Result <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Death montage failed to start"), *GetName());

		FinishDeath();
		return;
	}

	// 몽타주 종료 델리게이트 구독
	// 사망 연출 종료 처리를 Notify 방식으로 변경했지만, 혹시 모를 중단/누락 방어용으로 델리게이트 구독 유지
	FOnMontageEnded OnDeathMontageEnd;
	OnDeathMontageEnd.BindUObject(this, &AEnemyCharacter::HandleDeathMontageEnded);
	// 재생에 성공한 사망 몽타주의 종료 델리게이트를 AnimInstance에 연결
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

	GetCharacterMovement()->StopMovementImmediately();	// 현재 움직임 제거
	GetCharacterMovement()->DisableMovement();	// 움직임 제한 설정

	// 캡슐 충돌 무시
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	USkeletalMeshComponent* MeshComp = GetMesh();

	// MeshComp 충돌 Ragdoll 설정
	MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));

	// MeshComp 충돌이 플레이어, Enemy는 무시하도록 설정
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// MeshComp 물리 설정 (공중에서 죽었으면 땅에 떨어져야 하기 때문)
	MeshComp->SetEnableGravity(true);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetAllPhysicsLinearVelocity(RagdollVelocity);
	MeshComp->WakeAllRigidBodies();

	UE_LOG(LogTemp, Log, TEXT("[%s] FinishDeath"), *GetName());

	// 시체 제거 유예 (초단위)
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
