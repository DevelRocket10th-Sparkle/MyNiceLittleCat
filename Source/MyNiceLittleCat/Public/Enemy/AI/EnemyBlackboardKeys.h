// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.16.
* AI 컨트롤러가 직접 쓰는 Blackboard 키 이름 모음
* - BT 노드는 이 상수를 쓰지 않고 FBlackboardKeySelector로 에디터에서 키를 고름
*   (이 상수는 노드 Selector의 "기본값"으로만 사용 → 기존 BT 에셋을 다시 설정하지 않아도 동작)
* - 키 이름을 바꿀 때는 BB 에셋과 이 파일만 수정
*/

#pragma once

#include "CoreMinimal.h"

namespace EnemyBlackboardKeys
{
	/* 공통: AEnemyAIController가 기록 */

	// 전투 타깃 (Object, AActor)
	inline const FName TargetActor(TEXT("TargetActor"));

	// 복귀 위치 (Vector)
	inline const FName HomeLocation(TEXT("HomeLocation"));

	// 일반 행동 가능 여부 (Bool)
	inline const FName CanAct(TEXT("CanAct"));

	/* 보스 전용: ABossAIController가 기록 */

	// 페이즈 전환 요청 또는 진행 중 (Bool)
	inline const FName PhaseTransitionRequested(TEXT("PhaseTransitionRequested"));

	/* BT 서비스 출력 키의 기본 이름 (노드 Selector 기본값) */

	inline const FName IsInAttackRange(TEXT("IsInAttackRange"));
	inline const FName HasLineOfSight(TEXT("HasLineOfSight"));
	inline const FName CanStartAttack(TEXT("CanStartAttack"));
	inline const FName IsAwayFromHome(TEXT("IsAwayFromHome"));
}
