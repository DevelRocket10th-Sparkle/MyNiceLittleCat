// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyCharacterMovementComponent.h"

#include "DrawDebugHelpers.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Enemy/EnemyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"

bool UEnemyCharacterMovementComponent::CanStartPathFollowing() const
{
	// 새 공격 경로에서는 명시적인 이동 제한 상태를 먼저 검사
	const AEnemyCharacter* OwnerEnemy = Cast<AEnemyCharacter>(CharacterOwner);
	if (OwnerEnemy && OwnerEnemy->IsAttackMovementLocked())
		return false;

	// 지상 경로를 이용하는 비행형에만 예외 적용: 루트모션 때문에
	if (!bUseGroundPathForFlight || !IsFlying())
		return Super::CanStartPathFollowing();

	if (!HasValidData() || !IsValid(CharacterOwner))
		return false;

	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CharacterOwner);
	if (!IsValid(Enemy) || !Enemy->IsAlive())
		return false;

	// 루트 컴포넌트에 물리 시뮬레이션 있는지 확인
	const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(CharacterOwner->GetRootComponent());
	if (RootPrimitive && RootPrimitive->IsSimulatingPhysics())
		return false;

	const USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	const UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;

	// 공격, 피격 등의 몽타주 중에는 새 경로 이동을 시작핮 ㅣ않음
	if (AnimInstance&&AnimInstance->IsAnyMontagePlaying())
		return false;

	// Idle, 이동 애니메이션의 루트 모션 중에도 길 찾기 시작 허용
	return true;
}

FVector UEnemyCharacterMovementComponent::GetActorFeetLocation() const
{
	FVector ActorFeetLocation = Super::GetActorFeetLocation();
	FVector NavFeetLocation = ActorFeetLocation;

	if (bUseGroundPathForFlight && IsFlying() && bNavigationGroundInitialized)
	{
		// 실제 공중 위치를 지상 경로 기준 위치로 변환
		NavFeetLocation.Z = NavigationGroundZ;
	}

#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GetWorld())
	{
		// 초록색: 실제 캡슐 발 위치
		DrawDebugSphere(
			World,
			ActorFeetLocation,
			8.f,	// 반지름
			12,		// 구체 분할 수
			FColor::Green,
			false,
			0.05f
		);
		// 파란색: 길 찾기용 발 위치
		DrawDebugSphere(
			World,
			NavFeetLocation,
			10.f,	// 반지름
			12,		// 구체 분할 수
			FColor::Blue,
			false,
			0.05f
		);
	}
#endif

	return NavFeetLocation;
}

void UEnemyCharacterMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	/* 밀림 및 이후 대기 시간 동안 일반 경로 이동 요청을 무시 */
	if (IsYieldingToPush())
	{
		RequestedVelocity = FVector::ZeroVector;
		bHasRequestedVelocity = false;
		return;
	}
	
	/* 비행형 Enemy 이동 보정 */

	FVector AdjustedVelocity = MoveVelocity;

	if (bUseGroundPathForFlight && IsFlying())
	{
		// 지상 경로를 따라 수평으로 비행
		AdjustedVelocity.Z = 0.f;
	}

	Super::RequestDirectMove(AdjustedVelocity, bForceMaxSpeed);
}

void UEnemyCharacterMovementComponent::RequestPathMove(const FVector & MoveInput)
{
	/* 밀려나기 중 이동 무시 */
	if (IsYieldingToPush())
	{
		return;
	}

	/* 비행형 Enemy 이동 보정 */

	FVector AdjustedInput = MoveInput;

	if (bUseGroundPathForFlight && IsFlying())
	{
		AdjustedInput.Z = 0.f;
	}

	Super::RequestPathMove(AdjustedInput);
}

