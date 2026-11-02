/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 분광기입니다 (Archaica 빛 나누기 참고). 뒤(-X)에서 들어온 빛을 받는 동안 같은 색 빛 두 갈래를 내보냅니다.
/// 두 갈래는 정면(+X) 기준 좌우 SplitAngle 방향입니다 (기본 90° = 들어온 빛과 수직으로 양쪽).
/// 받는 면은 InputDirection 화살표(기본 뒤쪽), 허용 각도는 InputHalfAngle. 받는 동안 켜짐이라 LinkedActors에도 신호를 보냅니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/Stage/Laser/GimmickLaserDevice.h"
#include "GimmickLaserSplitter.generated.h"

UCLASS()
class MYNICELITTLECAT_API AGimmickLaserSplitter : public AGimmickLaserDevice
{
	GENERATED_BODY()

public:
	AGimmickLaserSplitter();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetOutputLasers(TArray<ULaserBeamComponent*>& OutLasers) const override;

	// 두 출력의 방향을 SplitAngle에 맞춤
	void ApplySplitAngle();

protected:
	// 정면 기준 왼쪽(-SplitAngle)으로 나가는 빛
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserBeamComponent> OutputLeft;

	// 정면 기준 오른쪽(+SplitAngle)으로 나가는 빛
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserBeamComponent> OutputRight;

	// 정면(+X)에서 두 갈래가 벌어지는 각도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float SplitAngle = 90.f;
};
