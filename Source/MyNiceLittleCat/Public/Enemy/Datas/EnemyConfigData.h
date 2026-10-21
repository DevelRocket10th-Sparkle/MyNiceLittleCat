// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.15.
* Enemy 능력치(Attributes)
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyConfigData.generated.h"

class UBehaviorTree;
class UGameplayAbility;
class UEnemyPatternSetData;

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UEnemyConfigData : public UDataAsset
{
	GENERATED_BODY()

public :

	// 최대 체력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	// 피해 계산에 사용하는 공격력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float AttackPower = 20.f;

	// 설정 이동 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 600.f;

	// 이 Enemy가 사용할 행동 트리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;
	
	// 홈 복귀 도착 판정 반경
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Home", meta = (ClampMin = "0.0"))
	float HomeArrivalRadius = 100.f;
	
	// 기존 테스트 공격 경로의 거리 기준
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 150.f;

	// PatternSet이 없을 때 사용하는 기존 테스트 GA
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TSubclassOf<UGameplayAbility> BasicAttackAbility;

	// 패턴별 공격 및 몽타주 설정: 미지정 시 기존 테스트 공격 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UEnemyPatternSetData> PatternSet;

	// 최초로 플레이어를 감지하는 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0"))
	float SightRadius = 1500.f;

	// 이미 발견한 플레이어를 놓치는 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 1800.f;

	// 정면에서 한쪽으로 보는 반각: 180이면 뒤쪽을 포함한 전 방향
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PeripheralVisionHalfAngle = 180.f;
	
};
