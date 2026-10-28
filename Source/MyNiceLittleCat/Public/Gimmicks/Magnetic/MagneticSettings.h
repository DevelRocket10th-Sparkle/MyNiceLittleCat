/// 작성자: 최요환
/// 작성일자: 2026-10-28
///
/// 자성 기믹 공통 설정 (프로젝트 설정 > 게임 > Magnetic, DefaultGame.ini에 저장)
/// UMagneticComponent가 PolarityEffect를 따로 지정하지 않으면 여기의 기본 극성 이펙트를 사용합니다.
/// 코인·스테이지 기믹 등 자성 액터 전체가 같은 연출을 쓰도록 한 곳에서 관리하기 위한 용도입니다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MagneticSettings.generated.h"

class UNiagaraSystem;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Magnetic"))
class MYNICELITTLECAT_API UMagneticSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 자성 컴포넌트 기본 극성 이펙트 (User 파라미터 규칙은 UMagneticComponent 주석 참고)
	UPROPERTY(Config, EditAnywhere, Category = "Effect")
	TSoftObjectPtr<UNiagaraSystem> DefaultPolarityEffect;
};
