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

	// 사망 후처리 - 사망 연출 종료 시점에 실행
	virtual void FinishDeath();

	// Getter: AttributeSet
	inline UEnemyAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// Getter: bWantsToMvoe
	bool GetWantsToMove() const { return bWantsToMove; }
	// Setter: bWantsToMove
	void SetWantsToMove(bool bNewWantsToMove)	{ bWantsToMove = bNewWantsToMove; }

protected:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Enemy 능력치 적용
	UFUNCTION(CallInEditor)
	virtual void ApplyEnemyConfig();

	// 사망 전처리 - 사망 판정 직후 실행
	virtual void BeginDeath();	// 필요할 경우 EnemyTypes에 선언된 구조체 FEnemyDeathContext 받기

	/* 델리게이트 반응 함수 */

	void HandleHealthChanged(float NewHealth, float MaxHealth);

	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

protected :

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UEnemyAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TObjectPtr<UEnemyConfigData> EnemyConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Life")
	EEnemyLifeState LifeState = EEnemyLifeState::Alive;

	// AnimInstance에서 플레이어 이동 애니메이션 재생 여부를 결정하기 위해 사용
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement")
	bool bWantsToMove = false;

	/* AnimMontage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

};
