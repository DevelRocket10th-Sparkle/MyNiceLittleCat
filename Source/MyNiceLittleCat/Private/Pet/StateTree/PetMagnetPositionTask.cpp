#include "Pet/StateTree/PetMagnetPositionTask.h"
#include "Pet/PetCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
EStateTreeRunStatus FPetMagnetPositionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    Data.Movement.Reset();
    Data.bHasSavedMovement = false;
    Data.SafeLocation = FVector::ZeroVector;
    Data.bHasSafeLocation = false;
    Data.TimeUntilSafeSearch = 0.f;

    if (false == IsValid(Data.Pet.Get()) || false == IsValid(Data.Pet->GetPartnerPlayer()))
        return EStateTreeRunStatus::Failed;

    UCharacterMovementComponent* Movement = Data.Pet->GetCharacterMovement();
    if (false == IsValid(Movement)) return EStateTreeRunStatus::Failed;
    // 복귀 대기 중 자성을 다시 사용하면 이전 복구를 종료.
    // 이전 이동 모드가 복원된 뒤 아래에서 다시 저장됨.
    Data.Pet->CancelNavigationRecovery();

    // 종료할 때 되돌릴 설정을 기억
    Data.Movement = Movement;
    Data.SavedMovementMode = Movement->MovementMode;
    Data.SavedCustomMovementMode = Movement->CustomMovementMode;
    Data.bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
    Data.bHasSavedMovement = true;

    // 이전 AI 이동 요청을 종료
    if (AAIController* Controller = Cast<AAIController>(Data.Pet->GetController()))
        Controller->StopMovement();

    // 이 Task가 직접 위치와 회전을 관리
    Movement->DisableMovement();
    Movement->bOrientRotationToMovement = false;

    // 자성 시작 전의 현재 위치가 안전하면 초기 기억으로 사용
    const FVector InitialLocation = Data.Pet->GetActorLocation();
    if (true == Data.Pet->CanUseMagnetPosition(InitialLocation))
    {
        Data.SafeLocation = InitialLocation;
        Data.bHasSafeLocation = true;
    }

    // 진입한 프레임부터 위치를 적용!
    return Tick(Context, 0.f);
}

EStateTreeRunStatus FPetMagnetPositionTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (false == IsValid(Data.Pet.Get())) return EStateTreeRunStatus::Failed;

    APawn* Player = Data.Pet->GetPartnerPlayer();
    if (false == IsValid(Player)) return EStateTreeRunStatus::Failed;

    const FVector Direction = Data.Pet->GetAimDirection(); // 펫의 컨트롤러가 구한 마우스 커서가 향하는 방향을 가져와요
    const FVector TargetLocation = Player->GetActorLocation() + Direction * Data.DistanceFromPlayer + FVector(0.f, 0.f, Data.HeightOffset);
    const FRotator TargetRotation = Direction.Rotation();

    Data.TimeUntilSafeSearch -= DeltaTime;

    FVector PositionToUse;

    if (true == Data.Pet->CanUseMagnetPosition(TargetLocation))
    {
        // 원래 목표가 다시 유효해지면 즉시 원래 배치로 복귀
        PositionToUse = TargetLocation;
        //Data.bHasSafeLocation = false;
        Data.TimeUntilSafeSearch = 0.f;
    }
    else
    {
        const float MaxSafeDistance = FMath::Max(Data.MaxSafeDistanceFromPlayer, Data.DistanceFromPlayer);

        const bool bRememberedPositionUsable = Data.bHasSafeLocation && FVector::DistSquared2D(Data.SafeLocation, Player->GetActorLocation())
            <= FMath::Square(MaxSafeDistance) && Data.Pet->CanUseMagnetPosition(Data.SafeLocation);

        if (false == bRememberedPositionUsable)
        {
            Data.bHasSafeLocation = false;

            // 시작부터 안전 위치가 없거나,
            // 기억한 위치가 무효가 된 예외에서만 탐색
            if (Data.TimeUntilSafeSearch <= 0.f)
            {
                Data.TimeUntilSafeSearch = 0.2f;
                FVector NewSafeLocation;
                if (true == Data.Pet->TryFindSafeRecoveryLocation(TargetLocation, NewSafeLocation))
                {
                    Data.SafeLocation = NewSafeLocation;
                    Data.bHasSafeLocation = true;
                }
            }
        }

        if (false == Data.bHasSafeLocation)
        {
            // 잘못된 좌표로 이동하지 않고 다음 Tick에 재검사
            Data.Pet->SetActorRotation(TargetRotation);
            return EStateTreeRunStatus::Running;
        }

        // 원래 목표가 무효여도 최근 안전 위치가 유효하면 유지
        PositionToUse = Data.SafeLocation;
    }

    const bool bMoved = Data.Pet->SetActorLocationAndRotation(PositionToUse, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
    // 실제 배치에 성공한 위치를 가장 최근 안전 위치로 기억
    if (true == bMoved)
    {
        Data.SafeLocation = Data.Pet->GetActorLocation();
        Data.bHasSafeLocation = true;
    }

    return EStateTreeRunStatus::Running;
}

void FPetMagnetPositionTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (true == Data.bHasSavedMovement)
    {
        if (UCharacterMovementComponent* Movement = Data.Movement.Get())
        {
            Movement->bOrientRotationToMovement = Data.bSavedOrientRotationToMovement;
            //Movement->SetMovementMode(Data.SavedMovementMode, Data.SavedCustomMovementMode);
            if (APetCharacter* Pet = Data.Pet.Get())
                Pet->ResumeNavigationSafely(Data.SavedMovementMode, Data.SavedCustomMovementMode);
        }
    }

    Data.bHasSavedMovement = false;
    Data.Movement.Reset();
}
