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

bool UEnemyCharacterMovementComponent::CanStartPathFollowing() const
{
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

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);

	FHitResult Hit;

	if (GetWorld()->LineTraceSingleByChannel(Hit, FeetLocation, TraceEnd, ECC_Visibility, Params))
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
