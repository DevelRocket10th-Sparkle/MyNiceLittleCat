// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.15.
* 수정 일자: 2026.11.17. (미사용 사망 정보 구조체 삭제, 주석 정리)
* Enemy를 구현하며 사용되는 enum, struct를 포함하는 일반 헤더 파일
*/

#pragma once

#include "CoreMinimal.h"
#include "EnemyTypes.generated.h"

// Enemy 생존 상태
UENUM(BlueprintType)
enum class EEnemyLifeState : uint8
{
	Alive,	// 생존
	Dying,	// 사망 연출 중 (공격/AI 중단, 충돌 해제)
	Dead	// 사망 연출 종료 (래그돌, 수명 만료 대기)
};

// Enemy 활동 단계 (UEnemyActivitySubsystem이 거리, 화면 렌더 여부로 정함)
// - 순서가 의미 있음: 뒤로 갈수록 갱신이 적음
UENUM(BlueprintType)
enum class EEnemyActivityTier : uint8
{
	Active,		// 지금과 같음 (가깝거나, 화면에 보이거나, 싸우는 중)
	Reduced,	// 멀고 화면 밖: 액터, 이동, AI 컨트롤러 Tick 간격을 늘림
	Dormant		// 아주 멀고 쉬는 중: 판단 로직 일시정지, 땅에 서 있으면 이동 Tick 끔
};
