// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20.
* 적 활동 단계 관리 (멀리 있거나 안 보이는 적의 갱신 줄이기)
* - 일정 주기(UEnemyActivitySettings)마다 플레이어와의 거리, 화면 렌더 여부로 단계를 정해 적에게 적용
*   Active: 지금과 같음 / Reduced: Tick 간격 늘림 / Dormant: 판단 로직 일시정지, 이동 Tick 끔
* - 전투 중, 패턴 실행 중, 피해 직후, 화면에 보이는 적은 항상 Active
* - 엔진 Significance Manager와 같은 개념을 적 수십 마리 규모에 맞게 한 곳에서 처리
*/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Enemy/EnemyTypes.h"
#include "EnemyActivitySubsystem.generated.h"

class AEnemyCharacter;
class UEnemyActivitySettings;

UCLASS()
class MYNICELITTLECAT_API UEnemyActivitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public :

	// 단계 관리 대상에 추가 (적 초기화 완료 시)
	void RegisterEnemy(AEnemyCharacter* Enemy);

	// 단계 관리 대상에서 제외 (사망, 플레이 종료): 단계는 호출한 쪽이 되돌림
	void UnregisterEnemy(AEnemyCharacter* Enemy);

protected :

	// 게임 월드(PIE 포함)에서만 생성
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 평가 타이머 시작
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// 평가 타이머 정리
	virtual void Deinitialize() override;

private :

	// 등록된 적 전부의 단계를 정해 적용
	void EvaluateTiers();

	// 적 하나의 단계 결정 (현재 단계 기준으로 여유 거리 적용)
	EEnemyActivityTier ChooseTier(const AEnemyCharacter& Enemy, const FVector& PlayerLocation,
		const UEnemyActivitySettings& Settings) const;

private :

	// 단계 관리 중인 적
	TArray<TWeakObjectPtr<AEnemyCharacter>> Enemies;

	FTimerHandle EvaluateTimerHandle;
};
