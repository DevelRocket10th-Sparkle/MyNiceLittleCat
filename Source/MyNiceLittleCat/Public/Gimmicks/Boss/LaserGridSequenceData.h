/// 작성자: 최요환
/// 작성일자: 2026-10-21
///
/// 바닥 광역 레이저 패턴의 공격 순서 데이터입니다 (DA_LaserGrid_...).
/// Steps를 위에서부터 차례로 실행하며, 각 단계의 DelayToNext만큼 기다렸다가 다음 단계로 갑니다 (0이면 동시).
/// ALaserGridPattern의 TargetMode가 Sequence일 때 Sequences 목록에서 하나씩 골라 실행합니다.
/// 패턴을 늘릴 때는 C++ 수정 없이 이 데이터 에셋만 추가하면 됩니다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Gimmicks/Boss/LaserGridTypes.h"
#include "LaserGridSequenceData.generated.h"

UCLASS(BlueprintType)
class MYNICELITTLECAT_API ULaserGridSequenceData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 공격 순서 (위에서부터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid")
	TArray<FLaserGridStep> Steps;

	// 이 시퀀스가 끝난 뒤 다음 시퀀스까지 추가로 쉬는 시간 (초, 마지막 단계의 DelayToNext 뒤에 더함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid", meta = (ClampMin = "0.0", Units = "s"))
	float RestAfter = 1.f;
};
