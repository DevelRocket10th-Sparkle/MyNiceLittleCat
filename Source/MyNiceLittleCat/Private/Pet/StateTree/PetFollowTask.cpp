#include "Pet/StateTree/PetFollowTask.h"
#include "Pet/PetCharacter.h"
#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

EStateTreeRunStatus FPetFollowTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    // 현재 실행중인 Task의 데이터를 꺼냄 (InstanceData)
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    // 데이터 초기화
    Data.Controller.Reset();
    Data.MoveTarget.Reset();
    Data.TimeUntilNextCheck = 0.f;
    Data.bHasSavedMoveSpeed = false;
    Data.MoveRetryCount = 0;
    Data.RetryTimeRemaining = 0.f;
    Data.bWaitingToRetry = false;
    Data.bMoveRequestFailed = false;

    // 펫이 없다 : Task 실패
    if (false == IsValid(Data.Pet.Get()))
        return EStateTreeRunStatus::Failed;
    
    // 펫의 AI 컨트롤러가 없다 : Task 실패
    AAIController* Controller = Cast<AAIController>(Data.Pet->GetController());
    if (false == IsValid(Controller))
        return EStateTreeRunStatus::Failed;

    // 펫의 타겟이 없다 : Task 실패
    AActor* Target = Data.Pet->GetPartnerPlayer();
    if (false == IsValid(Target))
        return EStateTreeRunStatus::Failed;

    Data.ProgressAnchor = Data.Pet->GetActorLocation();
    Data.NoProgressTime = 0.f;

    Data.Controller = Controller;

    UCharacterMovementComponent* Movement = Data.Pet->GetCharacterMovement();
    if (false == IsValid(Movement))
        return EStateTreeRunStatus::Failed;

    // 추종 전의 속도를 기억해요
    Data.OriginalMaxWalkSpeed = Movement->MaxWalkSpeed;
    Data.bHasSavedMoveSpeed = true;

    // Controller와 속도 정보는 준비하고, 이동 요청만 보류
    if (true == Data.Pet->IsNavigationRecoveryPending()) return EStateTreeRunStatus::Running;

    // Target에게로 이동 요청 실패 : Task 실패
    // MoveToActor(따라갈 대상, 도착 허용 거리, 펫 반경을 허용 거리에 추가할지)
    const EPathFollowingRequestResult::Type Result = Controller->MoveToActor(Target, Data.FollowDistance, false);
   
    Data.MoveTarget = Target;
    Data.bMoveRequestFailed = Result == EPathFollowingRequestResult::Failed;

    if (true == Data.bMoveRequestFailed)
        Data.Pet->SetAnimationMovementStyle(EPetMovementStyle::Normal);

    // 이동 요청 실패여도 Task를 유지해서 Tick에서 재시도
    return EStateTreeRunStatus::Running;
}

void FPetFollowTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (AAIController* Controller = Data.Controller.Get())
        Controller->StopMovement();

    if (true == Data.bHasSavedMoveSpeed && true == IsValid(Data.Pet.Get()))
    {
        if(UCharacterMovementComponent* Movement = Data.Pet->GetCharacterMovement())
            Movement->MaxWalkSpeed = Data.OriginalMaxWalkSpeed;
    }


    Data.bHasSavedMoveSpeed = false;
    Data.Controller.Reset();
    Data.MoveTarget.Reset();
    Data.TimeUntilNextCheck = 0.f;
}

