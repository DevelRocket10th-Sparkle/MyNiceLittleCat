// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.19.
* 비행형 몬스터의 루트 모션 관련 회전 문제 처리를 위해
* 캐릭터 무브먼트 컴포넌트를 상속하여 일부 함수 오버라이드 구현
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnemyCharacterMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UEnemyCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public :

	virtual bool CanStartPathFollowing() const override;

	virtual FVector GetActorFeetLocation() const override;
	
	virtual void RequestDirectMove(
		const FVector& MoveVelocity,
		bool bForceMaxSpeed
	) override;

	virtual void RequestPathMove(const FVector& MoveInput) override;

	void InitializeHoverHeight();

	virtual FVector ConstrainAnimRootMotionVelocity(
		const FVector& RootMotionVelocity,
		const FVector& CurrentVelocity
	) const override;

	virtual bool ShouldRemainVertical() const override;

	// 공격 회전 제한을 실제 이동 처리에 적용하여 루트 모션 회전도 차단
	// 위치 이동과 충돌 해결을 유지하면서 공격 중 회전만 제한
	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation,
		bool bSweep, FHitResult* OutHit = nullptr, ETeleportType Teleport = ETeleportType::None) override;

	/* 밀려나기 */
	
	// Player의 밀치기 적용 요청
	void RequestPlayerPush(const FVector& PushVelocity, bool bDirectPush);

	// 직접 밀림 또는 양보로 일반 수평 이동을 억제 중인지
	// 밀림 요청이 끊긴 뒤에 짧은 대기 시간도 포함
	bool IsYieldingToPush() const;

protected:

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	// 실제 발 위치와 내비게이션 기준 위치 표시
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawNavigationFeet = false;

private :

	FTransform FilterFlightRootMotion(const FTransform& LocalRootMotion,
		UCharacterMovementComponent* MovementComponent, float DeltaSeconds);

	// 높이 보정
	void UpdateFlightHeight(float DeltaTime);

	/* 밀려나기 */

	// 주변 Enemy에 양보 요청
	void RequestNearbyYield(const FVector& PushVelocity);
	
	// Player 밀기 요청 해제
	void ClearPlayerPush();

protected :

	// 평평한 지면 위 일정 높이 비행에 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path Find")
	bool bUseGroundPathForFlight = false;

	// 현재 캡슐 중심에서 아래쪽으로 바닥을 탐색할 거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Flight|Height")
	float FlightGroundTraceDistance = 1500.f;

	// 목표 높이로 복귀하는 최대 속력 (cm/s)
	UPROPERTY(EditDefaultsOnly, Category = "Flight|Height")
	float FlightHeightCorrectionSpeed = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight|Height", meta = (ClampMin = "0.0"))
	float FlightHeightTolerance = 0.5f;

	/* 밀려나기 */
	
	// 마지막 밀림 요청 이후 일반 이동 억제를 유지할 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Push", meta = (ClampMin = "0.0"))
	float PushHoldDuration = 0.15f;
	
	// 요청받은 밀림 속력의 상한 (cm/s)
	UPROPERTY(EditDefaultsOnly, Category = "Push", meta = (ClampMin = "0.0"))
	float MaxPushSpeed = 150.f;
	
	// 주변 Enemy에 요청할 옆 방향 양보 속력 (cm/s)
	UPROPERTY(EditDefaultsOnly, Category = "Push", meta = (ClampMin = "0.0"))
	float YieldSpeed = 80.f;
	
	// 양보 대상 탐색 시 실제 캡슐 반경에 더할 여유 폭 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Push", meta = (ClampMin = "0.0"))
	float YieldContactMargin = 15.f;

private :

	// 캡슐 바닥과 지면 사이의 높이
	UPROPERTY(VisibleInstanceOnly, Category = "Path Find")
	float HoverHeight = 0.f;

	// 길 찾기 기준으로 사용할 바닥 높이
	float NavigationGroundZ = 0.f;

	// 길 찾기용 바닥 높이 초기화 완료 여부
	bool bNavigationGroundInitialized = false;

	/* 밀려나기 */

	// 아직 이동 갱신에 반영하지 않은 플레이어의 직접 밀기 요청
	FVector PendingDirectPush = FVector::ZeroVector;

	// 아직 반영하지 않은 주변 몬스터의 양보 요청
	FVector PendingYieldPush = FVector::ZeroVector;

	// 이번 이동 갱신에서 실제로 사용할 밀림 속도
	FVector ActivePushVelocity = FVector::ZeroVector;
	
	// 일반 이동 억제의 남은 시간 (초)
	// 실제 밀림 지속 시간과는 다름
	float PushHoldRemaining = 0.f;

};
