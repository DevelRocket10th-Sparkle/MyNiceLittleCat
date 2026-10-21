/// 작성자: 우진
/// 작성일자: 2026-10-21
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/HitResult.h"
#include "ReflectiveSurfaceInterface.generated.h"

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
};
