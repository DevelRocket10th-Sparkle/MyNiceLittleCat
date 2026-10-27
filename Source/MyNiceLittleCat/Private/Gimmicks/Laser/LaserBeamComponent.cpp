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
#include "Gimmicks/Boss/DamageTypes/DamageType_ReflectedLaser.h"
#include "Gimmicks/Reflection/ReflectionMath.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

ULaserBeamComponent::ULaserBeamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	DamageTypeClass = UDamageType_BossLaser::StaticClass();
	ReflectedDamageTypeClass = UDamageType_ReflectedLaser::StaticClass();
}

void ULaserBeamComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLaser();
	DestroyBeamEffects();

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
	DeactivateBeamEffects();

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
	if (true == bFollowComponentRotation)
		FireDirection = MakeFireDirection(GetForwardVector());

	// 계산 결과를 실제 레이저 배열에 저장
	CalculateBeamPath(BeamSegments);

	BeamStart = GetComponentLocation();
	BeamEnd = BeamStart;
	HitActor.Reset();
	LastHit = FHitResult();

	// 기존 조회 함수는 원본 구간 정보를 유지
	if (false == BeamSegments.IsEmpty())
	{
		const FLaserBeamSegment& FirstSegment = BeamSegments[0];

		BeamEnd = FirstSegment.End;
		HitActor = FirstSegment.Hit.GetActor();
		LastHit = FirstSegment.Hit;
	}

	if (bLaserActive)
	{
		UpdateBeamEffects();
	}
}

void ULaserBeamComponent::SetVisualSettings(const FLaserBeamVisualSettings& NewSettings)
{
	// 이펙트 에셋이 바뀔 수 있으므로 기존 컴포넌트는 버리고 다시 만듦
	DestroyBeamEffects();
	VisualSettings = NewSettings;

	if (bLaserActive)
	{
		UpdateBeamEffects();
	}
}

UNiagaraComponent* ULaserBeamComponent::CreateEffectComponent(UNiagaraSystem* System)
{
	if (!System)
	{
		return nullptr;
	}

	// 위치는 매 틱 월드 좌표 파라미터로 넣으므로 붙이기만 하고, 켜고 끄기는 직접 관리
	return UNiagaraFunctionLibrary::SpawnSystemAttached(System, this, NAME_None,
		FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy*/ false, /*bAutoActivate*/ false, ENCPoolMethod::None, /*bPreCullCheck*/ false);
}

void ULaserBeamComponent::UpdateBeamEffects()
{
	const float BeamWidth = FMath::Max(BeamRadius * 2.f * VisualSettings.BeamWidthScale, VisualSettings.MinBeamWidth);

	int32 UsedCount = 0;
	if (VisualSettings.BeamEffect)
	{
		for (const FLaserBeamSegment& Segment : BeamSegments)
		{
			if (!BeamEffectComponents.IsValidIndex(UsedCount))
			{
				BeamEffectComponents.Add(nullptr);
			}

			UNiagaraComponent* Effect = BeamEffectComponents[UsedCount];
			if (!IsValid(Effect))
			{
				Effect = CreateEffectComponent(VisualSettings.BeamEffect);
				if (!Effect)
				{
					break;
				}
				BeamEffectComponents[UsedCount] = Effect;
			}

			const FLinearColor& Color = (0 == Segment.ReflectionCount) ? VisualSettings.BeamColor : VisualSettings.ReflectedBeamColor;

			// 이펙트를 구간 시작점으로 옮김: 시작점 파라미터 없이 시스템 위치를 빔 시작으로 쓰는 이펙트(NS_Laser_Basic 등)도
			// 반사 구간이 반사 지점에서 시작하도록 (붙인 채로 두면 모든 구간이 발사 위치에서 시작해 보임)
			Effect->SetWorldLocationAndRotation(Segment.Start, Segment.Direction.Rotation());

			// 엔진 Beam Emitter Setup 입력이 Vector 타입이라 월드 좌표를 Vector로 넘김 (Absolute Beam Start/End 켬)
			Effect->SetVariableVec3(VisualSettings.BeamStartParam, Segment.Start);
			Effect->SetVariableVec3(VisualSettings.BeamEndParam, Segment.End);
			Effect->SetVariableFloat(VisualSettings.BeamWidthParam, BeamWidth);
			Effect->SetVariableLinearColor(VisualSettings.BeamColorParam, Color);

			if (!Effect->IsActive())
			{
				Effect->Activate(true);
			}
			++UsedCount;
		}
	}

	// 반사가 줄어 남는 구간 이펙트는 끔
	for (int32 Index = UsedCount; Index < BeamEffectComponents.Num(); ++Index)
	{
		UNiagaraComponent* Effect = BeamEffectComponents[Index];
		if (IsValid(Effect) && Effect->IsActive())
		{
			Effect->Deactivate();
		}
	}

	// 충돌 이펙트: 마지막 구간이 막힌 지점
	const FLaserBeamSegment* LastSegment = BeamSegments.IsEmpty() ? nullptr : &BeamSegments.Last();
	const bool bShowImpact = VisualSettings.ImpactEffect && LastSegment && LastSegment->Hit.bBlockingHit;
	if (!bShowImpact)
	{
		if (IsValid(ImpactEffectComponent) && ImpactEffectComponent->IsActive())
		{
			ImpactEffectComponent->Deactivate();
		}
		return;
	}

	if (!IsValid(ImpactEffectComponent))
	{
		ImpactEffectComponent = CreateEffectComponent(VisualSettings.ImpactEffect);
		if (!ImpactEffectComponent)
		{
			return;
		}
	}

	const FVector ImpactNormal = FVector(LastSegment->Hit.ImpactNormal).GetSafeNormal();
	const FRotator ImpactRotation = ImpactNormal.IsNearlyZero()
		? (-LastSegment->Direction).Rotation() : ImpactNormal.Rotation();
	ImpactEffectComponent->SetWorldLocationAndRotation(FVector(LastSegment->Hit.ImpactPoint), ImpactRotation);

	const FLinearColor& ImpactColor = (0 == LastSegment->ReflectionCount) ? VisualSettings.BeamColor : VisualSettings.ReflectedBeamColor;
	ImpactEffectComponent->SetVariableLinearColor(VisualSettings.BeamColorParam, ImpactColor);

	if (!ImpactEffectComponent->IsActive())
	{
		ImpactEffectComponent->Activate(true);
	}
}

