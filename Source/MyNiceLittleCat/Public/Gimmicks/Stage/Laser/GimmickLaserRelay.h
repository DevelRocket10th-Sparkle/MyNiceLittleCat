/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 빛 중계 장치입니다 (Portal 2 레이저 중계 참고). 빛을 받는 동안 정면(+X)으로 같은 색 빛을 다시 내보냅니다.
/// 기본은 어느 면으로 들어온 빛이든 받습니다(bAcceptAnySide). 받는 동안 켜짐이라 LinkedActors(문·표시등)에도 신호를 보냅니다.
/// 회전 거울처럼 돌릴 수는 없으며, 내보내는 방향은 배치 회전으로 정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/Stage/Laser/GimmickLaserDevice.h"
#include "GimmickLaserRelay.generated.h"

UCLASS()
class MYNICELITTLECAT_API AGimmickLaserRelay : public AGimmickLaserDevice
{
	GENERATED_BODY()

public:
	AGimmickLaserRelay();

protected:
	virtual void GetOutputLasers(TArray<ULaserBeamComponent*>& OutLasers) const override;

protected:
	// 내보내는 빛 (컴포넌트 정면으로 발사)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserBeamComponent> OutputLaser;
};
