/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 레이저 빔 컴포넌트 구현

#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Gimmicks/Boss/DamageTypes/DamageType_BossLaser.h"
#include "Characters/PlayerCharacter.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Interfaces/ReflectiveSurfaceInterface.h"
//#include "CollisionQueryParams.h"
//#include "CollisionShape.h"

ULaserBeamComponent::ULaserBeamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	DamageTypeClass = UDamageType_BossLaser::StaticClass();
}

void ULaserBeamComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLaser();

	Super::EndPlay(EndPlayReason);
}

void ULaserBeamComponent::StartLaser(const FVector& TargetLocation, float Duration)
{
	SetTargetLocation(TargetLocation);

	RemainingTime = Duration > 0.f ? Duration : LaserDuration;
	LastDamagedActor.Reset();
	LastDamageTime = -1.f;

	const bool bWasActive = bLaserActive;
	bLaserActive = true;
	SetComponentTickEnabled(true);

	UpdateBeam();

	if (!bWasActive)
	{
		OnLaserStarted.Broadcast(this);
	}
}

void ULaserBeamComponent::StartLaserAtActor(AActor* TargetActor, float Duration)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	StartLaser(TargetActor->GetActorLocation(), Duration);
}

void ULaserBeamComponent::StartLaserForward(float Duration)
{
	StartLaser(GetComponentLocation() + GetForwardVector() * 100.f, Duration);
}

void ULaserBeamComponent::StopLaser()
{
	if (!bLaserActive)
	{
		return;
	}

	bLaserActive = false;
	HitActor.Reset();
	LastHit = FHitResult();
	BeamSegments.Reset();
	SetComponentTickEnabled(false);

	OnLaserStopped.Broadcast(this);
}

void ULaserBeamComponent::SetTargetLocation(const FVector& TargetLocation)
{
	SetFireDirection(TargetLocation - GetComponentLocation());
}

void ULaserBeamComponent::SetFireDirection(const FVector& Direction)
{
	// 컴포넌트 회전을 따라가는 중이면 방향은 UpdateBeam에서 정함
	if (bFollowComponentRotation)
	{
		return;
	}

	FireDirection = MakeFireDirection(Direction);
}

FVector ULaserBeamComponent::MakeFireDirection(const FVector& Direction) const
{
	FVector NewDirection = Direction;
	if (bPlanarAim)
	{
		NewDirection.Z = 0.f;
	}

	// 방향이 없으면(목표가 발사 위치와 같음, 수평 발사인데 정면이 수직) 컴포넌트 정면으로
	return NewDirection.IsNearlyZero() ? GetForwardVector() : NewDirection.GetSafeNormal();
}

void ULaserBeamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bLaserActive)
	{
		return;
	}

	// 발사하는 액터나 막는 물체가 움직이면 끝 지점이 바뀌므로 매 틱 다시 계산
	UpdateBeam();
	ApplyLaserDamage();

	if (bDrawDebugBeam)
	{
		DrawDebugBeam();
	}

	if (RemainingTime > 0.f)
	{
		RemainingTime -= DeltaTime;
		if (RemainingTime <= 0.f)
		{
			StopLaser();
		}
	}
}