void ULaserBeamComponent::DeactivateBeamEffects()
{
	for (UNiagaraComponent* Effect : BeamEffectComponents)
	{
		if (IsValid(Effect))
		{
			Effect->Deactivate();
		}
	}

	if (IsValid(ImpactEffectComponent))
	{
		ImpactEffectComponent->Deactivate();
	}
}

void ULaserBeamComponent::DestroyBeamEffects()
{
	for (UNiagaraComponent* Effect : BeamEffectComponents)
	{
		if (IsValid(Effect))
		{
			Effect->DestroyComponent();
		}
	}
	BeamEffectComponents.Reset();

	if (IsValid(ImpactEffectComponent))
	{
		ImpactEffectComponent->DestroyComponent();
	}
	ImpactEffectComponent = nullptr;
}

void ULaserBeamComponent::CalculateBeamPath(TArray<FLaserBeamSegment>& OutSegments, const FVirtualReflectiveSurface* VirtualSurface) const
{
	OutSegments.Reset();

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

	FVector SegmentStart = GetComponentLocation();
	FVector SegmentDirection = (true == bFollowComponentRotation) ? 
		MakeFireDirection(GetForwardVector()).GetSafeNormal() : FireDirection.GetSafeNormal();
	float RemainingRange = FMath::Max(0.f, MaxRange);
	int32 ReflectionCount = 0;

	while ((RemainingRange > KINDA_SMALL_NUMBER) && (false == SegmentDirection.IsNearlyZero()))
	{
		const FVector TraceEnd = SegmentStart + SegmentDirection * RemainingRange;

		// 원본 구간은 발사자 자신을 무시. / 반사된 구간은 발사자도 맞힐 수 있음.
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LaserBeam), false);
		if (0 == ReflectionCount)
			QueryParams.AddIgnoredActor(GetOwner());

		FHitResult Hit;

		// 먼저 실제 월드와 충돌 검사
		bool bHit = BeamRadius > 0.f ? 
			World->SweepSingleByObjectType(Hit, SegmentStart, TraceEnd, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(BeamRadius), QueryParams)
			: World->LineTraceSingleByObjectType(Hit, SegmentStart, TraceEnd, ObjectParams, QueryParams);

		bool bHitVirtualSurface = false;

		if (nullptr != VirtualSurface)
		{
			FHitResult VirtualHit;
			const bool bVirtualHit = FReflectionMath::TraceVirtualSurface(SegmentStart, TraceEnd, BeamRadius, *VirtualSurface, VirtualHit);

			// 실제 물체보다 가상 반사판이 먼저라면 충돌 결과 교체
			// 거의 같은 거리라면 실제 월드 충돌을 우선
			if (true == bVirtualHit
				&& (false == bHit || VirtualHit.Distance + KINDA_SMALL_NUMBER < Hit.Distance))
			{
				Hit = VirtualHit;
				bHit = true;
				bHitVirtualSurface = true;
			}
		}

		FLaserBeamSegment& Segment = OutSegments.AddDefaulted_GetRef(); // 직선 저장
		Segment.Start = SegmentStart;
		Segment.End = (true == bHit) ? FVector(Hit.Location) : TraceEnd;
		Segment.Direction = SegmentDirection;
		Segment.Hit = Hit;
		Segment.ReflectionCount = ReflectionCount;
		Segment.bHitVirtualSurface = bHitVirtualSurface;

		// 아무것도 맞지 않았으면 경로 계산 종료
		if (false == bHit || false == Hit.bBlockingHit || true == Hit.bStartPenetrating)
			break;

		// 반사 옵션 또는 계산 횟수 제한 확인
		if (false == bEnableReflection || ReflectionCount >= FMath::Max(0, MaxReflectionCount))
			break;

		//AActor* SurfaceActor = Hit.GetActor();
		//if (false == IsValid(SurfaceActor) ||
		//	false == SurfaceActor->GetClass()->ImplementsInterface(UReflectiveSurfaceInterface::StaticClass()))
		//	break;

		//// 레이저에 맞은 물체에게 반사 가능 여부, 방향을 물어봄
		//FVector ReflectedDirection = FVector::ZeroVector;
		//const bool bCanReflect = IReflectiveSurfaceInterface::Execute_TryGetReflection(
		//	SurfaceActor, SegmentDirection, Hit, ReflectedDirection);
		//if (false == bCanReflect || true == ReflectedDirection.ContainsNaN()) break;
		FVector ReflectedDirection = FVector::ZeroVector;
		bool bCanReflect = false;

		if (true == bHitVirtualSurface)
		{
			// 가상 반사판에는 실제 액터가 없으므로 공통 계산 함수 사용
			const FVector FrontNormal = VirtualSurface->Rotation.GetNormalized().GetForwardVector();
			bCanReflect = FReflectionMath::TryReflectFrontFace(SegmentDirection, FrontNormal, FVector(Hit.ImpactNormal), ReflectedDirection);
		}
		else
		{
			// 실제 물체는 기존 인터페이스 사용
			AActor* SurfaceActor = Hit.GetActor();

			if (false == IsValid(SurfaceActor) || 
				false == SurfaceActor->GetClass()->ImplementsInterface(UReflectiveSurfaceInterface::StaticClass()))
				break;

			bCanReflect = IReflectiveSurfaceInterface::Execute_TryGetReflection(SurfaceActor, SegmentDirection, Hit, ReflectedDirection);
		}

		if (false == bCanReflect || true == ReflectedDirection.ContainsNaN())
			break;

		// [추가: 최요환 2026-10-27] 반사 구간을 수평으로 폄 (반사 계산 자체는 반사판 쪽 그대로).
		// 반사판은 바닥에 수직이라 3D 반사는 아래로 쏜 빔(보스 단일 레이저)의 하강 성분을 그대로 유지해 반사판 뒤 바닥에 바로 꽂힘
		if (bPlanarReflection)
		{
			ReflectedDirection.Z = 0.f;
		}

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
	if (true == BeamSegments.IsEmpty())
	{
		LastDamagedActor.Reset();
		LastDamageTime = -1.f;
		return;
	}

	// 피해 콜백에서 경로가 변경되어도 안전하도록 복사
	const FLaserBeamSegment Segment = BeamSegments.Last();
	AActor* Target = Segment.Hit.GetActor();

	// 파괴형: 반사판에 막혔으면 피해 대신 부숨 (피해량 0이어도 동작). 부서지지 않는 반사판이면 아래 일반 피해 처리
	if (bBreakReflectors && false == bEnableReflection && Segment.Hit.bBlockingHit && IsValid(Target)
		&& Target->GetClass()->ImplementsInterface(UReflectiveSurfaceInterface::StaticClass()))
	{
		if (IReflectiveSurfaceInterface::Execute_BreakByLaser(Target, GetOwner()))
		{
			UE_LOG(LogTemp, Log, TEXT("[LaserBeam] %s: 반사판 파괴 %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Target));
			LastDamagedActor.Reset();
			LastDamageTime = -1.f;
			return;
		}
	}

	if (0.f >= DamagePerHit) return;

	if (false == Segment.Hit.bBlockingHit || false == IsValid(Target) || 
		false == IsActorOfClasses(Target, DamageTargetClasses))
	{
		LastDamagedActor.Reset();
		LastDamageTime = -1.f;
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
		// 맞은 구간 방향(수평)의 오른쪽 수직 벡터: Up × Forward (반사된 구간이면 반사 방향 기준)
		const FVector BeamDir = Segment.Direction.GetSafeNormal2D();
		FVector PushDir = FVector::CrossProduct(FVector::UpVector, BeamDir);

		// 플레이어가 빔 중심선의 왼쪽에 있으면 왼쪽으로 (빔 방향으로 밀면 계속 맞으므로 빔 밖으로 밀어냄)
		const FVector ToPlayer = Player->GetActorLocation() - Segment.Start;
		if (FVector::DotProduct(ToPlayer, PushDir) < 0.f)
		{
			PushDir = -PushDir;
		}

		// 피해·경직·무적은 플레이어 쪽에서 처리 (무적 중이면 무시됨), 빔 옆으로 밀림
		Player->ApplyPlayerHit(DamagePerHit, PushDir);
		OnLaserDamaged.Broadcast(this, Target, DamagePerHit);
		return;
	}

	const TSubclassOf<UDamageType> HitDamageType = Segment.ReflectionCount > 0
		? ReflectedDamageTypeClass : DamageTypeClass;

	AActor* OwnerActor = GetOwner();
	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	const float AppliedDamage = UGameplayStatics::ApplyDamage(
		Target, DamagePerHit, InstigatorController, OwnerActor, HitDamageType);

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
