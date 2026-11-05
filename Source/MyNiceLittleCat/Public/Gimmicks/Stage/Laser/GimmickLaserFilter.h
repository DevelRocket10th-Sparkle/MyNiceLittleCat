/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 색 필터입니다 (Archaica 색 필터 참고). 뒤(-X)에서 들어온 빛을 받는 동안 정면(+X)으로 FilterColor 빛을 내보냅니다.
/// 색 규칙: 흰빛(White) → 필터 색으로 바뀜 / 필터와 같은 색 → 그대로 통과 / 다른 색 → 막힘(출력 없음)
/// 색 수신기(AGimmickLaserReceiver AcceptedColors)와 함께 씁니다. 받는 동안 켜짐이라 LinkedActors에도 신호를 보냅니다.
/// 필터 색이 보이도록 메시 머티리얼은 BP에서 지정합니다.
/// 통과형 렌즈(bPassThrough, 2026-11-05 기본 켜짐): 앞뒤 어느 면으로 들어온 빛이든(렌즈 면 기준 InputHalfAngle 안) 받고,
///   출력은 정면 고정이 아니라 들어온 빛의 진행 방향 그대로, 빔이 닿은 지점에서 이어 나갑니다(색만 바뀜). 옆(렌즈 테두리)으로 온 빛은 막음.
///   끄면 예전 방식(뒤에서 받아 정면으로 발사).

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
	virtual bool AcceptsIncomingDirection(const FVector& Incoming) const override;
	virtual void PlaceOutputsForInput(const FVector& InputDirection2D, const FVector& InputLocation) override;
	virtual bool GetPreviewOutputRay(const ULaserBeamComponent* Output, const FHitResult& Hit, FVector& OutStart, FVector& OutDirection) const override;

	// 통과형 출력 시작점: 빔이 닿은 지점의 수평 위치, 높이는 출력 레이저 높이
	FVector MakePassThroughStart(const FVector& InputLocation) const;

protected:
	// 통과형 렌즈: 앞뒤 양면으로 받고 들어온 방향 그대로 이어서 내보냄 (끄면 뒤에서 받아 정면으로)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice")
	bool bPassThrough = true;

	// 통과한 빛 (컴포넌트 정면으로 발사)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserBeamComponent> OutputLaser;

	// 통과한 빛의 색 (White면 색을 바꾸지 않는 통로)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice")
	ELaserBeamColor FilterColor = ELaserBeamColor::Red;
};
