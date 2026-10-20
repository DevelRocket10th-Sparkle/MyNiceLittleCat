// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 패턴 정보 설정 및 보관용 데이터 에셋
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EnemyPatternSetData.generated.h"

class UGameplayAbility;
class UAnimMontage;
class UGameplayEffect;

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

	// 캐릭터 및 패턴별 공격 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> Montage;

	// 공격 몽타주 시작 섹션; None이면 기본 위치
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
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

	// 기본 근접 GA에서 타깃의 Damage 속성에 적용할 효과: 미지정 시 연출만 실행
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> DamageEffect;
	// DamageEffect가 SetByCaller를 사용할 때 전달할 태그: 미지정 시 GE 자체 수치 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag DamageMagnitudeTag;
	// 기본 근접 판정 구체의 반지름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float HitRadius = 150.f;
	// 근접 판정 중심을 전방으로 옮길 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float HitForwardOffset = 100.f;
	// SetByCaller 피해량에 곱할 공격력 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
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

	// 캐릭터별 패턴 설정 목록; 런타임 실행 상태는 저장하지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Patterns")
	TArray<FEnemyPatternDefinition> Patterns;
};
