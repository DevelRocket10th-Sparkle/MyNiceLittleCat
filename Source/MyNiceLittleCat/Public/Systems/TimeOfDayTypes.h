// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20.
* 시간대(아침, 저녁 등) 조명 설정과 하늘 액터 인터페이스
* - FTimeOfDayPreset: 해(Directional Light), 하늘빛(Sky Light), 하늘 색을 한 묶음으로
* - ITimeOfDayReceiver: 하늘 액터가 구현 (엔진 BP_Sky_Sphere의 자식 BP처럼 C++에서 직접 못 다루는 하늘)
*   → 코드가 하늘 BP의 변수나 함수를 이름으로 찾지 않도록 인터페이스로만 전달
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TimeOfDayTypes.generated.h"

// 하늘 액터에 전달할 하늘 색 설정
USTRUCT(BlueprintType)
struct FTimeOfDaySkyPreset
{
	GENERATED_BODY()

	// 하늘 색을 해 높이에서 자동으로 정할지 (끄면 아래 색을 그대로 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky")
	bool bColorsFromSunPosition = true;

	// 머리 위 하늘 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky")
	FLinearColor ZenithColor = FLinearColor::Black;

	// 지평선 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky")
	FLinearColor HorizonColor = FLinearColor::Black;

	// 구름 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky")
	FLinearColor CloudColor = FLinearColor::White;

	// 하늘 전체에 곱하는 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky")
	FLinearColor OverallColor = FLinearColor::White;

	// 해 높이 (-1 아래 ~ 1 위): 하늘 색 자동 계산에 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float SunHeight = 0.5f;
};

// 시간대 하나의 조명 설정
USTRUCT(BlueprintType)
struct FTimeOfDayPreset
{
	GENERATED_BODY()

	// 해 방향 (Directional Light 액터 회전)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun")
	FRotator SunRotation = FRotator(-45.f, 0.f, 0.f);

	// 해 밝기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun", meta = (ClampMin = "0.0"))
	float SunIntensity = 3.14f;

	// 해 색 (선형 색)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun")
	FLinearColor SunColor = FLinearColor::White;

	// 하늘빛 밝기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky Light", meta = (ClampMin = "0.0"))
	float SkyLightIntensity = 5.5f;

	// 하늘빛 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky Light")
	FLinearColor SkyLightColor = FLinearColor::White;

	// 하늘 액터에 전달할 하늘 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sky")
	FTimeOfDaySkyPreset Sky;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UTimeOfDayReceiver : public UInterface
{
	GENERATED_BODY()
};

// 시간대가 바뀔 때 하늘 액터가 받는 알림 (하늘 BP가 구현: 하늘 변수 설정 후 머티리얼 갱신)
class MYNICELITTLECAT_API ITimeOfDayReceiver
{
	GENERATED_BODY()

public :

	// 하늘 색 설정 적용
	UFUNCTION(BlueprintImplementableEvent, Category = "Time Of Day")
	void ApplyTimeOfDaySky(const FTimeOfDaySkyPreset& Sky);
};
