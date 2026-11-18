// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.16.
* 보스 페이즈 전환 연출을 레벨 쪽에 맡기는 계약
* - 보스는 "전환 연출이 끝났다"는 통지만 기다리고, 어떤 시퀀스/문/대기 조건을 쓰는지는 모름
* - IBossPhaseGimmickInterface와 같은 방식: 시작 시 번호를 넘기고, 완료 시 같은 번호로 통지
* - C++ 전용 인터페이스(CannotImplementInterfaceInBlueprint): Execute_ 래퍼 없이 바로 호출
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BossPhaseTransitionHandler.generated.h"

class ABossCharacter;

// 전환 연출에 넘기는 정보
struct FBossPhaseTransitionContext
{
	// 전환 중인 보스
	ABossCharacter* Boss = nullptr;

	// 이번 전환 요청 번호: 완료 통지 시 그대로 전달
	int64 TransitionId = 0;

	// 끝나는 페이즈와 다음 페이즈
	FName FromPhaseId;
	FName ToPhaseId;

	// 다음 페이즈 설정의 전환 제한 시간: 연출 재생 제한 시간으로 사용
	float TimeoutSeconds = 0.f;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UBossPhaseTransitionHandler : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IBossPhaseTransitionHandler
{
	GENERATED_BODY()

public :

	// 전환 연출 시작
	// - 담당하면 true: 연출과 대기 조건이 모두 끝나면 반드시 한 번 Boss->NotifyPhaseTransitionPresentationFinished(TransitionId) 호출
	// - 담당하지 않으면 false: 보스가 페이즈 설정의 기본 전환 시퀀스로 처리
	virtual bool BeginPhaseTransition(const FBossPhaseTransitionContext& Context) = 0;

	// 전투 정지/초기화로 전환이 무효화됨: 진행 중인 연출과 대기를 정리 (완료 통지 금지)
	virtual void CancelPhaseTransition(int64 TransitionId) = 0;

	// 연출은 끝났고 플레이어 도착(아레나 진입 등)을 기다리는 중인지 (UI 안내용, 기본 false)
	virtual bool IsWaitingForPlayer(int64 TransitionId) const { return false; }
};
