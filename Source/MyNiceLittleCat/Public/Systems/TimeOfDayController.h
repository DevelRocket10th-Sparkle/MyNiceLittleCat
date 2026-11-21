// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20.
* 맵의 해, 하늘빛, 하늘을 시간대 설정대로 바꾸는 배치 액터 (공용 하늘이 있는 메인 맵에 하나)
* - 참조는 같은 레벨의 액터를 에디터에서 지정: 하늘·조명은 메인 맵에, 연출 담당자는 레벨 인스턴스에 있어
*   레벨 사이에 직접 참조를 걸 수 없으므로 UTimeOfDaySubsystem에 등록하고, 다른 레벨은 태그로 요청
* - 기본 설정은 시작할 때 적용 (체크포인트에 저장된 시간대가 있으면 그것)
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Systems/TimeOfDayTypes.h"
#include "TimeOfDayController.generated.h"

class ADirectionalLight;
class ASkyLight;

UCLASS()
class MYNICELITTLECAT_API ATimeOfDayController : public AActor
{
	GENERATED_BODY()

public :

	ATimeOfDayController();

	// 시간대 적용: 태그가 비었거나 등록되지 않았으면 기본 설정
	void ApplyTimeOfDay(const FGameplayTag& TimeOfDay);

protected :

	// 서브시스템에 등록하고 저장된 시간대(없으면 기본) 적용
	virtual void BeginPlay() override;

	// 등록 해제
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :

	// 설정 하나를 해, 하늘빛, 하늘에 적용
	void ApplyPreset(const FTimeOfDayPreset& Preset);

public :

	/* 대상 */

	// 바꿀 해 (Directional Light)
	UPROPERTY(EditInstanceOnly, Category = "Time Of Day")
	TArray<TObjectPtr<ADirectionalLight>> Suns;

	// 바꿀 하늘빛 (Sky Light)
	UPROPERTY(EditInstanceOnly, Category = "Time Of Day")
	TArray<TObjectPtr<ASkyLight>> SkyLights;

	// 하늘 색을 받을 하늘 액터 (ITimeOfDayReceiver를 구현한 하늘 BP)
	UPROPERTY(EditInstanceOnly, Category = "Time Of Day", meta = (MustImplement = "/Script/MyNiceLittleCat.TimeOfDayReceiver"))
	TObjectPtr<AActor> Sky;

	/* 설정 */

	// 기본 시간대 (태그 없이 시작할 때)
	UPROPERTY(EditAnywhere, Category = "Time Of Day")
	FTimeOfDayPreset DefaultPreset;

	// 태그별 시간대 (예: TimeOfDay.Evening)
	UPROPERTY(EditAnywhere, Category = "Time Of Day", meta = (Categories = "TimeOfDay"))
	TMap<FGameplayTag, FTimeOfDayPreset> Presets;

private :

	// 루트 (위치 표시용)
	UPROPERTY(VisibleAnywhere, Category = "Time Of Day")
	TObjectPtr<USceneComponent> Root;
};
