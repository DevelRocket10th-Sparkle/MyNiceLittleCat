// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20.
* 적 활동 단계 설정 (프로젝트 설정 → Game → Enemy Activity, DefaultGame.ini)
* - 플레이어와의 거리, 화면에 보이는지로 적의 갱신 빈도를 낮춤 (UEnemyActivitySubsystem)
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EnemyActivitySettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Enemy Activity"))
class MYNICELITTLECAT_API UEnemyActivitySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public :

	// 끄면 모든 적이 항상 Active (비교 측정, 문제 확인용)
	UPROPERTY(Config, EditAnywhere, Category = "Enemy Activity")
	bool bEnableActivityTiers = true;

	// 단계를 다시 정하는 주기
	UPROPERTY(Config, EditAnywhere, Category = "Enemy Activity", meta = (ClampMin = "0.05", Units = "s"))
	float EvaluationInterval = 0.25f;

	// 이 거리 안이면 Active
	// - 적의 시야 놓침 반경 + 여유 거리보다 작으면 그 값을 씀 (인지 범위 안에서는 항상 Active)
	UPROPERTY(Config, EditAnywhere, Category = "Enemy Activity", meta = (ClampMin = "0.0", Units = "cm"))
	float ActiveRadius = 3500.f;

	// 이 거리 밖이고 쉬는 중(전투·이동 없음)이면 Dormant
	UPROPERTY(Config, EditAnywhere, Category = "Enemy Activity", meta = (ClampMin = "0.0", Units = "cm"))
	float DormantRadius = 6000.f;

	// 바깥 단계로 넘어갈 때만 더하는 거리 (경계에서 단계가 자주 바뀌는 것 방지)
	UPROPERTY(Config, EditAnywhere, Category = "Enemy Activity", meta = (ClampMin = "0.0", Units = "cm"))
	float TierHysteresis = 300.f;

	// Reduced, Dormant에서 액터, 이동, AI 컨트롤러의 Tick 간격
	UPROPERTY(Config, EditAnywhere, Category = "Enemy Activity", meta = (ClampMin = "0.0", Units = "s"))
	float ReducedTickInterval = 0.1f;

	// 이 시간 안에 화면에 그려졌으면 거리와 상관없이 Active
	UPROPERTY(Config, EditAnywhere, Category = "Enemy Activity", meta = (ClampMin = "0.0", Units = "s"))
	float RecentlyRenderedSeconds = 0.5f;

	// 체력이 바뀐 뒤(피해) Active를 유지하는 시간
	UPROPERTY(Config, EditAnywhere, Category = "Enemy Activity", meta = (ClampMin = "0.0", Units = "s"))
	float DamageWakeSeconds = 5.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
