#include "Pet/StateTree/PetMagnetPositionTask.h"
#include "Pet/PetCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"

EStateTreeRunStatus FPetMagnetPositionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    Data.Movement.Reset();
    Data.bHasSavedMovement = false;

    if (false == IsValid(Data.Pet.Get()) || false == IsValid(Data.Pet->GetPartnerPlayer()))
        return EStateTreeRunStatus::Failed;

    // 이전 사용 때 값이 남지 않도록 시작 시 정상으로 초기화 (바로 아래 Tick에서 다시 계산됨)
    Data.Pet->SetMagnetPlacementValid(true);

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

    //// 자성 시작 전의 현재 위치가 안전하면 초기 기억으로 사용
    //const FVector InitialLocation = Data.Pet->GetActorLocation();
    //if (true == Data.Pet->CanUseMagnetPosition(InitialLocation))
    //{
    //    Data.SafeLocation = InitialLocation;
    //    Data.bHasSafeLocation = true;
    //}

    // 진입한 프레임부터 위치를 적용!
    return Tick(Context, 0.f);
}

EStateTreeRunStatus FPetMagnetPositionTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    if (false == IsValid(Data.Pet.Get())) return EStateTreeRunStatus::Failed;

    APawn* Player = Data.Pet->GetPartnerPlayer();
    if (false == IsValid(Player)) return EStateTreeRunStatus::Failed;

    const FVector Direction = Data.Pet->GetAimDirection(); // 마우스 커서 방향
    const FVector DesiredLocation = Player->GetActorLocation() + Direction * Data.DistanceFromPlayer;
    const FRotator TargetRotation = Direction.Rotation();

    // 바닥이 있으면 바닥 위에, 없으면(허공, 벽 속) 원래 궤도 높이 그대로
    // 의도: 공전 중에는 막지 않고 끝까지 따라가게 해서 안 가버리는!! 답답함을 없앴어요
    FVector TargetLocation;
    TryGetGroundedTargetLocation(Data, DesiredLocation, TargetLocation);

    Data.Pet->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);

    // 위치는 막지 않는 대신, 지금 자리가 정상인지만 기록
    // -> 자성 능력이 이 값을 보고 벽 속, 허공, 벽 너머에서는 자기장을 끔 (물체 벽 통과 방지)
    Data.Pet->SetMagnetPlacementValid(Data.Pet->IsMagnetPlacementValidAt(Data.Pet->GetActorLocation()));

    return EStateTreeRunStatus::Running;
}

void FPetMagnetPositionTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    APetCharacter* Pet = Data.Pet.Get();
    UCharacterMovementComponent* Movement = Data.Movement.Get();

    if (true == Data.bHasSavedMovement && true == IsValid(Pet) && true == IsValid(Movement))
    {
        Movement->bOrientRotationToMovement = Data.bSavedOrientRotationToMovement;

        const FVector CurrentLocation = Pet->GetActorLocation();
        FVector LandingLocation;

        // 다른 행동(MoveTo 등)으로 바뀌어서 나가는 경우엔 그 요청을 덮어쓰지 않음
        const bool bReturningToFollow = Pet->GetRequestedBehavior() == EPetBehavior::Follow;

        if (true == Pet->CanDropPetAt(CurrentLocation))
            // 1) 놓을 수 있는 자리 → 그 자리에 그대로 내려놓기
            Pet->ResumeNavigationSafely(Data.SavedMovementMode, Data.SavedCustomMovementMode);

        else if (true == bReturningToFollow && true == Pet->TryFindSafeRecoveryLocation(CurrentLocation, LandingLocation, true))
        {
            // 2) 놓을 수 없는 자리 → 가장 가까운 가능한 위치로 날아가기 (RecoverFollow)
            // 이동 모드를 먼저 되돌려야 RecoverFollow가 '걷기'를 저장했다가 착지 후 복원함
            // (DisableMovement 상태 그대로 넘기면 착지 후에도 이동 불가가 됨)
            Movement->SetMovementMode(Data.SavedMovementMode, Data.SavedCustomMovementMode);
            Pet->RequestBehavior(EPetBehavior::RecoverFollow);
        }

        else
            // 3) 갈 곳을 못 찾았거나 다른 행동으로 바뀌는 중 → 기존 안전 복구(재시도 + 순간이동)
            Pet->ResumeNavigationSafely(Data.SavedMovementMode, Data.SavedCustomMovementMode);
    }

    Data.bHasSavedMovement = false;
    Data.Movement.Reset();
}

bool FPetMagnetPositionTask::TryGetGroundedTargetLocation(const FInstanceDataType& Data, const FVector& DesiredLocation, FVector& OutLocation)
{
    OutLocation = DesiredLocation;

    APetCharacter* Pet = Data.Pet.Get();
    if (false == IsValid(Pet)) return false;

    APawn* Player = Pet->GetPartnerPlayer();
    UCapsuleComponent* Capsule = Pet->GetCapsuleComponent();
    UCharacterMovementComponent* Movement = Pet->GetCharacterMovement();

    if (false == IsValid(Player) || false == IsValid(Pet->GetWorld()) ||
        false == IsValid(Capsule) || false == IsValid(Movement))
        return false;

    // 플레이어 중심(기존)이 아니라 발 높이를 기준으로 탐색
    FVector PlayerFeet = Player->GetActorLocation();
    if (const ACharacter* PlayerCharacter = Cast<ACharacter>(Player))
    {
        if (const UCharacterMovementComponent* PlayerMovement = PlayerCharacter->GetCharacterMovement())
            PlayerFeet = PlayerMovement->GetActorFeetLocation();
    }

    //. XY는 원래 자성 목표 위치, Z는 플레이어 발 높이
    FVector ProbeLocation = DesiredLocation;
    ProbeLocation.Z = PlayerFeet.Z;

    const float SearchHeight = FMath::Max(Data.GroundSearchHeight, 1.f);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PetMagnetGround), false, Pet);
    QueryParams.AddIgnoredActor(Player);

    // 펫이 실제로 막히는 충돌 응답을 기준으로 바닥 탐색
    const FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());

    FHitResult GroundHit;
    const bool bFoundGround = Pet->GetWorld()->LineTraceSingleByChannel(
        GroundHit, ProbeLocation + FVector::UpVector * SearchHeight, ProbeLocation - FVector::UpVector * SearchHeight,
        Capsule->GetCollisionObjectType(), QueryParams, ResponseParams);

    if (false == bFoundGround || GroundHit.bStartPenetrating || false == Movement->IsWalkable(GroundHit))
        return false;

    // 실제 바닥 위에 캡슐을 배치
    constexpr float GroundClearance = 2.f;
    OutLocation = GroundHit.ImpactPoint + FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() + GroundClearance + Data.HeightOffset);
    return true;
}
