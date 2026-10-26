/// 작성자: 최요환
/// 작성일자: 2026-10-26
///
/// 보스 패턴 GA가 공격 전에 정해진 지점(격자 바깥 대기 지점 등)으로 보스를 옮길 때 쓰는 AbilityTask입니다.
/// AI MoveTo로 걸어가고, 경로가 없거나 TimeLimit 안에 도착하지 못하면 그 지점으로 순간이동합니다.
/// 도착하면 지점 방향(Yaw)으로 돌리고 OnArrived를 알립니다. GA가 끝나면(취소 포함) 이동을 멈춥니다.
/// 보스의 공격 이동 잠금(AcquireAttackMovementLock)이 걸려 있으면 경로 이동이 막히므로, 잠금은 도착한 뒤에 겁니다.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "AbilityTask_BossMoveToPoint.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossMoveToPointDelegate);

UCLASS()
class MYNICELITTLECAT_API UAbilityTask_BossMoveToPoint : public UAbilityTask
{
	GENERATED_BODY()

public:
	// Location은 바닥 지점, Yaw는 도착 후 바라볼 방향
	static UAbilityTask_BossMoveToPoint* BossMoveToPoint(UGameplayAbility* OwningAbility, const FVector& Location, float Yaw,
		float AcceptanceRadius, float TimeLimit);

	virtual void Activate() override;

	// 걸어서 도착했거나 순간이동으로 옮겨졌을 때
	UPROPERTY(BlueprintAssignable)
	FBossMoveToPointDelegate OnArrived;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

	UFUNCTION()
	void HandleMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	// bTeleport면 지점으로 순간이동 후 완료
	void Finish(bool bTeleport);

	void UnbindMove();

	FVector TargetLocation = FVector::ZeroVector;

	float TargetYaw = 0.f;

	float AcceptanceRadius = 50.f;

	float TimeLimit = 3.f;

	FAIRequestID MoveRequestId;

	FTimerHandle TimeLimitHandle;

	bool bFinished = false;
};