EStateTreeRunStatus FPetFollowTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    AAIController* Controller = Data.Controller.Get();
    if (false == IsValid(Data.Pet.Get()) || false == IsValid(Controller))
        return EStateTreeRunStatus::Failed;

    if (true == Data.Pet->IsNavigationRecoveryPending())
        return EStateTreeRunStatus::Running;

    // StateTree가 다음 행동으로 전환하기 전까지 이전 추종 Task가 이동을 다시 요청하지 않도록 함
    if (Data.Pet->GetRequestedBehavior() != EPetBehavior::Follow)
        return EStateTreeRunStatus::Running;

    AActor* Target = Data.Pet->GetPartnerPlayer();
    if (false == IsValid(Target))
    {
        Controller->StopMovement();
        return EStateTreeRunStatus::Failed;
    }

    UCharacterMovementComponent* Movement = Data.Pet->GetCharacterMovement();
    if (false == IsValid(Movement))
        return EStateTreeRunStatus::Failed;

    Data.RetryTimeRemaining = FMath::Max(0.f, Data.RetryTimeRemaining - DeltaTime);

    // 설치 후 복귀가 실제 도착으로 끝났다면 일반 이동으로 복원
    if (Data.Pet->GetRequestedBehavior() == EPetBehavior::Follow && 
        Data.Pet->GetAnmationMovementStyle() == EPetMovementStyle::ReflectPlacement)
    {
        const UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent();
        if (PathFollowing && PathFollowing->DidMoveReachGoal())
            Data.Pet->SetAnimationMovementStyle(EPetMovementStyle::Normal);
    }

    //! 뒤쳐진 거리를 가속/감속으로 따라잡기
    // 높이 차이를 제외한 거리, 플레이어 속도
    const float Distance = FVector::Dist2D(Data.Pet->GetActorLocation(), Target->GetActorLocation());
    const float PlayerSpeed = Target->GetVelocity().Size2D();

    // 추종을 유지하려는 거리보다 펫이 얼마나 뒤쳐진건지 계산
    const float ExcessDistance = FMath::Max(0.f, Distance - Data.FollowDistance);
    const float DesiredSpeed = PlayerSpeed * Data.PlayerSpeedMultiplier + ExcessDistance * Data.CatchUpSpeedPerCm;

    // 기본 이동 속도보다는 낮아지지 않도록 해요
    const float SpeedLimit = FMath::Max(Data.OriginalMaxWalkSpeed, Data.MaxFollowSpeed);
    Movement->MaxWalkSpeed = FMath::Clamp(DesiredSpeed, Data.OriginalMaxWalkSpeed, SpeedLimit);

    UPathFollowingComponent* RecoveryPathFollowing = Controller->GetPathFollowingComponent();

    if (false == IsValid(RecoveryPathFollowing)) return EStateTreeRunStatus::Failed;

    const FVector CurrentLocation = Data.Pet->GetActorLocation();
    const EPathFollowingStatus::Type MoveStatus = Controller->GetMoveStatus();

    const bool bNormallyArrived = MoveStatus == EPathFollowingStatus::Idle && false == Data.bMoveRequestFailed && RecoveryPathFollowing->DidMoveReachGoal();
    const bool bShouldWatchMovement = MoveStatus == EPathFollowingStatus::Moving || (MoveStatus == EPathFollowingStatus::Idle && false == bNormallyArrived);

    if (false == bShouldWatchMovement)
    {
        // 정상 도착, 일시 정지, 경로 대기 중에는 누적하지 않음
        Data.ProgressAnchor = CurrentLocation;
        Data.NoProgressTime = 0.f;
    }
    else if (FVector::DistSquared2D(CurrentLocation, Data.ProgressAnchor) >= FMath::Square(Data.ProgressDistance))
    {
        // 충분히 움직였다면 막힘 기록과 재시도 횟수 초기화
        Data.ProgressAnchor = CurrentLocation;
        Data.NoProgressTime = 0.f;
        Data.MoveRetryCount = 0;
    }
    else
    {
        Data.NoProgressTime += DeltaTime;

        if (Data.NoProgressTime >= Data.StuckDetectionTime)
        {
            if (Data.MoveRetryCount >= Data.MaxMoveRetries)
            {
                FVector SafeLandingLocation;

                // 착지할 곳이 있을 때만 비행 복귀 요청
                if (Data.Pet->TryFindSafeRecoveryLocation(CurrentLocation, SafeLandingLocation))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[PetFollow] Stuck: requesting flight recovery."));
                    Data.Pet->RequestBehavior(EPetBehavior::RecoverFollow);
                    return EStateTreeRunStatus::Running;
                }

                // 안전한 곳이 없으면 잠시 후 다시 판단
                Data.NoProgressTime = 0.f;
            }
            else if (MoveStatus == EPathFollowingStatus::Moving)
            {
                // 엔진이 아직 Moving이라고 판단하더라도,
                // 실제로 움직이지 못하면 기존 재시도 흐름으로 보냄
                Controller->StopMovement();

                Data.bMoveRequestFailed = true;
                Data.NoProgressTime = 0.f;
            }
        }
    }

    //! 이동 재요청
    Data.TimeUntilNextCheck -= DeltaTime;

    if (0.f < Data.TimeUntilNextCheck)
        return EStateTreeRunStatus::Running;

    Data.TimeUntilNextCheck = 0.2f; //! 이동명령 주기 임시값

    UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent();

    if (false == IsValid(PathFollowing))
        return EStateTreeRunStatus::Failed;

    const bool bTargetChanged = Data.MoveTarget.Get() != Target;
    bool bIsRetry = false;

    if (true == bTargetChanged)
    {
        Data.MoveRetryCount = 0;
        Data.bWaitingToRetry = false;
        Data.bMoveRequestFailed = false;
    }
    else
    {
        // 이동 중이거나 경로를 기다리는 동안은 현재 이동 유지
        if (Controller->GetMoveStatus() != EPathFollowingStatus::Idle)
            return EStateTreeRunStatus::Running;

        const bool bStoppedWithoutArrival = Data.bMoveRequestFailed || false == PathFollowing->DidMoveReachGoal();
        if (true == bStoppedWithoutArrival)
        {
            // 실패를 감지한 뒤 NavMesh 갱신을 기다림
            if (false == Data.bWaitingToRetry)
            {
                Data.bWaitingToRetry = true;
                Data.RetryTimeRemaining = Data.MoveRetryInterval;
                return EStateTreeRunStatus::Running;
            }

            if (Data.RetryTimeRemaining > 0.f) return EStateTreeRunStatus::Running;
            if (Data.MoveRetryCount >= Data.MaxMoveRetries) return EStateTreeRunStatus::Running;

            ++Data.MoveRetryCount;
            Data.bWaitingToRetry = false;
            bIsRetry = true;
        }
        else
        {
            // 정상 도착한 경우에만 기존 재출발 거리 사용
            Data.MoveRetryCount = 0;
            Data.bWaitingToRetry = false;

            const float RestartDistance = FMath::Max(Data.ResumeDistance, Data.FollowDistance);
            const float DistanceSquared = FVector::DistSquared2D(Data.Pet->GetActorLocation(), Target->GetActorLocation());
            if (DistanceSquared <= FMath::Square(RestartDistance)) return EStateTreeRunStatus::Running;
        }
    }

    const EPathFollowingRequestResult::Type Result = Controller->MoveToActor(Target, Data.FollowDistance, false);

    Data.MoveTarget = Target;
    Data.bMoveRequestFailed = Result == EPathFollowingRequestResult::Failed;

    if (true == bIsRetry)
    {
        UE_LOG(LogTemp, Log, TEXT("[PetFollow] Retry %d/%d, RequestResult=%d"),
            Data.MoveRetryCount, Data.MaxMoveRetries, static_cast<int32>(Result));
    }

    if (true == Data.bMoveRequestFailed)
        Data.Pet->SetAnimationMovementStyle(EPetMovementStyle::Normal);
    
    else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        Data.MoveRetryCount = 0;
        Data.bWaitingToRetry = false;
    }

    return EStateTreeRunStatus::Running;
}
