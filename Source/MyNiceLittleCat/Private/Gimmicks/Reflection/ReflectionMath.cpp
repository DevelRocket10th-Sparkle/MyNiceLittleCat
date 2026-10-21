#include "Gimmicks/Reflection/ReflectionMath.h"
#include "Chaos/AABB.h"

bool FReflectionMath::TryReflectFrontFace(const FVector& IncomingDirection, 
	const FVector& FrontNormal, const FVector& HitNormal, FVector& OutDirection)
{
	OutDirection = FVector::ZeroVector;

	const FVector Direction = IncomingDirection.GetSafeNormal(); // 들어온 방향 (입사 벡터)
	const FVector Front = FrontNormal.GetSafeNormal(); // 앞면 (반사판의 법선 벡터)
	const FVector Surface = HitNormal.GetSafeNormal();

	if (true == Direction.IsNearlyZero() || true == Front.IsNearlyZero() || true == Surface.IsNearlyZero())
		return false;

	// 실제 맞은 면이 지정한 앞면과 같은 방향인가?
	constexpr float FrontFaceThreshold = 0.99f; // 거의 1에 가까운 (적당한 앞면 오차 각도를 허용)
	if (FrontFaceThreshold > FVector::DotProduct(Surface, Front)) return false;

	// 앞면을 향해 들어온 것인가?
	const float IncomingDot = FVector::DotProduct(Direction, Front);
	if (IncomingDot >= -KINDA_SMALL_NUMBER) return false;

	OutDirection = (Direction - 2.f * IncomingDot * Front).GetSafeNormal();

	return (false == OutDirection.IsNearlyZero());
}

bool FReflectionMath::TraceVirtualSurface(
    const FVector& Start, const FVector& End, float BeamRadius, const FVirtualReflectiveSurface& Surface, FHitResult& OutHit)
{
    OutHit = FHitResult();

    const FVector Delta = End - Start;
    const double Length = Delta.Size();

    if (Length <= KINDA_SMALL_NUMBER || Surface.HalfExtents.GetMin() <= 0.f) return false;

    const FVector Direction = Delta / Length;
    const FQuat Rotation = Surface.Rotation.GetNormalized();

    // 회전한 박스를 검사하기 위해 레이저를 박스 로컬 공간으로 변환
    const FVector LocalStart = Rotation.UnrotateVector(Start - Surface.Center);
    const FVector LocalDirection = Rotation.UnrotateVector(Direction);

    const Chaos::FAABB3 Box(-Surface.HalfExtents, Surface.HalfExtents);

    Chaos::FReal HitDistance = 0;
    Chaos::FVec3 LocalImpactPoint(0);
    Chaos::FVec3 LocalImpactNormal(0);
    int32 FaceIndex = INDEX_NONE;

    const bool bHit = Box.Raycast(LocalStart, LocalDirection, Length, 
        FMath::Max(0.f, BeamRadius), HitDistance, LocalImpactPoint, LocalImpactNormal, FaceIndex);

    if (false == bHit) return false;

    OutHit.bBlockingHit = true;
    OutHit.bStartPenetrating = HitDistance <= 0;
    OutHit.TraceStart = Start;
    OutHit.TraceEnd = End;
    OutHit.Distance = FMath::Clamp(static_cast<float>(HitDistance), 0.f, static_cast<float>(Length));
    OutHit.Time = OutHit.Distance / Length;

    // 레이저 중심이 충돌 시점에 도달한 위치
    OutHit.Location = Start + Direction * OutHit.Distance;

    if (true == OutHit.bStartPenetrating)
    {
        // 시작부터 겹친 경우 반사에는 사용하지 않음
        OutHit.ImpactPoint = Start;
        OutHit.ImpactNormal = -Direction;
    }
    else
    {
        OutHit.ImpactPoint = Surface.Center + Rotation.RotateVector(FVector(LocalImpactPoint));
        OutHit.ImpactNormal = Rotation.RotateVector(FVector(LocalImpactNormal)).GetSafeNormal();
    }

    OutHit.Normal = OutHit.ImpactNormal;
    return true;
}