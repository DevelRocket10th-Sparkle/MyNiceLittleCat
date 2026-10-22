// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 패턴 정보 설정 및 보관용 데이터 에셋
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Enemy/Datas/EnemyPatternExecutionSettings.h"
// 기존 에셋 이전용 필드의 타입; 공통 실행 로직에서는 근접 설정을 사용하지 않음
#include "Enemy/Datas/EnemyMeleePatternSettings.h"
#include "EnemyPatternSetData.generated.h"

class UGameplayAbility;
class UAnimMontage;

UENUM(BlueprintType)
enum class EEnemyPatternResult : uint8
{
	Invalid,
	Running,
	Completed,
	Cancelled,
	Failed
};

USTRUCT(BlueprintType)
struct FEnemyPatternDefinition
{
	GENERATED_BODY()

	// 다른 담당자의 패턴: GA, 타깃 및 거리 검사 없이 로그 후 완료 처리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bLogOnly = false;


	// 패턴을 식별하는 고유 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName PatternId;

	// 실제 공격을 수행하도록 부여할 GA 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityClass;

	// GA별 전용 설정; 몽타주와 근접 판정이 필요 없는 패턴은 비워도 됨
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Execution")
	TObjectPtr<UEnemyPatternExecutionSettings> ExecutionSettings;

	// 캐릭터 및 패턴별 공격 몽타주
	// 이전 에셋 호환용: PostLoad에서 ExecutionSettings로 이전
	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage;

	// 공격 몽타주 시작 섹션; None이면 기본 위치
	// 이전 에셋 호환용: 새 설정은 GA 전용 객체에서 편집
	UPROPERTY()
	FName MontageSection;

	// 타깃과의 수평 최소 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float MinRange = 0.f;

	// 타깃과 허용하는 최대 수평 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float MaxRange = 200.f;

	// 타깃과 허용하는 최대 높이 차이
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float MaxHeightDifference = 200.f;

	// 자동 선택 확률의 상대 가중치; 0이면 직접 지정으로만 실행
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	// 타깃 없이 아레나 전체에 실행할 수 있는 패턴인지 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bRequiresTarget = true;

	// 타깃과의 거리 및 높이를 실행 조건으로 검사할지 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bCheckTargetDistance = true;

	// 실행 시 타깃까지 시야를 확보해야 하는지 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bRequiresLineOfSight = true;

	// 공격 시작 시에만 타깃 방향으로 정렬할지 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bFaceTargetOnStart = true;

	// 공격 동안 회전을 제한할지 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bLockRotation = true;

	// GA 종료 누락을 감지하기 위한 제한 시간
	// 공격 시간이나 쿨다운이 아님
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float MaxExecutionSeconds = 15.f;

	// NotifyState 구간 동안 각 소켓의 이전 위치에서 현재 위치까지 Sphere Sweep
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UEnemyMeleeTraceSettings>> MeleeTraceSettings;
	// 기존 에셋의 설정을 잃지 않도록 PostLoad에서 이전. 에디터에는 표시하지 않음.
	UPROPERTY()
	TArray<FEnemyMeleeTraceDefinition> MeleeTraces;
	// SetByCaller 피해량에 곱할 공격력 배율
	// 이전 에셋 호환용 배율; 현재 근접 GA는 전용 설정 값을 사용
	UPROPERTY()
	float DamageMultiplier = 1.f;
};

/**
 * 
 */
UCLASS(BlueprintType)
class MYNICELITTLECAT_API UEnemyPatternSetData : public UDataAsset
{
	GENERATED_BODY()

public :

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	// 캐릭터별 패턴 설정 목록; 런타임 실행 상태는 저장하지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Patterns")
	TArray<FEnemyPatternDefinition> Patterns;
};
