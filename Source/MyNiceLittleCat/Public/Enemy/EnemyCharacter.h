// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.14.
* 잡몹/보스 캐릭터의 기본 클래스
* - GAS 연결
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "AbilitySystemInterface.h"
#include "EnemyTypes.h"
#include "GameplayAbilitySpec.h"
#include "Interfaces/TargetableInterface.h"

#include "EnemyCharacter.generated.h"

class UEnemyAttributeSet;
class UEnemyConfigData;

// Test용
class UAnimMontage;

UCLASS()
class MYNICELITTLECAT_API AEnemyCharacter : public ACharacter, public IAbilitySystemInterface, public ITargetableInterface
{
	GENERATED_BODY()

public:

	// Sets default values for this character's properties
	AEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent()const override;

	// 사망 후처리 - 사망 연출 종료 시점에 실행
	virtual void FinishDeath();

	// Getter: AttributeSet
	inline UEnemyAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// Getter: EnemyConfig
	// 앞 const: 반환된 포인터를 사용한 EnemyConfig 내용 수정 방지
	// 뒤 const: EnemyCharacter의 일반 멤버 변수인 EnemyConfig 변경 방지
	UFUNCTION(BlueprintPure, Category = "Config")
	const UEnemyConfigData* GetEnemyConfig() const { return EnemyConfig; }

	// Getter: bWantsToMvoe
	bool GetWantsToMove() const { return bWantsToMove; }
	// Setter: bWantsToMove
	void SetWantsToMove(bool bNewWantsToMove)	{ bWantsToMove = bNewWantsToMove; }

	// Getter: BasicAttackHandle
	FGameplayAbilitySpecHandle GetBasicAttackHandle() const { return BasicAttackHandle; }

	// AI가 실행될 준비가 되었는지 확인
	bool IsEnemyInitialized() const { return bEnemyInitialized; }

	// 생존 상태 확인
	bool IsAlive() const { return LifeState == EEnemyLifeState::Alive; }

	// ITargetable 유효한 타깃인지 체크
	virtual bool IsTargetAlive_Implementation() const override;

	// 이번 피해로 감소할 Health 계산
	// 잡몹은 일반 계산, 보스는 페이즈 정책 추가
	virtual float CalculateHealthAfterDamage(float CurrentHealth, float MaxHealth, float Damage) const;

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

	// Test용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAnimMontage> BasicAttackMontage;

public :

	// 기본 공격 능력
	TSubclassOf<UGameplayAbility> BasicAttackAbility;

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

	// ASC, 능력치 등 초기화 완료 여부
	bool bEnemyInitialized = false;

	// ASC에 부여된 기본 공격 식별
	FGameplayAbilitySpecHandle BasicAttackHandle;

};
