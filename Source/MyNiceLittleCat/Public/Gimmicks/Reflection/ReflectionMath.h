// 작성자: 우진
// 작성일자: 2026-10-21

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

// 실제 액터 없이 계산에만 사용하는 반사판
struct MYNICELITTLECAT_API FVirtualReflectiveSurface
{
    FVector Center = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;

    // 실제 설치될 충돌 박스의 반크기. 스케일 반영 완료된 값
    FVector HalfExtents = FVector::ZeroVector;
};

struct MYNICELITTLECAT_API FReflectionMath
{
    // 앞면에 들어온 경우에만 반사 방향 계산
    static bool TryReflectFrontFace(const FVector& IncomingDirection, const FVector& FrontNormal,
        const FVector& HitNormal, FVector& OutDirection);

    // 레이저 구간과 가상 박스의 충돌 검사
    static bool TraceVirtualSurface(const FVector& Start, const FVector& End,
        float BeamRadius, const FVirtualReflectiveSurface& Surface, FHitResult& OutHit);
};