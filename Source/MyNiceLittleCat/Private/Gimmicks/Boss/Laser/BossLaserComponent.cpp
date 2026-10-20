/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 보스 레이저 컴포넌트 구현

#include "Gimmicks/Boss/Laser/BossLaserComponent.h"
#include "Gimmicks/Boss/DamageTypes/DamageType_BossLaser.h"
#include "Characters/PlayerCharacter.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UBossLaserComponent::UBossLaserComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	DamageTypeClass = UDamageType_BossLaser::StaticClass();
}

void UBossLaserComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLaser();

	Super::EndPlay(EndPlayReason);
}

void UBossLaserComponent::StartLaser(const FVector& TargetLocation, float Duration)
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

void UBossLaserComponent::StartLaserAtActor(AActor* TargetActor, float Duration)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	StartLaser(TargetActor->GetActorLocation(), Duration);
}

void UBossLaserComponent::StopLaser()
{
	if (!bLaserActive)
	{
		return;
	}

	bLaserActive = false;
	HitActor.Reset();
	SetComponentTickEnabled(false);

	OnLaserStopped.Broadcast(this);
}

void UBossLaserComponent::SetTargetLocation(const FVector& TargetLocation)
{
	SetFireDirection(TargetLocation - GetComponentLocation());
}

void UBossLaserComponent::SetFireDirection(const FVector& Direction)
{
	FVector NewDirection = Direction;
	if (bPlanarAim)
	{
		NewDirection.Z = 0.f;
	}

	// 방향이 없으면(목표가 발사 위치와 같음) 컴포넌트 정면으로
	FireDirection = NewDirection.IsNearlyZero() ? GetForwardVector() : NewDirection.GetSafeNormal();
}

void UBossLaserComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bLaserActive)
	{
		return;
	}

	// 보스나 막는 물체가 움직이면 끝 지점이 바뀌므로 매 틱 다시 계산
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

void UBossLaserComponent::UpdateBeam()
{
	BeamStart = GetComponentLocation();
	BeamEnd = BeamStart + FireDirection * MaxRange;
	HitActor.Reset();

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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossLaser), false, GetOwner());

	FHitResult Hit;
	const bool bHit = BeamRadius > 0.f
		? World->SweepSingleByObjectType(Hit, BeamStart, BeamEnd, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(BeamRadius), QueryParams)
		: World->LineTraceSingleByObjectType(Hit, BeamStart, BeamEnd, ObjectParams, QueryParams);

	if (bHit)
	{
		BeamEnd = Hit.Location;
		HitActor = Hit.GetActor();
	}
}

void UBossLaserComponent::ApplyLaserDamage()
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

void UBossLaserComponent::DrawDebugBeam() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DrawDebugLine(World, BeamStart, BeamEnd, FColor::Red, false, 0.f, 0, FMath::Max(BeamRadius * 0.5f, 2.f));

	if (HitActor.IsValid())
	{
		DrawDebugPoint(World, BeamEnd, 12.f, FColor::Red, false, 0.f);
	}
#endif
}

bool UBossLaserComponent::IsActorOfClasses(const AActor* Actor, const TArray<TSubclassOf<AActor>>& Classes)
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
