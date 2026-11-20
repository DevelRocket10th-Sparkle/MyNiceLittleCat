// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20.
* 절차적 피드백 연출(AFeedbackVisual)이 쓰는 공용 에셋 (프로젝트 설정 → Game → Feedback Visual, DefaultGame.ini)
* - 코드에서 경로로 불러오지 않도록 메시와 머티리얼을 설정에서 지정
* - 설정에서만 참조하는 에셋은 자동으로 쿠킹되지 않으므로 DefaultGame.ini의 DirectoriesToAlwaysCook에 폴더를 함께 등록
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FeedbackVisualSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Feedback Visual"))
class MYNICELITTLECAT_API UFeedbackVisualSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public :

	// 조각 하나의 메시 (길쭉하게 늘려 쓰므로 단순한 박스가 좋음)
	UPROPERTY(Config, EditAnywhere, Category = "Feedback Visual")
	TSoftObjectPtr<UStaticMesh> ShardMesh;

	// 조각 발광 머티리얼: 벡터 파라미터 "Color"로 연출마다 색을 바꿈
	UPROPERTY(Config, EditAnywhere, Category = "Feedback Visual")
	TSoftObjectPtr<UMaterialInterface> GlowMaterial;
};
