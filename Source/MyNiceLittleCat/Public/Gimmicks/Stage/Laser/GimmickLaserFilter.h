/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 색 필터입니다 (Archaica 색 필터 참고). 뒤(-X)에서 들어온 빛을 받는 동안 정면(+X)으로 FilterColor 빛을 내보냅니다.
/// 색 규칙: 흰빛(White) → 필터 색으로 바뀜 / 필터와 같은 색 → 그대로 통과 / 다른 색 → 막힘(출력 없음)
/// 색 수신기(AGimmickLaserReceiver AcceptedColors)와 함께 씁니다. 받는 동안 켜짐이라 LinkedActors에도 신호를 보냅니다.
/// 필터 색이 보이도록 메시 머티리얼은 BP에서 지정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/Stage/Laser/GimmickLaserDevice.h"
#include "GimmickLaserFilter.generated.h"

UCLASS()
class MYNICELITTLECAT_API AGimmickLaserFilter : public AGimmickLaserDevice
{
	GENERATED_BODY()

public:
	AGimmickLaserFilter();

	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserDevice")
	ELaserBeamColor GetFilterColor() const { return FilterColor; }

protected:
	virtual void GetOutputLasers(TArray<ULaserBeamComponent*>& OutLasers) const override;
	virtual bool ComputeOutputColor(ELaserBeamColor InputColor, ELaserBeamColor& OutColor) const override;

protected:
	// 통과한 빛 (컴포넌트 정면으로 발사)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserBeamComponent> OutputLaser;

	// 통과한 빛의 색 (White면 색을 바꾸지 않는 통로)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice")
	ELaserBeamColor FilterColor = ELaserBeamColor::Red;
};
