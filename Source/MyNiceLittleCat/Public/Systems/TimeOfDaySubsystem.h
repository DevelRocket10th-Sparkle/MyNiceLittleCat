// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20. (Stage3 전용 UMansionLightingSubsystem 대체)
* 현재 시간대를 관리하는 월드 서브시스템
* - 메인 맵의 ATimeOfDayController가 등록되면 그 컨트롤러로 조명을 바꿈
* - 다른 레벨(레벨 인스턴스의 연출 담당자 등)은 태그로 요청: SetTimeOfDay(TimeOfDay.Evening)
* - 체크포인트 저장에는 현재 태그만 남기고, 같은 레벨에서 불러오면 그 시간대로 되돌림
*/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "TimeOfDaySubsystem.generated.h"

class ATimeOfDayController;

UCLASS()
class MYNICELITTLECAT_API UTimeOfDaySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public :

	// 저장 교체 알림 구독 해제
	virtual void Deinitialize() override;

	/* 컨트롤러 */

	// 컨트롤러 등록: 저장 교체 알림을 구독하고 저장된 시간대(없으면 기본) 적용
	void RegisterController(ATimeOfDayController* Controller);

	// 컨트롤러 해제 (같은 컨트롤러일 때만)
	void UnregisterController(ATimeOfDayController* Controller);

	/* 시간대 */

	// 시간대 바꾸기: 빈 태그면 기본 시간대
	UFUNCTION(BlueprintCallable, Category = "Time Of Day")
	void SetTimeOfDay(FGameplayTag TimeOfDay);

	// 현재 시간대 (기본이면 빈 태그)
	UFUNCTION(BlueprintPure, Category = "Time Of Day")
	FGameplayTag GetTimeOfDay() const { return CurrentTimeOfDay; }

	// 체크포인트의 시간대로 되돌림: 같은 레벨의 체크포인트가 아니면 기본 시간대
	void RestoreFromCheckpoint();

private :

	// 등록된 컨트롤러
	TWeakObjectPtr<ATimeOfDayController> ActiveController;

	// 현재 시간대 (빈 태그 = 기본)
	FGameplayTag CurrentTimeOfDay;

	// 저장 교체 알림 구독 핸들
	FDelegateHandle SaveReplacedHandle;
};
