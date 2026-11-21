// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.19.
* 수정 일자: 2026.11.17. (기능별 섹션 구분, 함수 주석 정리)
* 수정 일자: 2026.11.20. (바닥 탐색 도우미)
* Enemy 전용 캐릭터 무브먼트 컴포넌트
* - 비행 높이: 지상 내비 경로를 따라 일정 높이로 비행 (bUseGroundPathForFlight)
* - 루트 모션: 비행형 루트 모션의 추가 회전 제거와 공격 중 회전 제한
* - 밀려나기: 플레이어의 직접 밀기와 주변 Enemy의 양보
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnemyCharacterMovementComponent.generated.h"

UCLASS()
class MYNICELITTLECAT_API UEnemyCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public :

	/* 경로 이동, 비행 높이 */

	// 경로 이동 시작 가능 여부: 공격 이동 제한 중이면 false, 지상 경로 비행형은 공중에서도 허용
	virtual bool CanStartPathFollowing() const override;

	// 내비 기준 발 위치: 지상 경로 비행형은 실제 공중 위치 대신 바닥 높이를 사용
	virtual FVector GetActorFeetLocation() const override;

	// 경로 이동 속도 요청: 밀려나는 중에는 무시, 지상 경로 비행형은 수직 성분 제거
	virtual void RequestDirectMove(
		const FVector& MoveVelocity,
		bool bForceMaxSpeed
	) override;

	// 경로 이동 입력 요청: RequestDirectMove와 같은 규칙
	virtual void RequestPathMove(const FVector& MoveInput) override;

	// 배치 위치 기준으로 비행 높이(HoverHeight)와 내비 바닥 높이 기록: 지상 경로 비행형만
	void InitializeHoverHeight();

	/* 루트 모션, 회전 */

	// 루트 모션 속도 제한: 밀려나는 중에는 수평 속도를 밀림 속도로 대체, 비행형은 몽타주 밖에서 수평 속도를 경로 이동으로 보정
	virtual FVector ConstrainAnimRootMotionVelocity(
		const FVector& RootMotionVelocity,
		const FVector& CurrentVelocity
	) const override;

	// 지상 경로 비행형은 비행 중에도 캡슐을 세운 채 유지
	virtual bool ShouldRemainVertical() const override;

	// 공격 회전 제한을 실제 이동 처리에 적용하여 루트 모션 회전도 차단
	// - 위치 이동과 충돌 해결은 유지하고 회전만 제한
	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation,
		bool bSweep, FHitResult* OutHit = nullptr, ETeleportType Teleport = ETeleportType::None) override;

	/* 밀려나기 */
	
	// Player의 밀치기 적용 요청
	void RequestPlayerPush(const FVector& PushVelocity, bool bDirectPush);

	// 직접 밀림 또는 양보로 일반 수평 이동을 억제 중인지
	// - 밀림 요청이 끊긴 뒤의 짧은 대기 시간도 포함
	bool IsYieldingToPush() const;

protected :

	// 비행 루트 모션 필터 연결
	virtual void BeginPlay() override;

	// 쌓인 밀림 요청을 이번 밀림 속도로 정리한 뒤 이동 처리, 이후 비행 높이 보정
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 밀려나는 중에는 일반 가속·제동 대신 밀림 속도를 수평 속도로 사용
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	// 실제 발 위치와 내비게이션 기준 위치 표시
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawNavigationFeet = false;

private :

	/* 비행 */

	// 비행형 루트 모션의 추가 회전을 월드 변환 전에 제거 (BeginPlay에서 ProcessRootMotionPreConvertToWorld에 연결)
	FTransform FilterFlightRootMotion(const FTransform& LocalRootMotion,
		UCharacterMovementComponent* MovementComponent, float DeltaSeconds);

	// 바닥 기준 목표 높이(HoverHeight)로 천천히 복귀
	void UpdateFlightHeight(float DeltaTime);

	// 시작 위치에서 아래로 Distance만큼 정적 지형만 탐색 (자기 자신 무시): 비행 높이 기록과 보정에서 함께 사용
	bool TraceStaticGround(const FVector& Start, float Distance, FHitResult& OutHit) const;

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

	// 아직 반영하지 않은 주변 Enemy의 양보 요청
	FVector PendingYieldPush = FVector::ZeroVector;

	// 이번 이동 갱신에서 실제로 사용할 밀림 속도
	FVector ActivePushVelocity = FVector::ZeroVector;
	
	// 일반 이동 억제의 남은 시간 (초)
	// 실제 밀림 지속 시간과는 다름
	float PushHoldRemaining = 0.f;

};