void ULaserBeamComponent::UpdateBeam()
{
	if (bFollowComponentRotation)
	{
		FireDirection = MakeFireDirection(GetForwardVector());
	}

	BeamSegments.Reset();
	BeamStart = GetComponentLocation();
	BeamEnd = BeamStart; //. +FireDirection * MaxRange;
	HitActor.Reset();
	LastHit = FHitResult();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : TraceObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}

	FVector SegmentStart = BeamStart;
	FVector SegmentDirection = FireDirection.GetSafeNormal();
	float RemainingRange = FMath::Max(0.f, MaxRange);
	int32 ReflectionCount = 0;

	while ((RemainingRange > KINDA_SMALL_NUMBER) && (false == SegmentDirection.IsNearlyZero()))
	{
		const FVector TraceEnd = SegmentStart + SegmentDirection * RemainingRange;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LaserBeam), false, GetOwner());
		FHitResult Hit;
		const bool bHit = BeamRadius > 0.f
			? World->SweepSingleByObjectType(Hit, SegmentStart, TraceEnd, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(BeamRadius), QueryParams)
			: World->LineTraceSingleByObjectType(Hit, SegmentStart, TraceEnd, ObjectParams, QueryParams);

		FLaserBeamSegment& Segment = BeamSegments.AddDefaulted_GetRef(); // 직선 저장

		Segment.Start = SegmentStart;
		Segment.End = (true == bHit) ? FVector(Hit.Location) : TraceEnd;
		Segment.Direction = SegmentDirection;
		Segment.Hit = Hit;
		Segment.ReflectionCount = ReflectionCount;

		if (0 == ReflectionCount)
		{
			BeamEnd = Segment.End;
			HitActor = Hit.GetActor();
			LastHit = Hit;
		}

		// 아무것도 맞지 않았으면 경로 계산 종료
		if (false == bHit || false == Hit.bBlockingHit || true == Hit.bStartPenetrating)
			break;

		// 반사 옵션 또는 계산 횟수 제한 확인
		if (false == bEnableReflection || ReflectionCount >= FMath::Max(0, MaxReflectionCount))
			break;

		AActor* SurfaceActor = Hit.GetActor();
		if (false == IsValid(SurfaceActor) ||
			false == SurfaceActor->GetClass()->ImplementsInterface(UReflectiveSurfaceInterface::StaticClass()))
			break;

		// 레이저에 맞은 물체에게 반사 가능 여부, 방향을 물어봄
		FVector ReflectedDirection = FVector::ZeroVector;
		const bool bCanReflect = IReflectiveSurfaceInterface::Execute_TryGetReflection(
			SurfaceActor, SegmentDirection, Hit, ReflectedDirection);
		if (false == bCanReflect || true == ReflectedDirection.ContainsNaN()) break;

		ReflectedDirection = ReflectedDirection.GetSafeNormal();
		if (true == ReflectedDirection.IsNearlyZero()) break;

		RemainingRange -= FVector::Distance(SegmentStart, Segment.End);

		// 스윕의 중심 위치에서 표면 바깥으로 조금 이동
		const float Offset = FMath::Max(0.1f, ReflectionStartOffset);
		const FVector SurfaceNormal = FVector(Hit.ImpactNormal).GetSafeNormal();

		SegmentStart = FVector(Hit.Location) + SurfaceNormal * Offset;

		RemainingRange -= Offset;
		SegmentDirection = ReflectedDirection;
		++ReflectionCount;
	}
}

void ULaserBeamComponent::ApplyLaserDamage()
{
	AActor* Target = HitActor.Get();
	if (!IsValid(Target) || !IsActorOfClasses(Target, DamageTargetClasses) || DamagePerHit <= 0.f)
	{
		return;
	}

	// 새 대상은 닿은 즉시 한 번, 같은 대상에 계속 닿아 있으면 DamageInterval마다
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bSameTarget = LastDamagedActor.Get() == Target;
	if (bSameTarget && LastDamageTime >= 0.f && Now - LastDamageTime < DamageInterval)
	{
		return;
	}
	LastDamageTime = Now;
	LastDamagedActor = Target;

	// TODO(합의): 플레이어가 TakeDamage를 처리하게 되면 아래 ApplyDamage로 통일
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(Target))
	{
		// 빔 방향(수평)의 오른쪽 수직 벡터: Up × Forward
		const FVector BeamDir = FireDirection.GetSafeNormal2D();
		FVector PushDir = FVector::CrossProduct(FVector::UpVector, BeamDir);

		// 플레이어가 빔 중심선의 왼쪽에 있으면 왼쪽으로 (빔 방향으로 밀면 계속 맞으므로 빔 밖으로 밀어냄)
		const FVector ToPlayer = Player->GetActorLocation() - BeamStart;
		if (FVector::DotProduct(ToPlayer, PushDir) < 0.f)
		{
			PushDir = -PushDir;
		}

		// 피해·경직·무적은 플레이어 쪽에서 처리 (무적 중이면 무시됨), 빔 옆으로 밀림
		Player->ApplyPlayerHit(DamagePerHit, PushDir);
		OnLaserDamaged.Broadcast(this, Target, DamagePerHit);
		return;
	}

	AActor* OwnerActor = GetOwner();
	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	const float AppliedDamage = UGameplayStatics::ApplyDamage(Target, DamagePerHit, InstigatorController, OwnerActor, DamageTypeClass);
	if (AppliedDamage > 0.f)
	{
		OnLaserDamaged.Broadcast(this, Target, AppliedDamage);
	}
}

void ULaserBeamComponent::DrawDebugBeam() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}


	for (const FLaserBeamSegment& Segment : BeamSegments)
	{
		const FColor DebugColor = (0 == Segment.ReflectionCount) ? FColor::Red : FColor::Green;

		DrawDebugLine(World, Segment.Start, Segment.End, DebugColor, false, 0.f, 0, FMath::Max(BeamRadius * 0.5f, 2.f));

		if(Segment.Hit.bBlockingHit)
			DrawDebugPoint(World, Segment.End, 12.f, DebugColor, false, 0.f);
	}
#endif
}

bool ULaserBeamComponent::IsActorOfClasses(const AActor* Actor, const TArray<TSubclassOf<AActor>>& Classes)
{
	if (Classes.IsEmpty())
	{
		return true;
	}

	for (const TSubclassOf<AActor>& Class : Classes)
	{
		if (Class && Actor->IsA(Class))
		{
			return true;
		}
	}

	return false;
}