void UEnemyCharacterMovementComponent::InitializeHoverHeight()
{
	bNavigationGroundInitialized = false;

	// 캐릭터가 지상형이거나 준비되지 않은 경우 HoverHeight 설정하지 않음
	if (!bUseGroundPathForFlight || !IsValid(CharacterOwner) || !GetWorld())
		return;

	const FVector FeetLocation = Super::GetActorFeetLocation();

	const FVector TraceEnd = FeetLocation - FVector(0.f, 0.f, 5000.f);

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(
		Hit,
		FeetLocation,
		TraceEnd,
		Objects,
		Params))
	{
		HoverHeight = FMath::Max(0.f, FeetLocation.Z - Hit.ImpactPoint.Z);

		// 비행형 캐릭터가 애니메이션으로 인해 위아래로 움직여도 이 값 유지
		NavigationGroundZ = Hit.ImpactPoint.Z;
		bNavigationGroundInitialized = true;
	}

}

FVector UEnemyCharacterMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	FVector Result = Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);

	// 이동 억제 중에는 루트 모션의 수평 속도를 밀림 속도로 대체
	// 수직 속도는 기존 계산 결과 유지
	if (IsYieldingToPush())
	{
		Result.X = ActivePushVelocity.X;
		Result.Y = ActivePushVelocity.Y;
		return Result;
	}

	/* 비행형 Enemy 루트 모션 관련 보정 */

	// 비행형일 때만 계속 진행: 루트 모션 때문에
	if (!bUseGroundPathForFlight || !IsFlying())
		return Result;

	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CharacterOwner);
	if (!IsValid(Enemy) || !Enemy->IsAlive())
		return Result;

	const USkeletalMeshComponent* Mesh = Enemy->GetMesh();
	const UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;

	// 몽타주 재생이 아닐 때만 적용
	if (AnimInstance && AnimInstance->IsAnyMontagePlaying())
		return Result;

	const AAIController* AI = Cast<AAIController>(Enemy->GetController());
	const bool bFollowingPath = IsValid(AI) && AI->GetMoveStatus() == EPathFollowingStatus::Moving;

	// 수평 속도 설정
	FVector HorizontalVelocity = FVector::ZeroVector;
	if (bFollowingPath && bHasRequestedVelocity)
	{
		// 경로 요청값은 매우 클 수 있으므로 최대 비행 속도로 제한
		HorizontalVelocity = RequestedVelocity;
		HorizontalVelocity.Z = 0.f;
		HorizontalVelocity = HorizontalVelocity.GetClampedToMaxSize(GetMaxSpeed());
	}

	Result.X = HorizontalVelocity.X;
	Result.Y = HorizontalVelocity.Y;

	// Result.Z는 기존 루트 모션 값 유지
	return Result;
}

bool UEnemyCharacterMovementComponent::ShouldRemainVertical() const
{
	if (bUseGroundPathForFlight && IsFlying())
	{
		return true;
	}

	return Super::ShouldRemainVertical();
}

// 위치 이동과 충돌 해결을 유지하면서 공격 중 회전만 제한
bool UEnemyCharacterMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation,
	bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	// 회전 제한은 위치 이동 및 충돌 해결을 유지하면서 회전값에만 적용
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CharacterOwner);
	const FQuat Rotation = Enemy && Enemy->IsRotationLocked() ? Enemy->GetActorQuat() : NewRotation;
	return Super::MoveUpdatedComponentImpl(Delta, Rotation, bSweep, OutHit, Teleport);
}

