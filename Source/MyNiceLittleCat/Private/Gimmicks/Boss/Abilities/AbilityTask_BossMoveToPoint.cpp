/// 작성자: 최요환
/// 작성일자: 2026-10-26
///
/// 보스 지점 이동 AbilityTask 구현

#include "Gimmicks/Boss/Abilities/AbilityTask_BossMoveToPoint.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAbilityTask_BossMoveToPoint* UAbilityTask_BossMoveToPoint::BossMoveToPoint(UGameplayAbility* OwningAbility, const FVector& Location,
	float Yaw, float AcceptanceRadius, float TimeLimit)
{
	UAbilityTask_BossMoveToPoint* Task = NewAbilityTask<UAbilityTask_BossMoveToPoint>(OwningAbility);
	Task->TargetLocation = Location;
	Task->TargetYaw = Yaw;
	Task->AcceptanceRadius = FMath::Max(1.f, AcceptanceRadius);
	Task->TimeLimit = TimeLimit;
	return Task;
}

void UAbilityTask_BossMoveToPoint::Activate()
{
	Super::Activate();

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (!Character)
	{
		Finish(false);
		return;
	}

	// 이미 지점 근처면 이동 없이 완료
	if (FVector::DistSquared2D(Character->GetActorLocation(), TargetLocation) <= FMath::Square(AcceptanceRadius))
	{
		Finish(false);
		return;
	}

	AAIController* AI = Cast<AAIController>(Character->GetController());
	if (!AI)
	{
		Finish(true);
		return;
	}

	AI->ReceiveMoveCompleted.AddUniqueDynamic(this, &UAbilityTask_BossMoveToPoint::HandleMoveCompleted);

	const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(TargetLocation, AcceptanceRadius, false, true, true, false);
	if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		Finish(false);
		return;
	}
	if (Result == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogTemp, Log, TEXT("[BossMove] %s: 경로를 찾지 못해 순간이동합니다."), *Character->GetName());
		Finish(true);
		return;
	}

	MoveRequestId = AI->GetCurrentMoveRequestID();

	if (TimeLimit > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(TimeLimitHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogTemp, Log, TEXT("[BossMove] 제한 시간 안에 도착하지 못해 순간이동합니다."));
			Finish(true);
		}), TimeLimit, false);
	}
}

void UAbilityTask_BossMoveToPoint::HandleMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	if (bFinished || RequestID != MoveRequestId)
	{
		return;
	}

	// 막혀서 멈춘 경우도 지점으로 맞춰 줌
	Finish(Result != EPathFollowingResult::Success);
}

void UAbilityTask_BossMoveToPoint::Finish(bool bTeleport)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	UnbindMove();

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
	{
		if (AAIController* AI = Cast<AAIController>(Character->GetController()))
		{
			AI->StopMovement();
		}

		if (bTeleport)
		{
			// 지점은 바닥이므로 캡슐 절반 높이만큼 올림
			const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			Character->SetActorLocation(TargetLocation + FVector(0.f, 0.f, HalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
		}

		Character->SetActorRotation(FRotator(0.f, TargetYaw, 0.f));
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnArrived.Broadcast();
	}

	EndTask();
}

void UAbilityTask_BossMoveToPoint::UnbindMove()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeLimitHandle);
	}

	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
	{
		if (AAIController* AI = Cast<AAIController>(Character->GetController()))
		{
			AI->ReceiveMoveCompleted.RemoveDynamic(this, &UAbilityTask_BossMoveToPoint::HandleMoveCompleted);
		}
	}
}

void UAbilityTask_BossMoveToPoint::OnDestroy(bool bInOwnerFinished)
{
	// GA가 먼저 끝나면(취소) 걷던 것을 멈춤
	if (!bFinished)
	{
		bFinished = true;
		UnbindMove();

		if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
		{
			if (AAIController* AI = Cast<AAIController>(Character->GetController()))
			{
				AI->StopMovement();
			}
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}
