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

    Data.Controller = Controller;

    UCharacterMovementComponent* Movement = Data.Pet->GetCharacterMovement();
    if (false == IsValid(Movement))
        return EStateTreeRunStatus::Failed;

    // 추종 전의 속도를 기억해요
    Data.OriginalMaxWalkSpeed = Movement->MaxWalkSpeed;
    Data.bHasSavedMoveSpeed = true;

    // Target에게로 이동 요청 실패 : Task 실패
    // MoveToActor(따라갈 대상, 도착 허용 거리, 펫 반경을 허용 거리에 추가할지)
    const EPathFollowingRequestResult::Type Result =
        Controller->MoveToActor(Target, Data.FollowDistance, false);

    if (EPathFollowingRequestResult::Failed == Result)
        return EStateTreeRunStatus::Failed;

    // Task 성공 ~
    Data.MoveTarget = Target;
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

    AActor* Target = Data.Pet->GetPartnerPlayer();
    if (false == IsValid(Target))
    {
        Controller->StopMovement();
        return EStateTreeRunStatus::Failed;
    }

    UCharacterMovementComponent* Movement = Data.Pet->GetCharacterMovement();
    if (false == IsValid(Movement))
        return EStateTreeRunStatus::Failed;

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

    //! 이동 재요청
    Data.TimeUntilNextCheck -= DeltaTime;

    if (0.f < Data.TimeUntilNextCheck)
        return EStateTreeRunStatus::Running;

    Data.TimeUntilNextCheck = 0.2f; //! 이동명령 주기 임시값

    const bool bTargetChanged = Data.MoveTarget.Get() != Target;

    if (false == bTargetChanged)
    {
        // 기존 이동 요청이 진행 중이면 다시 요청하지 않아요
        if (Controller->GetMoveStatus() != EPathFollowingStatus::Idle)
            return EStateTreeRunStatus::Running;
  
        const float RestartDistance = FMath::Max(Data.ResumeDistance, Data.FollowDistance);
        const float DistanceSquared = FVector::DistSquared2D(Data.Pet->GetActorLocation(), Target->GetActorLocation());

        if (DistanceSquared <= FMath::Square(RestartDistance))
            return EStateTreeRunStatus::Running;
    }

    // 이동 요청 실패? : Task 실패
    const EPathFollowingRequestResult::Type Result =
        Controller->MoveToActor(Target, Data.FollowDistance, false);
    
    if (Result == EPathFollowingRequestResult::Failed)
        return EStateTreeRunStatus::Failed;

    Data.MoveTarget = Target;
    return EStateTreeRunStatus::Running;
}
