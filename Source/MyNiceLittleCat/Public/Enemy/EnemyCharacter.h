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

class UEnemyPatternComponent;
class UEnemyMeleeTraceComponent;

// Test용
class UAnimMontage;

UCLASS()
class MYNICELITTLECAT_API AEnemyCharacter : public ACharacter, public IAbilitySystemInterface, public ITargetableInterface
{
	GENERATED_BODY()

public:

	// Sets default values for this character's properties
	AEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	// Getter: 이 캐릭터의 능력 실행 컴포넌트
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
	// Health 변경 전에 무적 및 페이즈 체력 하한을 반영
	virtual float CalculateHealthAfterDamage(float CurrentHealth, float MaxHealth, float Damage) const;

	/* Pattern Component */

	// Getter: 패턴 실행 컴포넌트
	// Getter: 캐릭터별 패턴 실행 컴포넌트
	UFUNCTION(BlueprintPure, Category = "Enemy|Pattern")
	UEnemyPatternComponent* GetPatternComponent() const;
	UFUNCTION(BlueprintPure, Category = "Enemy|Melee")
	UEnemyMeleeTraceComponent* GetMeleeTraceComponent() const { return MeleeTraceComponent; }
	// 공격 GA가 이동 요청을 막는 동안 유지하는 참조 횟수 증가
	void AcquireAttackMovementLock() { ++AttackMovementLockCount; }
	// 공격 GA 종료 시 자신이 획득한 이동 제한 해제
	void ReleaseAttackMovementLock() { AttackMovementLockCount = FMath::Max(0, AttackMovementLockCount - 1); }
	// Getter: 일반 경로 이동이 제한되는지 확인
	bool IsAttackMovementLocked() const { return AttackMovementLockCount > 0; }
	// 공격 중 Controller 및 루트 모션 회전을 막는 참조 횟수 증가
	void AcquireRotationLock() { ++RotationLockCount; }
	// 공격 종료 시 자신이 획득한 회전 제한 해제
	void ReleaseRotationLock() { RotationLockCount = FMath::Max(0, RotationLockCount - 1); }
	// Getter: 공격 중 회전 제한 여부
	bool IsRotationLocked() const { return RotationLockCount > 0; }
	// 회전 제한 중 Controller의 방향 갱신이 액터를 회전시키지 않도록 처리
	// 공격 중 회전 제한을 존중하여 Controller 방향 반영
	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime = 0.f) override;

	// 초기화와 생존 및 보스방 전투 상태를 포함한 AI 실행 가능 여부 확인
	virtual bool CanRunEnemyAI() const;

protected:

	// Called when the game starts or when spawned
	// 컴포넌트 초기화 후 실행 준비
	virtual void BeginPlay() override;
	
	// Called every frame
	// 활성 상태의 매 프레임 처리
	//virtual void Tick(float DeltaTime) override;

	// Enemy 능력치 적용
	// 설정 데이터의 능력치 및 이동 속도를 캐릭터에 적용
	UFUNCTION(CallInEditor)
	virtual void ApplyEnemyConfig();

	// 사망 전처리 - 사망 판정 직후 실행
	// 중복 사망을 막고 공격과 AI를 정리한 뒤 사망 연출 시작
	virtual void BeginDeath();	// 필요할 경우 EnemyTypes에 선언된 구조체 FEnemyDeathContext 받기

	/* 델리게이트 반응 함수 */

	// 체력 변경 시 사망 판정을 먼저 수행한 뒤 추가 상태 처리
	virtual void HandleHealthChanged(float NewHealth, float MaxHealth);

	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// Test용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAnimMontage> BasicAttackMontage;

	/* Pattern Component */

	// 기본 초기화 뒤 파생 캐릭터의 추가 설정 준비
	virtual bool InitializeEnemyExtensions();

	// 제거 중 이전 공격의 이벤트가 동작하지 않도록 실행 무효화
	// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected :

	// 능력을 부여하고 실행할 ASC 참조
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> ASC;

	// 이 Enemy의 능력치 저장소
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UEnemyAttributeSet> AttributeSet;

	// 능력치, AI 및 패턴 설정 참조
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TObjectPtr<UEnemyConfigData> EnemyConfig;

	// 생존, 사망 진행 및 사망 완료 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Life")
	EEnemyLifeState LifeState = EEnemyLifeState::Alive;

	// AnimInstance에서 Enemy 이동 애니메이션 재생 여부를 결정하기 위해 사용
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement")
	bool bWantsToMove = false;

	/* AnimMontage */
	// 사망 연출에 사용하는 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	// ASC, 능력치 등 초기화 완료 여부
	bool bEnemyInitialized = false;

	// ASC에 부여된 기본 공격 식별
	FGameplayAbilitySpecHandle BasicAttackHandle;

	// 캐릭터별 패턴 실행 상태 및 GA 핸들 관리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UEnemyPatternComponent> PatternComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UEnemyMeleeTraceComponent> MeleeTraceComponent;
	// 현재 이동 제한을 획득한 공격 실행 수
	int32 AttackMovementLockCount = 0;
	// 현재 회전 제한을 획득한 공격 실행 수
	int32 RotationLockCount = 0;

};
