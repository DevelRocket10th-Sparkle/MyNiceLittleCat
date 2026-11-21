// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyPatternOccupancy.h"

#include "Enemy/Components/EnemyPatternCatalog.h"
#include "Enemy/Components/EnemyPatternComponent.h"

void EnemyPatternOccupancy::Collect(TConstArrayView<FEnemyPatternSlot> Slots, TConstArrayView<FEnemyPatternLingeringHazard> Hazards,
	FGameplayTagContainer& OutResources, int32& OutThreat)
{
	OutResources.Reset();
	OutThreat = 0;

	// 위험도를 이미 센 요청: 실행 중인 슬롯과 그 슬롯이 남긴 생성물을 두 번 세지 않음
	TSet<int64, DefaultKeyFuncs<int64>, TInlineSetAllocator<8>> CountedRequests;

	for (const FEnemyPatternSlot& Slot : Slots)
	{
		if (Slot.Result != EEnemyPatternResult::Running)
		{
			continue;
		}

		OutResources.AppendTags(Slot.Resources);
		OutThreat += Slot.Threat;
		CountedRequests.Add(Slot.RequestId);
	}

	for (const FEnemyPatternLingeringHazard& Hazard : Hazards)
	{
		if (!Hazard.Actor.IsValid())
		{
			continue;
		}

		OutResources.AppendTags(Hazard.Hazards);

		// 같은 요청의 생성물 여러 개(폭탄 3개)는 위험도를 한 번만 셈
		if (!CountedRequests.Contains(Hazard.RequestId))
		{
			OutThreat += Hazard.Threat;
			CountedRequests.Add(Hazard.RequestId);
		}
	}
}

bool EnemyPatternOccupancy::CanJoin(const FGameplayTagContainer& Resources, int32 Threat,
	const FGameplayTagContainer& Occupied, int32 OccupiedThreat, int32 MaxThreat)
{
	// 자원·위험 태그 충돌: 양방향 계층 비교 (Body와 Body.Arms처럼 부모/자식 관계도 충돌)
	if (Resources.HasAny(Occupied) || Occupied.HasAny(Resources))
	{
		return false;
	}

	// 위험도 예산: 혼자 실행하는 경우는 항상 허용 (예산보다 큰 패턴이 영원히 막히지 않도록)
	if (MaxThreat > 0 && OccupiedThreat > 0 && OccupiedThreat + Threat > MaxThreat)
	{
		return false;
	}

	return true;
}

const FEnemyPatternDefinition* EnemyPatternOccupancy::PickWeighted(TConstArrayView<const FEnemyPatternDefinition*> Candidates,
	const FEnemyPatternCatalog& Catalog)
{
	/* 1) 유효 가중치 합산 */

	float TotalWeight = 0.f;
	for (const FEnemyPatternDefinition* Candidate : Candidates)
	{
		TotalWeight += Catalog.GetSelectionWeight(*Candidate);
	}

	if (Candidates.IsEmpty() || !FMath::IsFinite(TotalWeight) || TotalWeight <= 0.f)
	{
		return nullptr;
	}

	/* 2) 가중치 추첨: 0 ~ 총합 사이 값을 뽑고, 후보 가중치를 차례로 빼서 처음 0 이하가 되는 후보 선택 */

	float Roll = FMath::FRand() * TotalWeight;

	// 부동소수점 오차로 반복문에서 선택되지 않는 경우를 대비해 마지막 후보를 기본값으로
	const FEnemyPatternDefinition* Selected = Candidates.Last();

	for (const FEnemyPatternDefinition* Candidate : Candidates)
	{
		Roll -= Catalog.GetSelectionWeight(*Candidate);
		if (Roll <= 0.f)
		{
			Selected = Candidate;
			break;
		}
	}

	return Selected;
}
