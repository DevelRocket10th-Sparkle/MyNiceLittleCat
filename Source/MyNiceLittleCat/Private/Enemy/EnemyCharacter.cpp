// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyCharacter.h"

#include "AbilitySystemComponent.h"
#include "Enemy/GAS/EnemyAttributeSet.h"

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
	}
	
}

// Called every frame
void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
