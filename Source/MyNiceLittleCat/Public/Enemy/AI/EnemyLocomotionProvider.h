// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.16.
* 공격 패턴이 이동을 정하지 않는 동안의 "기본 이동"을 BT Task에 알려주는 계약
* - BTTask_Enemy_PatternMovement는 이 인터페이스만 보고 이동하므로 보스/페이즈 타입을 모름
* - 보스는 UBossPhaseComponent가 구현 (페이즈별 추격/정지/경로 순회)
* - 나중에 순찰 경로를 가진 엘리트 몬스터 등도 같은 Task를 그대로 사용 가능
* - 구현 위치: Pawn 자신 또는 Pawn의 컴포넌트
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnemyLocomotionProvider.generated.h"

// 기본 이동 종류
UENUM(BlueprintType)
enum class EEnemyLocomotionMode : uint8
{
	Stop,			// 제자리
	ChaseTarget,	// Blackboard 타깃 추격
	MoveToLocation	// 지정 위치로 이동 (경로 지점 등)
};

// 현재 기본 이동 의도
struct FEnemyLocomotionIntent
{
	EEnemyLocomotionMode Mode = EEnemyLocomotionMode::Stop;

	// MoveToLocation의 목적지
	FVector Destination = FVector::ZeroVector;

	// 도착 판정 반경 (중심 간 수평 거리)
	float AcceptanceRadius = 0.f;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UEnemyLocomotionProvider : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IEnemyLocomotionProvider
{
	GENERATED_BODY()

public :

	// 기본 이동 규칙의 세대 번호
	// - 0이면 제공할 기본 이동이 없음 → Task는 공격 패턴 이동만 처리
	// - 값이 바뀌면(페이즈 변경 등) 이전 규칙으로 시작한 Task는 종료
	virtual int64 GetLocomotionContextId() const = 0;

	// 현재 기본 이동 계산
	// - PatternId: 선택 또는 실행 중인 패턴 (패턴별 이동 예외 적용), 없으면 None
	// - 지금 이동할 수 없으면 false (목적지 없음 등)
	virtual bool ResolveLocomotion(FName PatternId, FEnemyLocomotionIntent& OutIntent) const = 0;

	// MoveToLocation 목적지에 도착: 다음 경로 지점으로 진행
	// - ContextId가 현재 값과 다르면(이전 페이즈의 늦은 통지) 무시해야 함
	virtual void NotifyLocomotionDestinationReached(int64 ContextId) = 0;
};
