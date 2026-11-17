// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.13.
* 수정 일자: 2026.11.17. (자기 Spec Handle로 실행 컨텍스트 수신, GA 기본 점유 자원, 남은 위험 생성물 등록)
* UEnemyPatternComponent가 실행하는 모든 공격 GA의 공통 부모
* - 실행 컨텍스트(패턴 설정, 타깃, 요청 번호) 수신, 유효성 확인, 이동/회전 잠금 획득과 자동 해제
* - 파생 GA는 CanStartPattern(설정 검사)과 StartPattern(동작 시작), StopPattern(정리)만 구현
* 
* 실행 흐름
*	ActivateAbility -> 컨텍스트 수신 -> CanStartPattern -> CommitAblity -> StartPattern
*	-> (FinishPatter / CancelPattern) -> EndAbility -> StopPattern -> 이동/회전 잠금 해제 -> 참조 정리
*/

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"

#include "Enemy/Data/EnemyPatternSetData.h"

#include "EnemyPatternAbility.generated.h"

class AEnemyCharacter;
class UEnemyPatternComponent;

/**
 * 
 */
UCLASS(Abstract)
class MYNICELITTLECAT_API UEnemyPatternAbility : public UGameplayAbility
{
	GENERATED_BODY()

public :
	
	UEnemyPatternAbility();

	// 실행 컨텍스트 수신 후 파생 GA 시작
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 정상 완료와 취소 모두 파생 정리 -> 잠금 해제 -> 참조 정리
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// Getter: 이 GA 클래스가 기본으로 점유하는 자원 (패턴 컴포넌트가 초기화 때 CDO에서 읽음)
	const FGameplayTagContainer& GetDefaultPatternResources() const { return DefaultPatternResources; }

protected :

	/* 파생 GA 구현 지점 */
	
	// 실행 확정(CommitAbility) 전 설정 검사: false면 취소 종료
	virtual bool CanStartPattern() const { return true; }

	// 동작 시작: false면 취소 종료
	virtual bool StartPattern() PURE_VIRTUAL(UEnemyPatternAbility::StartPattern, return false;);
	
	// 종료 시 파생 GA가 만든 타이머, 액터, 구독 정리 (잠금 해제 전에 호출)
	virtual void StopPattern(bool bWasCancelled) {}

	/* 파생 GA가 사용하는 도우미 */
	
	// 이 실행이 아직 유효한지: 능력 활성, Enemy 생존, 같은 패턴 요청 진행 중
	bool IsPatternExecutionValid() const;

	// 정상 완료
	void FinishPattern();
	// 취소 종료
	void CancelPattern();

	// 이동 잠금 획득: 한 실행에서 한 번만 획득하고, EndAbility에서 자동 해제
	void LockMovement();
	// 회전 잠금 획득: 한 실행에서 한 번만 획득하고, EndAbility에서 자동 해제
	void LockRotation();

	// 이 패턴 전용 실행 설정 (설정 타입이 다르면 nullptr)
	template <typename TSettings>
	TSettings* GetPatternSettings() const
	{
		return Cast<TSettings>(ExecutionPattern.ExecutionSettings);
	}

	// 1회 타격 피해량 = AttackPower * 패턴 DamageMultiplier
	float CalculatePatternDamage() const;

	// 이 실행이 만든 액터가 살아 있는 동안 패턴의 위험 태그와 위험도를 계속 점유 (폭탄, 장판, 포탑)
	// - 위험 태그가 없고 위험도가 0인 패턴이면 아무 일도 하지 않음
	void RegisterLingeringHazard(AActor* SpawnedActor) const;

protected :
	
	// 이 GA 클래스의 기본 점유 자원 (패턴 데이터의 Resources와 합쳐짐)
	// - 몸을 쓰는 GA는 생성자에서 Enemy.Pattern.Resource.Body를 넣어 둠 → 페이즈 설정자가 근접 조합마다 금지 등록할 필요 없음
	UPROPERTY(EditDefaultsOnly, Category = "Pattern", meta = (Categories = "Enemy.Pattern"))
	FGameplayTagContainer DefaultPatternResources;

	// 현재 실행용으로 복사한 패턴 설정
	UPROPERTY(BlueprintReadOnly, Category = "Pattern")
	FEnemyPatternDefinition ExecutionPattern;

	// 실행 시작 시 타깃: Blackboard 변경과 독립적으로 유지
	UPROPERTY(BlueprintReadOnly, Category = "Pattern")
	TObjectPtr<AActor> ExecutionTarget;

	// 공격 주체
	TWeakObjectPtr<AEnemyCharacter> Enemy;

	// 요청 유효성을 확인할 패턴 컴포넌트
	TWeakObjectPtr<UEnemyPatternComponent> PatternComponent;

	// 이번 실행의 패턴 요청 번호
	int64 RequestId = 0;

private :

	// 이 GA가 획득한 잠금만 해제하기 위한 상태
	bool bOwnsMovementLock = false;
	bool bOwnsRotationLock = false;

};
