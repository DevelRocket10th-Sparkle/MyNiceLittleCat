// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20. (UEnemyPatternComponent에서 분리)
* 패턴 동시 실행의 점유 계산과 가중치 추첨 (패턴 컴포넌트 내부용)
* - 슬롯과 남은 생성물을 배열 뷰로 받는 순수 함수: 컴포넌트 상태를 바꾸지 않음
*/

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FEnemyPatternCatalog;
struct FEnemyPatternDefinition;
struct FEnemyPatternLingeringHazard;
struct FEnemyPatternSlot;

namespace EnemyPatternOccupancy
{
	// 실행 중인 슬롯과 살아 있는 생성물이 점유한 자원·위험 태그와 위험도 합
	// - 같은 요청의 슬롯과 생성물 여러 개는 위험도를 한 번만 셈
	void Collect(TConstArrayView<FEnemyPatternSlot> Slots, TConstArrayView<FEnemyPatternLingeringHazard> Hazards,
		FGameplayTagContainer& OutResources, int32& OutThreat);

	// 점유 중인 자원·위험도와 겹치지 않고 함께 실행할 수 있는지
	// - 자원 충돌은 양방향 계층 비교, 혼자 실행하는 경우는 위험도 예산과 상관없이 허용
	bool CanJoin(const FGameplayTagContainer& Resources, int32 Threat,
		const FGameplayTagContainer& Occupied, int32 OccupiedThreat, int32 MaxThreat);

	// 가중치 추첨: 후보 중 하나 (후보가 없거나 가중치 합이 0이면 nullptr)
	const FEnemyPatternDefinition* PickWeighted(TConstArrayView<const FEnemyPatternDefinition*> Candidates,
		const FEnemyPatternCatalog& Catalog);
}
