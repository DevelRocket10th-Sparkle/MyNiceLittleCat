// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 수정 일자: 2026.11.19. (패턴 세트 검사를 ValidateConfiguration 하나로: 에디터 검사와 실행 시 검사 공용)
* 패턴 정보 설정 및 보관용 데이터 에셋
* - 캐릭터별 공격 목록: 실행 GA, 거리/시야 조건, 선택 가중치, 후딜, 이동 정책, 동시 실행 자원
* - 런타임 실행 상태는 저장하지 않음 (UEnemyPatternComponent가 관리)
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Enemy/Data/EnemyPatternExecutionSettings.h"
#include "EnemyPatternSetData.generated.h"

class UGameplayAbility;

// 패턴 요청의 실행 결과
UENUM(BlueprintType)
enum class EEnemyPatternResult : uint8
{
	Invalid,	// 해제되었거나 다른 요청
	Running,	// GA 실행 또는 후딜 중
	Completed,	// 후딜까지 정상 종료
	Cancelled,	// 취소
	Failed		// 시작 실패 또는 종료 누락 (시간 초과)
};

// 공격 실행 중 이동을 누가 결정하는지
UENUM(BlueprintType)
enum class EEnemyPatternMovementPolicy : uint8
{
	Stop,				// 공격 실행 중 정지
	AbilityDriven,		// 공격 GA가 이동 제어 (돌진 등)
	FollowLocomotion		// 기본 이동 규칙을 따름 (보스: 페이즈 이동 / 일반 Enemy: Stop)
};

USTRUCT(BlueprintType)
struct FEnemyPatternMovementSettings
{
	GENERATED_BODY()

	// 공격 실행 중 이동 정책
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	EEnemyPatternMovementPolicy Policy = EEnemyPatternMovementPolicy::FollowLocomotion;

	// 비교 연산자(==) 오버로드
	// 두 설정의 공격 이동 정책이 같은지 비교
	bool operator==(const FEnemyPatternMovementSettings& Other) const { return Policy == Other.Policy; }

};

USTRUCT(BlueprintType)
struct FEnemyPatternDefinition
{
	GENERATED_BODY()

	/* 실행 */

	// 패턴을 식별하는 고유 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern")
	FName PatternId;

	// 실제 공격을 수행하도록 부여할 GA 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern")
	TSubclassOf<UGameplayAbility> AbilityClass;

	// GA별 전용 설정: 몽타주와 근접 판정이 필요 없는 패턴은 비워도 됨
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Pattern")
	TObjectPtr<UEnemyPatternExecutionSettings> ExecutionSettings;

	// 1회 타격 피해량 = AttackPower * DamageMultiplier
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.f;

	/* 실행 조건 */
	
	// 타깃 없이 아레나 전체에 실행할 수 있는 패턴이면 false
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	bool bRequiresTarget = true;

	// 타깃과의 거리 및 높이를 실행 조건으로 검사할지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	bool bCheckTargetDistance = true;

	// 타깃과의 수평 거리 범위
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bCheckTargetDistance"))
	float MinRange = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bCheckTargetDistance"))
	float MaxRange = 200.f;
	
	// 타깃과 허용하는 최대 높이 차이
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bCheckTargetDistance"))
	float MaxHeightDifference = 200.f;

	// 실행 시 타깃까지 시야를 확보해야 하는지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	bool bRequiresLineOfSight = true;

	// 자동 선택 확률의 기본 상대 가중치 (페이즈 패턴 목록에서 재정의 가능)
	// - 0이면 자동 선택에서 제외, BT에서 직접 지정할 때만 실행
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	/* 실행 중 동작 */

	// 공격 시작 전 타깃 방향으로 정렬할지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior")
	bool bFaceTargetOnStart = true;

	// 공격 동안 회전을 막을지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior")
	bool bLockRotation = true;

	// 공격 실행 중 이동 정책
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior")
	FEnemyPatternMovementSettings Movement;

	// GA 정상 종료 후 다음 패턴을 막는 추가 대기 시간 (몽타주의 후딜과 합산)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.0", Units = "s"))
	float RecoverySeconds = 0.f;

	// GA 종료 누락을 감지하기 위한 제한 시간 (공격 시간이나 쿨다운이 아님)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.1", Units = "s"))
	float MaxExecutionSeconds = 15.f;

	/* 동시 실행: 페이즈의 MaxConcurrentPatterns가 2 이상일 때만 의미 있음 */

	// 다른 패턴이 실행 중일 때 보조로 함께 실행될 수 있는지
	// - 보조로 실행될 때는 시작 정렬, 회전 잠금, 이동 정책을 쓰지 않음 (몸은 주 패턴이 사용)
	// - 주 패턴으로 뽑혔을 때는 위 설정을 그대로 사용
	// - 근접, 돌진, 레이저 빔처럼 GA 자체가 몸을 쓰는 패턴은 체크해도 보조로 뽑히지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Concurrency")
	bool bCanRunAsCompanion = false;

	// 이 패턴이 추가로 점유하는 자원과 위험 태그 (선택: 겹치면 안 되는 조합이 있을 때만)
	// - Enemy.Pattern.Resource.*: 실행 중에만 점유. 같은 자원을 쓰는 패턴끼리는 동시에 실행하지 않음
	// - Enemy.Pattern.Hazard.*: 실행 중 + GA가 등록한 생성물(폭탄, 장판)이 남아 있는 동안 점유
	// - 주 패턴으로 실행될 때 몸을 쓰는 설정(회전 잠금, 시작 정렬, Stop/AbilityDriven 이동)이면 Resource.Body가 자동으로 붙음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Concurrency", meta = (Categories = "Enemy.Pattern"))
	FGameplayTagContainer Resources;