void UEnemyCharacterMovementComponent::RequestPlayerPush(const FVector& PushVelocity, bool bDirectPush)
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CharacterOwner);
	// 소유자가 없거나 밀림을 허용하지 않는 상태면 요청 거부
	if (!Enemy || !Enemy->CanReceivePlayerPush())
		return;

	// 지상 또는 비행 이동 Enemy에만 밀려나기 적용
	if (!IsMovingOnGround() && !IsFlying())
		return;

	// 수평 방향만 사용
	FVector HorizontalVelocity = PushVelocity;
	HorizontalVelocity.Z = 0.f;

	// 벡터의 방향을 유지하면서 길이가 최대를 넘지 않도록 제한
	HorizontalVelocity = HorizontalVelocity.GetClampedToMaxSize(MaxPushSpeed);
	if (HorizontalVelocity.IsNearlyZero())
		return;

	// 요청 종류에 해당하는 대기 슬롯 참조
	FVector& Pending = bDirectPush ? PendingDirectPush : PendingYieldPush;

	// 여러 요청을 합산해서 과도하게 빨라지는 것을 방지
	// 같은 종류의 요청은 합산하지 않고, 속력이 가장 큰 요청 하나만 저장
	if (HorizontalVelocity.SizeSquared() > Pending.SizeSquared())
	{
		Pending = HorizontalVelocity;
	}

}

bool UEnemyCharacterMovementComponent::IsYieldingToPush() const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CharacterOwner);

	return Enemy && Enemy->CanReceivePlayerPush() && PushHoldRemaining > 0.f;
}

void UEnemyCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	ProcessRootMotionPreConvertToWorld.BindUObject(
		this,
		&UEnemyCharacterMovementComponent::FilterFlightRootMotion
	);
}

void UEnemyCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CharacterOwner);

	const bool bCanPush =
		HasValidData()	// 이동에 필요한 소유 캐릭터와 이동 대상 컴포넌트가 유효한가?
		&& Enemy && Enemy->CanReceivePlayerPush()	// 플레이어의 직접 밀기를 적용 가능한 상태인가?
		&& (IsMovingOnGround() || IsFlying());

	// 밀려날 수 없는 상태이면 밀림 상태 모두 지우고 기존 이동 처리 실행
	if (!bCanPush)
	{
		// 밀리는 중 공격을 시작해도 남은 요청 즉시 폐기
		ClearPlayerPush();

		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		// 공격 중처럼 밀릴 수 없는 상태에서도 높이 유지는 필요
		UpdateFlightHeight(DeltaTime);

		return;
	}

	// 대기 중인 밀려나기 요청 꺼내기
	const FVector DirectPush = PendingDirectPush;
	const FVector YieldPush = PendingYieldPush;

	// 밀려나기 요청은 지역 변수에 보관했으므로 대기 슬롯을 비워 중복 적용 방지
	PendingDirectPush = FVector::ZeroVector;
	PendingYieldPush = FVector::ZeroVector;

	// 이번에 사용할 속도 선택: Player 직접 밀기 vs Enemy 양보 요청
	// 우선순위: Player 직접 밀기 > Enemy 양보 요청
	ActivePushVelocity = !DirectPush.IsNearlyZero() ? DirectPush : YieldPush;

	// 밀려나기 요청 접근 억제 시간 갱신
	if (!ActivePushVelocity.IsNearlyZero())
	{
		// 밀려나기 요청 들어올 때마다 정해진 억제 시간으로 유지
		PushHoldRemaining = PushHoldDuration;
	}
	else
	{
		// 밀려나기 요청 끊기면 맥 갱신마다 시간 줄이기
		// 밀림은 멈추지만, 몬스터가 곧바로 플레이어에게 재접근하지 않도록 잠깐 기다리도록 구현
		PushHoldRemaining = FMath::Max(0.f, PushHoldRemaining - DeltaTime);
	}

	// 밀려나는 동안 AI 이동이 밀려나기를 방해하지 않도록 이전 이동 정보 정리
	if (IsYieldingToPush())
	{
		// 이전 AI 이동의 수평 속도와 입력 정리
		Velocity.X = 0.f;
		Velocity.Y = 0.f;

		Acceleration = FVector::ZeroVector;
		RequestedVelocity = FVector::ZeroVector;
		bHasRequestedVelocity = false;

		CharacterOwner->ConsumeMovementInputVector();

		// 직접 밀리는 Enemy만 주변에 양보 요청
		// 양보받은 Enemy는 다시 전파하지 않도록 제한
		if (!DirectPush.IsNearlyZero())
		{
			RequestNearbyYield(DirectPush);
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 일반 이동과 충돌 처리 이후 비행 높이 보정
	UpdateFlightHeight(DeltaTime);
}

void UEnemyCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// 일반 가속, 제동 계산 대신 이번 밀림 속도를 수평 속도로 사용
	// 요청이 끊긴 뒤의 대기 시간에는 ActivePushVelocity가 0이므로 정지
	if (IsYieldingToPush())
	{
		Velocity.X = ActivePushVelocity.X;
		Velocity.Y = ActivePushVelocity.Y;
		return;
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

FTransform UEnemyCharacterMovementComponent::FilterFlightRootMotion(const FTransform& LocalRootMotion, 
	UCharacterMovementComponent* MovementComponent, float DeltaSeconds)
{
	FTransform Result = LocalRootMotion;

	if (false == bUseGroundPathForFlight || false == IsFlying())
		return Result;

	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CharacterOwner);
	if (false == IsValid(Enemy) || false == Enemy->IsAlive())
		return Result;

	// 비행형은 몽타주를 포함해 루트 모션의 추가 회전을 제거
	Result.SetRotation(FQuat::Identity);

	return Result;
}

void UEnemyCharacterMovementComponent::UpdateFlightHeight(float DeltaTime)
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CharacterOwner);

	// 살아 있는 비행형만 보정 (사망 후에는 랙돌에 맡김)
	if (!HasValidData() || !GetWorld() || !Enemy || !Enemy->IsAlive()
		|| !bUseGroundPathForFlight || !IsFlying()
		|| !bNavigationGroundInitialized
		|| DeltaTime <= 0.f)
	{
		return;
	}

	// 캡슐 컴포넌트
	const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();

	// 컴포넌트 위치
	const FVector Center = UpdatedComponent->GetComponentLocation();

	const FVector TraceEnd = Center - FVector(0.f, 0.f, FlightGroundTraceDistance);

	// 플레이어와 몬스터 대신 정적 지형만 바닥 후보로 탐색
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Enemy);	// 자기 자신 무시

	FHitResult GroundHit;
	if (!GetWorld()->LineTraceSingleByObjectType(
		GroundHit,
		Center,
		TraceEnd,
		Objects,
		Params))
	{
		// 바닥 찾지 못하면 이번 보정 생략
		return;
	}

	// 지형 내부에서 시작한 탐색 결과는 사용하지 않음
	if (GroundHit.bStartPenetrating)
		return;

	// 현재 지형 높이를 길 찾기 기준에도 반영
	NavigationGroundZ = GroundHit.ImpactPoint.Z;

	// 현재 바닥 높이에 초기 비행 높이와 캡슐 절반 높이를 더한 목표 캡슐 중심 Z 좌표 (cm)
	// HoverHeight는 캡슐 바닥과 지면 사이의 거리
	const float TargetCenterZ = NavigationGroundZ + HoverHeight + Capsule->GetScaledCapsuleHalfHeight();
	
	const float HeightError = TargetCenterZ - Center.Z;

	// 목표 높이와 거의 같으면 이동하지 않음
	if (FMath::Abs(HeightError) <= FlightHeightTolerance)
		return;

	// 프레임 시간에 비례해 최대 보정량 제한
	const float MaxCorrection = FlightHeightCorrectionSpeed * DeltaTime;
	const float CorrectionZ = FMath::Clamp(HeightError, -MaxCorrection, MaxCorrection);

	// 바닥이나 천장을 관통하지 않도록 충돌 검사하며 보정
	FHitResult MoveHit;
	SafeMoveUpdatedComponent(
		FVector(0.f, 0.f, CorrectionZ),
		UpdatedComponent->GetComponentQuat(),
		true,
		MoveHit
	);
}

