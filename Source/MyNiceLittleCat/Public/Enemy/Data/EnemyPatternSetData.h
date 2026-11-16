// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 수정 일자: 2026.11.12. (로그 전용 패턴 제거, 이동 정책 이름 변경, 이전 형식 이전 코드 제거)
* 패턴 정보 설정 및 보관용 데이터 에셋
* - 캐릭터별 공격 목록: 실행 GA, 거리/시야 조건, 선택 가중치, 후딜, 이동 정책
* - 런타임 실행 상태는 저장하지 않음 (UEnemyPatternComponent가 관리)
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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

	// 자동 선택 확률의 상대 가중치: 0이면 BT에서 직접 지정할 때만 실행
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

	// 값 범위 검사: 에디터 데이터 검사와 런타임 초기화 같은 규칙을 사용
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
