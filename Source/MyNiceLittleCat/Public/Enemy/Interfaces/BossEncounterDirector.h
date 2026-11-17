// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.16.
* 보스방 전투의 레벨 연출 담당자 계약
* - ABossEncounterManager는 전투 진행(생성, 시작, 실패, 재도전, 클리어)만 관리하고
*   등장 연출, 문, 출구처럼 스테이지마다 다른 처리는 이 인터페이스 구현자에게 맡김
* - 연출 담당자가 없는 보스방은 매니저 혼자 바로 전투를 시작
* - C++ 전용 인터페이스(CannotImplementInterfaceInBlueprint)
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BossEncounterDirector.generated.h"

class ABossCharacter;
class ABossEncounterManager;
class APlayerCharacter;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UBossEncounterDirector : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IBossEncounterDirector
{
	GENERATED_BODY()

public :

	// 새 보스가 생성됨 (최초 준비, 재도전 초기화 모두): 등장 전 숨김 등 준비
	virtual void OnBossSpawned(ABossCharacter* Boss) = 0;

	// 플레이어가 전투 시작 영역에 들어옴: 등장 연출 시작
	// - 시작했으면 true: 연출이 끝나면 Manager.StartEncounter(Player)를 호출할 책임이 있음
	// - 아직 시작할 수 없는 단계(다른 연출 중, 대문 잠김 등)면 false
	virtual bool TryBeginEncounterIntro(ABossEncounterManager& Manager, APlayerCharacter* Player) = 0;

	// 전투 초기화 직전 (보스 제거 전): 연출로 바꾼 레벨 상태 복원
	virtual void OnEncounterReset() = 0;

	// 보스 처치: 출구 개방 등
	virtual void OnEncounterCleared() = 0;
};