	// 위험도: 실행 중인 패턴(남은 생성물 포함)의 합이 페이즈의 MaxThreat를 넘으면 함께 실행하지 않음
	// - 0이면 위험도 예산을 쓰지 않음 (연출용, 보조 효과 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Concurrency", meta = (ClampMin = "0"))
	int32 Threat = 1;

	// 보조 패턴으로 뽑혔을 때 시작 지연: 앞 패턴을 보고 피할 시간 (주 패턴에는 적용하지 않음)
	// - 지연 동안에도 동시 실행 수와 자원은 이미 점유 (예약)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Concurrency", meta = (EditCondition = "bCanRunAsCompanion", ClampMin = "0.0", Units = "s"))
	float CompanionStartDelay = 0.f;

	// 값 범위 검사: 에디터 데이터 검사와 런타임 초기화 같은 규칙을 사용
	bool Validate(FString& OutError) const;

};

// 상황(보스 페이즈 등)별 패턴 사용 목록의 항목: 사용할 패턴과 가중치 재정의
USTRUCT(BlueprintType)
struct FEnemyPatternSelectionEntry
{
	GENERATED_BODY()

	// 사용할 패턴 ID (PatternSet에 있어야 함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern")
	FName PatternId;

	// 체크하면 패턴 기본 Weight 대신 아래 값 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (InlineEditConditionToggle))
	bool bOverrideWeight = false;

	// 이 상황에서의 가중치: 0이면 자동 선택에서 제외 (BT 직접 지정 실행은 허용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (EditCondition = "bOverrideWeight", ClampMin = "0.0"))
	float Weight = 1.f;

	// 유효 가중치: 재정의가 없으면 패턴 기본값
	float ResolveWeight(const FEnemyPatternDefinition& Pattern) const { return bOverrideWeight ? Weight : Pattern.Weight; }
};

// 동시 실행 설정 (보스 페이즈마다 다르게 지정, 일반 Enemy는 기본값 = 한 번에 하나)
USTRUCT(BlueprintType)
struct FEnemyPatternConcurrency
{
	GENERATED_BODY()

	// 동시에 실행할 수 있는 패턴 수 상한: 1이면 기존과 같음
	// - 상한일 뿐 목표가 아님: 실행 가능한 후보가 없으면 가능한 수만 실행
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Concurrency", meta = (ClampMin = "1"))
	int32 MaxConcurrentPatterns = 1;

	// 빈 자리마다 보조 패턴을 붙일 확률: 1이면 가능한 만큼 항상 채움
	// - 상한까지 매번 채우면 난이도가 급격히 오르므로 밸런싱 손잡이로 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Concurrency", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "MaxConcurrentPatterns > 1"))
	float CompanionChance = 1.f;

	// 동시에 살아 있는 위험도 합 상한 (실행 중 + 남은 생성물): 0이면 제한 없음
	// - 두 개씩은 피할 수 있어도 세 개가 겹치면 피할 곳이 없어지는 경우를 막는 장치
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Concurrency", meta = (ClampMin = "0", EditCondition = "MaxConcurrentPatterns > 1"))
	int32 MaxThreat = 0;

	// 값 범위 검사
	bool Validate(FString& OutError) const;
};

// 상시 패턴: 랜덤 선택과 별개로 페이즈 내내 따로 굴러가는 패턴 (보스 페이즈에서 사용)
// - 가중치, 동시 실행 수, 공격 간격과 무관. 페이즈 전환, 전투 초기화, 사망 때 자동 정지
// - 보조 실행과 같은 규칙: 패턴의 Can Run As Companion이 켜져 있어야 하고 몸 설정은 쓰지 않음
// - 위험 태그와 위험도는 점유: 같은 위험 태그의 랜덤 패턴은 그동안 뽑히지 않음
USTRUCT(BlueprintType)
struct FEnemyPersistentPatternEntry
{
	GENERATED_BODY()

	// 실행할 패턴 ID (PatternSet에 있어야 함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern")
	FName PatternId;

	// 페이즈가 시작된 뒤 첫 실행까지 대기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float StartDelay = 0.f;

	// 패턴이 끝난 뒤 다시 실행할 때까지 대기: 0이면 한 번만 실행 (포탑처럼 생성물이 계속 남는 패턴)
	// - 거리, 시야, 위험 태그 때문에 시작하지 못하면 이 값과 무관하게 잠시 뒤 다시 시도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float RestartInterval = 0.f;

	// 값 범위 검사
	bool Validate(FString& OutError) const;
};

/**
 * 
 */
UCLASS(BlueprintType)
class MYNICELITTLECAT_API UEnemyPatternSetData : public UDataAsset
{
	GENERATED_BODY()

public :

	 // 에셋 저장 형식 버전 기록 (이전 버전 에셋을 계속 읽을 수 있도록 등록 유지)
	virtual void Serialize(FArchive& Ar) override;

	// 전체 패턴 설정 검사: 패턴별 설정 오류와 패턴 ID 중복 (오류는 줄바꿈으로 모두 담음)
	// - 에디터 저장 검사(IsDataValid)와 실행 시 GA 부여 전 검사(UEnemyPatternComponent)가 같은 규칙을 씀
	bool ValidateConfiguration(FString& OutError) const;

#if WITH_EDITOR
	// GA별 설정 편집 처리는 각 설정 객체에 위임
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

	// 패턴 ID 중복, GA 누락, 거리/시간 범위를 저장/검사 시점에 표시
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

public :

	// 캐릭터별 패턴 설정 목록; 런타임 실행 상태는 저장하지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Patterns", meta = (TitleProperty = "PatternId"))
	TArray<FEnemyPatternDefinition> Patterns;
};
