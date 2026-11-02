/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 레이저 빔을 받는 액터가 구현하는 인터페이스입니다 (수신기, 빛에 타는 덤불 등).
/// ULaserBeamComponent가 빔 끝(마지막 구간이 막힌 액터)이 이 인터페이스 구현체이면
/// 닿아 있는 동안 매 틱 ReceiveLaserHit를, 빔이 떠나거나 꺼지면 EndLaserHit를 한 번 호출합니다.
/// 피해(ApplyDamage)와는 별개라 피해량 0인 퍼즐용 빛도 전달됩니다.
/// 맞은 면·들어온 방향(Hit.TraceStart → TraceEnd)·반사 횟수로 받을지는 구현체가 판단합니다.
/// BP 구현체도 호출되도록 Implements<ULaserReceiverInterface>() 확인 후 Execute_XXX로 호출합니다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/HitResult.h"
#include "Gimmicks/Laser/LaserBeamColor.h"
#include "LaserReceiverInterface.generated.h"

class ULaserBeamComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class ULaserReceiverInterface : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API ILaserReceiverInterface
{
	GENERATED_BODY()

public:
	// 빔이 이 액터에 닿아 있는 동안 매 틱 호출 (Hit = 마지막 구간의 충돌, ReflectionCount = 여기까지 반사된 횟수)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LaserReceiver")
	void ReceiveLaserHit(ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount);

	// 빔이 다른 곳으로 가거나 꺼졌을 때 한 번 호출
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LaserReceiver")
	void EndLaserHit(ULaserBeamComponent* Laser);

	// (C++ 전용) 펫 반사판 설치 프리뷰용: 이 빛(색·맞은 면·반사 횟수)을 받으면 켤 출력 레이저와 내보낼 색.
	// 빛을 받아 새 빛을 내보내는 장치(중계·분광기·필터)만 구현, 받지 않거나 출력이 없으면 false
	virtual bool GetPreviewOutputLasers(ELaserBeamColor InputColor, const FHitResult& Hit, int32 ReflectionCount,
		TArray<ULaserBeamComponent*>& OutLasers, ELaserBeamColor& OutColor) const
	{
		return false;
	}
};
