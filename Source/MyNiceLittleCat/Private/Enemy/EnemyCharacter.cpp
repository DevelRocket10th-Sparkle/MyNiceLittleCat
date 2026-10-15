// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyCharacter.h"

#include "AbilitySystemComponent.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Enemy/Datas/EnemyConfigData.h"
#include "GameFramework/CharacterMovementComponent.h"

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

// Called when the game starts or when spawned
void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

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
}

void AEnemyCharacter::BeginDeath()
{
	if (LifeState != EEnemyLifeState::Alive)
		return;

	LifeState = EEnemyLifeState::Dying;	// 중복 방지를 위해 최우선 처리 필요
	
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	UE_LOG(LogTemp, Log, TEXT("[%s] BeginDeath"), *GetName());

	// 추후: 공격 판정 제거, 전투 Ability 취소, AI 중단
	// 추후: 사망 연출 시작

	// 사망 연출 구현 전이기 때문에 바로 완료
	FinishDeath();
}

void AEnemyCharacter::FinishDeath()
{
	if (LifeState != EEnemyLifeState::Dying)
		return;
	
	LifeState = EEnemyLifeState::Dead;

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
