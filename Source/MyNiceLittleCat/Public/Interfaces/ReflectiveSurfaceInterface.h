/// 작성자: 우진
/// 작성일자: 2026-10-21
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/HitResult.h"
#include "ReflectiveSurfaceInterface.generated.h"

class AActor;

UINTERFACE(MinimalAPI)
class UReflectiveSurfaceInterface : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IReflectiveSurfaceInterface
{
	GENERATED_BODY()	

public:
    // true면 반사 가능. OutDirection에 반사 방향을 반환
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reflection")
    bool TryGetReflection(const FVector& IncomingDirection, const FHitResult& Hit, FVector& OutDirection) const;

    virtual bool TryGetReflection_Implementation(const FVector& IncomingDirection, const FHitResult& Hit, FVector& OutDirection) const
    {
        OutDirection = FVector::ZeroVector;
        return false;
    }

    // [추가: 최요환 2026-10-27] 반사판 파괴형 레이저에 맞았을 때 호출됩니다.
    // 레이저 종류 (팀 합의 2026-10-27):
    //  - 반사형 (ULaserBeamComponent::bEnableReflection): 스테이지 기믹·함정, 보스 단일 레이저 → TryGetReflection으로 반사
    //  - 파괴형 (ULaserBeamComponent::bBreakReflectors): 보스 격자 레이저 → 반사하지 않고 이 함수로 반사판을 부숨
    //    (펫 반사판을 격자 레이저 방패로 쓰지 못하게 하기 위함)
    // true를 반환하면 부서진 것으로 보고 레이저는 피해를 주지 않음 (다음 틱부터 빔이 그 자리를 지나감)
    // false면 부서지지 않는 반사판 → 레이저는 일반 물체처럼 막힘. 기본 구현은 false (기존 구현체 동작 그대로)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reflection")
    bool BreakByLaser(AActor* LaserInstigator);

    virtual bool BreakByLaser_Implementation(AActor* LaserInstigator)
    {
        return false;
    }
};