void UEnemyCharacterMovementComponent::RequestNearbyYield(const FVector& PushVelocity)
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CharacterOwner);
	if (!Enemy)
		return;

	const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
	
	const FVector Forward = PushVelocity.GetSafeNormal2D();
	const FVector Right = FVector(-Forward.Y, Forward.X, 0.f);	// 밀림 진행 방향 기준 수평 오른쪽 단위 벡터

	// 실제 캡슐보다 여유 폭만큼 넓은 탐색 반경
	const float Radius = Capsule->GetScaledCapsuleRadius() + YieldContactMargin;
	// 탐색 캡슐의 절반 높이가 반경보다 작아지지 않도록 보정
	const float HalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight(), Radius);
	
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);	// Object Type이 Pawn인 컴포넌트만 탐색

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Enemy);				// 이번 겹침 탐색 결과에서 자기 자신 제외

	TArray<FOverlapResult> Results;
	// 지정한 캡슐 모양의 영역과 겹친느 컴포넌트 조회
	GetWorld()->OverlapMultiByObjectType(
		Results,
		Enemy->GetActorLocation(),
		FQuat::Identity,
		Objects,
		FCollisionShape::MakeCapsule(Radius, HalfHeight),
		Params
	);

	TSet<AEnemyCharacter*> Processed;	// 이번 탐색에서 이미 확인한 Enemy를 기록해 중복 요청 방지
	for (const FOverlapResult& Result : Results)
	{
		AEnemyCharacter* Other = Cast<AEnemyCharacter>(Result.GetActor());

		// 유효한 다른 Enemy가 아니거나 이미 확인한 대상이면 건너뜀
		if (!IsValid(Other) || Other == Enemy || Processed.Contains(Other))
			continue;
		
		// 충돌체가 유효하면 처리 목록에 추가
		Processed.Add(Other);

		// 공격 중인 Enemy 등은 양보하지 않음
		if (!Other->CanReceivePlayerPush())
			continue;

		// 충돌한 Enemy와 자신의 위치 차이와 방향 알아내기
		const FVector Offset = Other->GetActorLocation() - Enemy->GetActorLocation();
		const FVector ToOther = Offset.GetSafeNormal2D();

		// 밀려갈 방향 앞을 막는 Enemy만 대상
		const float Ahead = FVector::DotProduct(Forward, ToOther);
		if (Ahead <= 0.1f)
			continue;

		// 다른 Enemy가 밀림 방향 기준 좌우 어느 쪽에 있는지 계산
		// Offset을 정규화하지 않았으므로 값은 좌우 방향 거리(cm)
		// 양수는 오른쪽, 음수는 왼쪽
		const float Side = FVector::DotProduct(Right, Offset);

		float SideSign;	// 좌유 이동 부호: 왼쪽 -1, 오른쪽 +1

		// 정면에서 좌우로 1cm 넘게 떨어져 있으면 현재 위치한 쪽으로 양보
		if (FMath::Abs(Side) > 1.f)
		{
			SideSign = Side > 0.f ? 1.f : -1.f;
		}
		else
		{
			// 정면에 일렬로 있을 때도 방향을 일정하게 유지
			SideSign = (Other->GetUniqueID() > Enemy->GetUniqueID())
				? 1.f : -1.f;
		}

		// 직접 밀기보다 빠르게 양보하지 않도록 제한
		const float Speed = FMath::Min(YieldSpeed, PushVelocity.Size2D());

		Other->ApplyPlayerPush(Right * SideSign * Speed, false);
	}
}

void UEnemyCharacterMovementComponent::ClearPlayerPush()
{
	PendingDirectPush = FVector::ZeroVector;
	PendingYieldPush = FVector::ZeroVector;
	ActivePushVelocity = FVector::ZeroVector;
	PushHoldRemaining = 0.f;
}
