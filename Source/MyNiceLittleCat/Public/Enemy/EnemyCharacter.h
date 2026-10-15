// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.14.
* 몬스터/보스 캐릭터의 기본 클래스
* - GAS 연결
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "AbilitySystemInterface.h"
#include "EnemyTypes.h"

#include "EnemyCharacter.generated.h"

class UEnemyAttributeSet;
class UEnemyConfigData;

UCLASS()
class MYNICELITTLECAT_API AEnemyCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	// Sets default values for this character's properties
	AEnemyCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent()const override;
	
	// Getter: AttributeSet
	inline UEnemyAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Enemy 능력치 적용
	UFUNCTION(CallInEditor)
	virtual void ApplyEnemyConfig();

	// 사망 반응
	virtual void BeginDeath();	// 필요할 경우 EnemyTypes에 선언된 구조체 FEnemyDeathContext 받기
	virtual void FinishDeath();

	/* 델리게이트 반응 함수 */

	void HandleHealthChanged(float NewHealth, float MaxHealth);

protected :

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UEnemyAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TObjectPtr<UEnemyConfigData> EnemyConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Life")
	EEnemyLifeState LifeState = EEnemyLifeState::Alive;

};
