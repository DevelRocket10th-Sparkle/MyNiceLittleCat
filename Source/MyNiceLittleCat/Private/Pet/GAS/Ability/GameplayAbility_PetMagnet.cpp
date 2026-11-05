#include "Pet/GAS/Ability/GameplayAbility_PetMagnet.h"
#include "Pet/PetCharacter.h"
#include "Pet/GAS/Task/AbilityTask_PetTick.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
//#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Gimmicks/Magnetic/MagneticComponent.h"
#include "Animation/AnimMontage.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
    // 해당 섹션이 같은 정점 수로 이미 만들어져 있으면 true
    // 의도: 정점 수가 같으면 Create(프록시 재생성) 대신 Update(정점만 갱신)를 써서 매 틱 비용을 줄이기 위함
    bool CanUpdateMeshSection(UProceduralMeshComponent* Mesh, int32 SectionIndex, int32 VertexCount)
    {
        const FProcMeshSection* Section = Mesh->GetProcMeshSection(SectionIndex);
        return nullptr != Section && Section->ProcVertexBuffer.Num() == VertexCount;
    }

    // 머티리얼이 실제로 바뀔 때만 지정
    // 의도: 같은 머티리얼을 매 틱 다시 넣으면서 렌더 상태를 갱신하는 낭비를 막기 위함
    void SetMaterialIfChanged(UProceduralMeshComponent* Mesh, int32 ElementIndex, UMaterialInterface* Material)
    {
        if (nullptr != Material && Mesh->GetMaterial(ElementIndex) != Material)
            Mesh->SetMaterial(ElementIndex, Material);
    }
}

UGameplayAbility_PetMagnet::UGameplayAbility_PetMagnet()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGameplayAbility_PetMagnet::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    APetCharacter* Pet = ActorInfo ? Cast<APetCharacter>(ActorInfo->AvatarActor.Get())  : nullptr;

    if (false == IsValid(Pet) || false == Pet->IsPetInitialized())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (false == CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const FGameplayTag SpecialActionTag = FGameplayTag::RequestGameplayTag(TEXT("Pet.SpecialAction"));

    UAbilityTask_WaitGameplayEvent* SpecialActionTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
            this, SpecialActionTag, nullptr, false, true);

    SpecialActionTask->EventReceived.AddDynamic(this, &UGameplayAbility_PetMagnet::HandleSpecialAction);
    SpecialActionTask->ReadyForActivation();

    //! EndAbility 호출 일부러 안 함
    
    UAbilityTask_PetTick* TickTask = UAbilityTask_PetTick::CreatePetTick(this);
    TickTask->OnTick.AddDynamic( this, &UGameplayAbility_PetMagnet::HandleMagnetTick);
    TickTask->ReadyForActivation();
}

void UGameplayAbility_PetMagnet::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    StopPolarityChangeAnimation();

    // 우클릭 해제·모드 변경 등 모든 종료 경로에서 정리
    
    // 비정상 위치에서 끝나면 던지지 않고 마지막 정상 위치에 내려놓기
    const APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    if (true == IsValid(Pet) && false == Pet->IsMagnetPlacementValid())
        DropHeldTargetAtLastValidLocation();
    else
        ReleaseHeldTarget(100.f);

    HideMagnetFieldMesh(); // 모드 종료 시 자기장 표시 제거
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGameplayAbility_PetMagnet::HandleMagnetTick(float DeltaTime)
{
    // 펫이 벽 속, 허공, 벽 너머에 있으면 자기장 정지
    // 의도: 공전은 자유롭게 두되, 그 자리에서 자성을 쓰는 악용(물체 벽 통과 등)은 막음
    const APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    if (true == IsValid(Pet) && false == Pet->IsMagnetPlacementValid())
    {
        DropHeldTargetAtLastValidLocation();
        HideMagnetFieldMesh();
        return;
    }

    if (true == bDrawMagnetDebug) 
        DrawMagnetDebugRange();

    UpdateMagnetFieldMesh();

    TArray<AActor*> Candidates;
    CollectMagnetTargetsInRange(Candidates);

    if (false == HeldTarget.IsValid())
        BeginAttractTarget(FindNearestMagnetTarget(Candidates));

    UpdateHeldTarget(DeltaTime);
    ApplyNearbyRepulsion(Candidates);

    // 정상 위치일 때 물체 위치를 계속 기억 -> 비정상이 되면 여기로 내려놔요!!!!!!
    if (const AActor* Target = HeldTarget.Get())
    {
        LastValidHeldLocation = Target->GetActorLocation();
        bHasLastValidHeldLocation = true;
    }
}

void UGameplayAbility_PetMagnet::DrawMagnetDebugRange() const
{
#if ENABLE_DRAW_DEBUG
    const APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());

    if (false == IsValid(Pet) || nullptr == GetWorld())
        return;

    const FVector Forward = Pet->GetActorForwardVector().GetSafeNormal2D();
    FVector Origin = Pet->GetActorLocation() + Forward * ForwardOffset;

    // 계산은 몸통 높이에서 하고, 표시만 보기 쉽게 캡슐 하단 높이로 내림
    Origin.Z -= Pet->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Origin.Z += 3.f;

    TArray<FVector> Directions;
    TArray<float> ReachLengths;
    TraceMagnetFan(Directions, ReachLengths);

    const FColor Color = CurrentPolarity == EPetMagnetPolarity::N ? FColor::Red : FColor::Blue;
    const int32 LastIndex = Directions.Num() - 1;

    FVector PreviousPoint = FVector::ZeroVector;

    for (int32 Index = 0; Index < Directions.Num(); ++Index)
    {
        const FVector Point = Origin + Directions[Index] * ReachLengths[Index];

        // 광선 하나하나를 얇게 그림 → 그림의 "노란 영역"처럼 실제 영향 범위가 채워져 보임
        DrawDebugLine(GetWorld(), Origin, Point, FColor::Yellow, false, 0.f, 0, 0.5f);

        // 부채꼴 양옆의 직선
        if (0 == Index || LastIndex == Index)
            DrawDebugLine(GetWorld(), Origin, Point, Color, false, 0.f, 0, 2.f);

        // 끝점끼리 연결 → 벽에 막힌 곳은 윤곽이 벽 쪽으로 움푹 들어감
        if (Index > 0)
            DrawDebugLine(GetWorld(), PreviousPoint, Point, Color, false, 0.f, 0, 2.f);

        // 벽에 막힌 지점은 점으로 표시 (차폐가 제대로 되는지 확인용)
        if (ReachLengths[Index] < RangeLength - KINDA_SMALL_NUMBER)
            DrawDebugPoint(GetWorld(), Point, 8.f, FColor::Yellow, false, 0.f);

        PreviousPoint = Point;
    }
#endif
}

void UGameplayAbility_PetMagnet::TraceMagnetFan(TArray<FVector>& OutDirections, TArray<float>& OutReachLengths) const
{
    OutDirections.Reset();
    OutReachLengths.Reset();

    const APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    if (false == IsValid(Pet) || nullptr == GetWorld()) return;

    const FVector Forward = Pet->GetActorForwardVector().GetSafeNormal2D();

    // 판정(CollectMagnetTargetsInRange)과 같은 원점 = 펫 몸통 높이
    // 의도: 바닥 높이에서 쏘면 바닥 자체(World Static)나 작은 턱에 맞아버리기 때문
    const FVector TraceOrigin = Pet->GetActorLocation() + Forward * ForwardOffset;

    // 자기장을 막는 건 World Static 오브젝트만 (펫, 플레이어, 물리 물체는 통과)
    FCollisionObjectQueryParams ShieldTypes;
    ShieldTypes.AddObjectTypesToQuery(ECC_WorldStatic);

    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(PetMagnetShield));
    TraceParams.AddIgnoredActor(Pet);

    // 양 끝을 포함하므로 광선 개수는 FanRayCount + 1개
    for (int32 Index = 0; Index <= FanRayCount; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / FanRayCount;
        const float Angle = FMath::Lerp(-HalfAngleDegrees, HalfAngleDegrees, Alpha);
        const FVector Direction = Forward.RotateAngleAxis(Angle, FVector::UpVector);

        // 기본은 최대 사거리, 벽에 맞으면 맞은 거리까지만 = 차폐
        float ReachLength = RangeLength;

        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByObjectType(Hit, TraceOrigin, TraceOrigin + Direction * RangeLength, ShieldTypes, TraceParams))
            ReachLength = Hit.Distance;

        OutDirections.Add(Direction);
        OutReachLengths.Add(ReachLength);
    }
}

void UGameplayAbility_PetMagnet::CollectMagnetTargetsInRange(TArray<AActor*>& OutTargets) const
{
    OutTargets.Reset();

    const APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    if (false == IsValid(Pet) || nullptr == GetWorld()) return;

    const FVector Forward = Pet->GetActorForwardVector().GetSafeNormal2D();
    const FVector Origin = Pet->GetActorLocation() + Forward * ForwardOffset;

    FCollisionObjectQueryParams ObjectTypes;
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
    ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PetMagnetSearch));
    QueryParams.AddIgnoredActor(Pet);

    if (APawn* Player = Pet->GetPartnerPlayer())
        QueryParams.AddIgnoredActor(Player);

    TArray<FOverlapResult> Overlaps;

    GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectTypes,
        FCollisionShape::MakeBox(FVector(RangeLength, RangeLength, TargetHeightTolerance)), QueryParams);

    const float RangeSquared = FMath::Square(RangeLength);
    const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));

    TSet<AActor*> CheckedActors;

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();

        if (false == IsValid(Candidate) || Candidate == HeldTarget.Get() || true == CheckedActors.Contains(Candidate))
            continue;


        CheckedActors.Add(Candidate);

        if (false == IsMagnetCandidate(Candidate))
            continue;

        FVector ToTarget = Candidate->GetActorLocation() - Origin;
        if (FMath::Abs(ToTarget.Z) > TargetHeightTolerance)
            continue;

        ToTarget.Z = 0.f;
        if (ToTarget.SizeSquared() > RangeSquared)
            continue;

        if (false == ToTarget.IsNearlyZero())
        {
            const float Dot = FVector::DotProduct(Forward, ToTarget.GetSafeNormal());
            if (Dot < MinimumDot)
                continue;
        }

        // 부채꼴 안에 있어도 벽 뒤에 있으면 자기장이 닿지 않음
        // 의도: 트레이스는 비용이 있으므로 거리·각도 검사를 모두 통과한 후보에만 수행
        if (true == IsShieldedByWall(Origin, Candidate))
            continue;

        OutTargets.Add(Candidate);
    }
}

bool UGameplayAbility_PetMagnet::IsShieldedByWall(const FVector& Origin, const AActor* Target) const
{
    if (false == IsValid(Target) || nullptr == GetWorld())
        return false;

    // 대상의 피벗 대신 바운드 중심을 목표점으로 사용
    // 의도: 피벗이 바닥에 있는 메시라면 광선이 바닥(World Static)에 먼저 맞아 차폐로 오판하기 때문
    FVector TargetCenter;
    FVector TargetExtent;
    Target->GetActorBounds(true, TargetCenter, TargetExtent);

    // 디버그(TraceMagnetFan)와 같은 조건: World Static 오브젝트만 자기장을 막음
    FCollisionObjectQueryParams ShieldTypes;
    ShieldTypes.AddObjectTypesToQuery(ECC_WorldStatic);

    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(PetMagnetShieldTarget));
    TraceParams.AddIgnoredActor(GetAvatarActorFromActorInfo());
    TraceParams.AddIgnoredActor(Target);

    FHitResult Hit;
    return GetWorld()->LineTraceSingleByObjectType(Hit, Origin, TargetCenter, ShieldTypes, TraceParams);
}

AActor* UGameplayAbility_PetMagnet::FindNearestMagnetTarget(const TArray<AActor*>& Candidates) const
{
    const APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    if (false == IsValid(Pet) || nullptr == GetWorld())
        return nullptr;

    AActor* NearestTarget = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();

    TSet<AActor*> CheckedActors;

    for (AActor* Candidate : Candidates)
    {
        EPetMagnetPolarity TargetPolarity;

        if (false == TryGetTargetPolarity(Candidate, TargetPolarity))
            continue;

        if (TargetPolarity == CurrentPolarity)
            continue;

        const float DistanceSquared = FVector::DistSquared2D(
            Pet->GetActorLocation(), Candidate->GetActorLocation());

        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            NearestTarget = Candidate;
        }
    }
    return NearestTarget;
}

void UGameplayAbility_PetMagnet::ApplyNearbyRepulsion(const TArray<AActor*>& Candidates)
{
    const AActor* Pet = GetAvatarActorFromActorInfo();
    if (false == IsValid(Pet)) return;

    for (AActor* Candidate : Candidates)
    {
        if (false == IsValid(Candidate) || Candidate == HeldTarget.Get())
            continue;

        EPetMagnetPolarity TargetPolarity;

        if (false == TryGetTargetPolarity(Candidate, TargetPolarity))
            continue;

        if (CurrentPolarity != TargetPolarity)
            continue;

        //! 임시 물체 연결부 -> 루트 메시의 물리에 힘을 줘요
        UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Candidate->GetRootComponent());

        if (false == IsValid(Body) || false == Body->IsSimulatingPhysics())
            continue;

        FVector Direction = Candidate->GetActorLocation() - Pet->GetActorLocation();
        Direction = Direction.GetSafeNormal2D();

        if (true == Direction.IsNearlyZero())
            Direction = Pet->GetActorForwardVector().GetSafeNormal2D();

        const float OutwardSpeed = FVector::DotProduct(Body->GetPhysicsLinearVelocity(), Direction);
        const float SpeedGap = NearbyRepelSpeed - OutwardSpeed;

        // 이미 충분히 빠르게 날아가는 물체에는 추가 힘을 주지 않음
        if (SpeedGap <= 0.f) continue;

        constexpr float ResponseRate = 10.f;
        const float Acceleration = FMath::Clamp(SpeedGap * ResponseRate, 0.f, NearbyRepelMaxAcceleration);
        Body->AddForce(Direction * Acceleration, NAME_None, true);
    }
}

bool UGameplayAbility_PetMagnet::IsMagnetCandidate(const AActor* _actor) const
{
    EPetMagnetPolarity TargetPolarity;
    return TryGetTargetPolarity(_actor, TargetPolarity);
}

void UGameplayAbility_PetMagnet::HandleSpecialAction(FGameplayEventData Payload)
{
    CurrentPolarity = (CurrentPolarity == EPetMagnetPolarity::N) ? EPetMagnetPolarity::S: EPetMagnetPolarity::N;

    PlayPolarityChangeAnimation();
}

bool UGameplayAbility_PetMagnet::TryGetTargetPolarity(const AActor* Target, EPetMagnetPolarity& OutPolarity) const
{
    if (false == IsValid(Target)) return false;

    const UMagneticComponent* Magnetic = Target->FindComponentByClass<UMagneticComponent>();

    if (false == IsValid(Magnetic) || false == Magnetic->IsMagnetEnabled())
        return false;

    switch (Magnetic->GetPolarity())
    {
        case EMagneticPolarity::N: OutPolarity = EPetMagnetPolarity::N; return true;
        case EMagneticPolarity::S: OutPolarity = EPetMagnetPolarity::S; return true;
        default: return false;
    }
}

void UGameplayAbility_PetMagnet::BeginAttractTarget(AActor* Target)
{
    if (true == HeldTarget.IsValid() || false == IsValid(Target)) return;

    UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Target->GetRootComponent());
    if (false == IsValid(Body)) return;

    HeldTarget = Target;
    HeldBody = Body;
    bAttachedToCarryPoint = false;

    bSavedSimulatePhysics = Body->IsSimulatingPhysics();
    bSavedActorCollision = Target->GetActorEnableCollision();

    Body->SetSimulatePhysics(false);
    Target->SetActorEnableCollision(false);

    // 월드 기준 회전을 초기화한 자세로 끌어오기
    Target->SetActorRotation(FRotator::ZeroRotator);
}

void UGameplayAbility_PetMagnet::UpdateHeldTarget(float DeltaTime)
{
    AActor* Target = HeldTarget.Get();
    if (false == IsValid(Target)) return;

    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());

    USceneComponent* CarryPoint = (true == IsValid(Pet)) ? Pet->GetCarryPoint() : nullptr;

    if (false == IsValid(CarryPoint))
    {
        ReleaseHeldTarget(0.f);
        return;
    }

    EPetMagnetPolarity TargetPolarity;
    if (false == TryGetTargetPolarity(Target, TargetPolarity))
    {
        ReleaseHeldTarget(100.f);
        return;
    }

    // Q로 극성이 바뀌어서 같은 극이 되면 강하게 내팽겨쳐요
    if (CurrentPolarity == TargetPolarity)
    {
        float LaunchMultiplier = 1.f;

        if (const UMagneticComponent* Magnetic = Target->FindComponentByClass<UMagneticComponent>())
            LaunchMultiplier = Magnetic->GetLaunchSpeedMultiplier();

        const float LaunchSpeed = RepelSpeed * LaunchMultiplier;
        ReleaseHeldTarget(LaunchSpeed, RepelUpwardRatio);
        return;
    }

    if (true == bAttachedToCarryPoint)
        return;

    UPrimitiveComponent* Body = HeldBody.Get();
    if (false == IsValid(Body))
    {
        ReleaseHeldTarget(0.f);
        return;
    }

    const FVector CarryOffset = CalculateCarryOffset(Body);
    const FVector Destination = CarryPoint->GetComponentLocation() + CarryOffset;

    const FVector NextLocation = FMath::VInterpConstantTo(
        Target->GetActorLocation(), Destination, DeltaTime, AttractionSpeed);
    Target->SetActorLocation(NextLocation, false);

    constexpr float AttachDistance = 10.f;
    if (FVector::DistSquared(NextLocation, Destination) <= FMath::Square(AttachDistance))
    {
        // 정확한 보정 위치까지 이동한 뒤 부착
        Target->SetActorLocation(Destination, false);

        bAttachedToCarryPoint = Target->AttachToComponent(CarryPoint,
            FAttachmentTransformRules::KeepWorldTransform);

        if (false == bAttachedToCarryPoint)
            ReleaseHeldTarget(0.f);
    }
}

FVector UGameplayAbility_PetMagnet::CalculateCarryOffset(const UPrimitiveComponent* Body) const
{
    if (false == IsValid(Body)) return FVector::ZeroVector;

    // 현재 회전과 크기를 반영한 월드 바운드
    const FBoxSphereBounds Bounds = Body->CalcBounds(Body->GetComponentTransform());
    const FVector BottomCenter = Bounds.Origin - FVector::UpVector * Bounds.BoxExtent.Z;

    // 바닥 중앙에서 물체 피벗으로 향하는 거리
    return Body->GetComponentLocation() - BottomCenter;
}

void UGameplayAbility_PetMagnet::ReleaseHeldTarget(float ReleaseSpeed, float UpwardRatio)
{
    AActor* Target = HeldTarget.Get();
    UPrimitiveComponent* Body = HeldBody.Get();

    if (true == IsValid(Target))
    {
        Target->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Target->SetActorEnableCollision(bSavedActorCollision);

        if (true == IsValid(Body))
        {
            Body->SetSimulatePhysics(bSavedSimulatePhysics);

            if (true == bSavedSimulatePhysics && (ReleaseSpeed > 0.f))
            {
                const AActor* Pet = GetAvatarActorFromActorInfo();
                /*const FVector Direction = IsValid(Pet) ? 
                    Pet->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;

                Body->AddImpulse(Direction * ReleaseSpeed, NAME_None, true);*/

                const FVector Forward = (true == IsValid(Pet)) ? Pet->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
                const FVector Direction = (Forward + FVector::UpVector * UpwardRatio).GetSafeNormal();

                Body->SetPhysicsLinearVelocity(FVector::ZeroVector);

                Body->AddImpulse(Direction * ReleaseSpeed, NAME_None, true);
            }
        }

        // 캐릭터는 CharacterMovement를 통해 발사
        if (ACharacter* Character = Cast<ACharacter>(Target);
            Character && false == bSavedSimulatePhysics && ReleaseSpeed > 0.f)
        {
            const AActor* Pet = GetAvatarActorFromActorInfo();
            const FVector Forward = IsValid(Pet) ? Pet->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
            const FVector Direction = (Forward + FVector::UpVector * UpwardRatio).GetSafeNormal();
            Character->LaunchCharacter(Direction * ReleaseSpeed, true, true);
        }
    }

    HeldTarget.Reset();
    HeldBody.Reset();
    bAttachedToCarryPoint = false;
    bHasLastValidHeldLocation = false; // 다음 물체에 이전 위치가 남지 않도록
}

void UGameplayAbility_PetMagnet::DropHeldTargetAtLastValidLocation()
{
    AActor* Target = HeldTarget.Get();
    if (false == IsValid(Target)) return;

    // ReleaseHeldTarget이 기억값을 지우므로 먼저 복사
    const bool bHasSavedLocation = bHasLastValidHeldLocation;
    const FVector SavedLocation = LastValidHeldLocation;

    // 던지지 않고(속도 0) 놓기
    ReleaseHeldTarget(0.f);

    if (true == bHasSavedLocation)
        Target->SetActorLocation(SavedLocation, false, nullptr, ETeleportType::TeleportPhysics);

    // 물리 물체라면 남아 있던 속도 제거
    if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
    {
        if (true == Body->IsSimulatingPhysics())
            Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    }
}

void UGameplayAbility_PetMagnet::PlayPolarityChangeAnimation()
{
    if (false == IsActive() || false == IsValid(PolarityChangeMontage.Get()))
        return;

    StopPolarityChangeAnimation(); // Q 연타했을 때 고려

    PolarityChangeTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy
    (this, NAME_None, PolarityChangeMontage.Get(), 1.f, NAME_None, true);

    if (true == IsValid(PolarityChangeTask.Get()))
        PolarityChangeTask->ReadyForActivation();
}

void UGameplayAbility_PetMagnet::StopPolarityChangeAnimation()
{
    MontageStop(0.1f); // 재생 중인 Montage

    if (true == IsValid(PolarityChangeTask.Get()))
        PolarityChangeTask->EndTask();

    PolarityChangeTask = nullptr;
}

void UGameplayAbility_PetMagnet::UpdateMagnetFieldMesh()
{
    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    if (false == IsValid(Pet)) return;

    UProceduralMeshComponent* FieldMesh = Pet->GetMagnetFieldMesh();
    if (false == IsValid(FieldMesh)) return;

    // 판정·디버그와 같은 광선 결과를 사용 → 보이는 범위 = 실제로 작동하는 범위
    TArray<FVector> Directions;
    TArray<float> ReachLengths;
    TraceMagnetFan(Directions, ReachLengths);

    if (Directions.Num() < 2)
    {
        HideMagnetFieldMesh();
        return;
    }

    // 표시 원점: 디버그와 동일하게 캡슐 하단(발밑) 높이
    const FVector Forward = Pet->GetActorForwardVector().GetSafeNormal2D();
    FVector WorldOrigin = Pet->GetActorLocation() + Forward * ForwardOffset;
    WorldOrigin.Z -= Pet->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    WorldOrigin.Z += FieldMeshHeightOffset;

    // 정점은 컴포넌트 로컬 좌표여야 하므로, 월드 좌표를 컴포넌트 기준으로 변환
    const FTransform& MeshTransform = FieldMesh->GetComponentTransform();
    const FVector LocalUp = MeshTransform.InverseTransformVectorNoScale(FVector::UpVector);

    const int32 RayCount = Directions.Num();

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> VertexColors;

    Vertices.Reserve(RayCount + 1);
    Normals.Reserve(RayCount + 1);
    UVs.Reserve(RayCount + 1);
    VertexColors.Reserve(RayCount + 1);

    // 0번 정점(중심): A = 0 → 머티리얼에서 "중심에서 끝까지 비율"의 시작점
    Vertices.Add(MeshTransform.InverseTransformPosition(WorldOrigin));
    Normals.Add(LocalUp);
    UVs.Add(FVector2D(0.f, 0.5f));
    VertexColors.Add(FLinearColor(0.f, 0.f, 0.f, 0.f));

    // 1 ~ RayCount번 정점 = 광선마다 실제로 도달한 끝점
    for (int32 Index = 0; Index < RayCount; ++Index)
    {
        const FVector WorldPoint = WorldOrigin + Directions[Index] * ReachLengths[Index];

        Vertices.Add(MeshTransform.InverseTransformPosition(WorldPoint));
        Normals.Add(LocalUp);

        // U: 중심→끝 거리 비율(최대 사거리 기준), V: 부채꼴 왼쪽→오른쪽
        // 의도: 머티리얼에서 U로 "멀어질수록 흐려지는" 효과를 줄 수 있게
        UVs.Add(FVector2D(ReachLengths[Index] / RangeLength, static_cast<float>(Index) / (RayCount - 1)));

        // R: 벽에 막힌 광선인지 (1 = 차폐됨) → 머티리얼에서 벽 경계만 따로 빛나게
        // A: 광선 끝 = 1 → 중심과 보간되면서 "자기 광선 길이 기준 비율"이 됨 (끝 테두리 검출용)
        const bool bBlocked = ReachLengths[Index] < RangeLength - KINDA_SMALL_NUMBER;
        VertexColors.Add(FLinearColor(bBlocked ? 1.f : 0.f, 0.f, 0.f, 1.f));
    }

    if (CanUpdateMeshSection(FieldMesh, 0, Vertices.Num()))
        // 이미 같은 모양의 섹션이 있음 → 정점 위치/UV/색만 갱신 (삼각형 연결은 그대로)
        FieldMesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, UVs, VertexColors, TArray<FProcMeshTangent>());
    else
    {
        // 처음 켰을 때(또는 정점 수가 바뀌었을 때)만 삼각형을 만들고 섹션 생성
        Triangles.Reserve((RayCount - 1) * 3);

        // 중심(0)과 이웃한 끝점 두 개로 삼각형을 만듦 = 삼각형 팬
        for (int32 Index = 1; Index < RayCount; ++Index)
        {
            Triangles.Add(0);
            Triangles.Add(Index + 1);
            Triangles.Add(Index);
        }

        // 충돌은 만들지 않음(마지막 인자 false)
        FieldMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColors, TArray<FProcMeshTangent>(), false);
    }

    // 극성이 바뀌었을 때만 머티리얼 교체
    UMaterialInterface* FieldMaterial = CurrentPolarity == EPetMagnetPolarity::N ? FieldMaterialN.Get() : FieldMaterialS.Get();
    SetMaterialIfChanged(FieldMesh, 0, FieldMaterial);

    // 바닥 부채꼴과 같은 원점·광선 결과로 테두리 커튼 생성
    UpdateMagnetCurtainSection(FieldMesh, WorldOrigin, Directions, ReachLengths);
    FieldMesh->SetVisibility(true);
}

void UGameplayAbility_PetMagnet::HideMagnetFieldMesh()
{
    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    if (false == IsValid(Pet)) return;

    if (UProceduralMeshComponent* FieldMesh = Pet->GetMagnetFieldMesh())
    {
        FieldMesh->ClearAllMeshSections();
        FieldMesh->SetVisibility(false);
    }
}

void UGameplayAbility_PetMagnet::UpdateMagnetCurtainSection(UProceduralMeshComponent* FieldMesh, const FVector& WorldOrigin,
    const TArray<FVector>& Directions, const TArray<float>& ReachLengths)
{
    // 높이 0 = 커튼 끔. 이전 틱에 만든 섹션이 남지 않게 지워 줌
    if (CurtainHeight <= KINDA_SMALL_NUMBER || Directions.Num() < 2)
    {
        FieldMesh->ClearMeshSection(1);
        return;
    }

    const FTransform& MeshTransform = FieldMesh->GetComponentTransform();
    const int32 RayCount = Directions.Num();

    // ---- 1) 테두리 경로 만들기: 원점 → 광선 끝점들 → 원점 ----
    // Blocked: 벽에 막힌 지점인지 / ReachRatio: 원점(0) ~ 부채꼴 끝(1)
    struct FCurtainPoint
    {
        FVector WorldPos;
        float Blocked;
        float ReachRatio;
    };

    TArray<FCurtainPoint> Outline;
    Outline.Reserve(RayCount + 2);

    Outline.Add({ WorldOrigin, 0.f, 0.f }); // 왼쪽 직선 변의 시작(펫 발밑)
    for (int32 Index = 0; Index < RayCount; ++Index)
    {
        const bool bBlocked = ReachLengths[Index] < RangeLength - KINDA_SMALL_NUMBER;
        Outline.Add({ WorldOrigin + Directions[Index] * ReachLengths[Index], bBlocked ? 1.f : 0.f, 1.f });
    }
    Outline.Add({ WorldOrigin, 0.f, 0.f }); // 오른쪽 직선 변의 끝(다시 펫 발밑)

    // ---- 2) 점마다 바닥/꼭대기 정점 2개씩 생성 ----
    const int32 PointCount = Outline.Num();

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> VertexColors;

    Vertices.Reserve(PointCount * 2);
    Normals.Reserve(PointCount * 2);
    UVs.Reserve(PointCount * 2);
    VertexColors.Reserve(PointCount * 2);

    const FVector HeightOffset = FVector::UpVector * CurtainHeight;
    float DistanceAlong = 0.f; // 테두리를 따라 지금까지 온 거리 → U 좌표

    for (int32 Index = 0; Index < PointCount; ++Index)
    {
        const FCurtainPoint& Point = Outline[Index];

        if (Index > 0)
            DistanceAlong += FVector::Dist(Outline[Index - 1].WorldPos, Point.WorldPos);

        // 앞뒤 점으로 진행 방향을 구하고, 위쪽과 외적해서 수평 바깥 방향 법선을 만듦
        // (Unlit + Two Sided라 정확도는 크게 중요하지 않음, Fresnel 등을 쓸 때를 대비)
        const FVector Prev = Outline[FMath::Max(Index - 1, 0)].WorldPos;
        const FVector Next = Outline[FMath::Min(Index + 1, PointCount - 1)].WorldPos;
        const FVector WorldNormal = FVector::CrossProduct(Next - Prev, FVector::UpVector).GetSafeNormal();
        const FVector LocalNormal = MeshTransform.InverseTransformVectorNoScale(WorldNormal);

        const float U = DistanceAlong / CurtainUVTileLength;
        const FLinearColor Color(Point.Blocked, Point.ReachRatio, 0.f, 1.f);

        // 바닥 정점 (짝수 번호) : V = 0
        Vertices.Add(MeshTransform.InverseTransformPosition(Point.WorldPos));
        Normals.Add(LocalNormal);
        UVs.Add(FVector2D(U, 0.f));
        VertexColors.Add(Color);

        // 꼭대기 정점 (홀수 번호) : V = 1
        Vertices.Add(MeshTransform.InverseTransformPosition(Point.WorldPos + HeightOffset));
        Normals.Add(LocalNormal);
        UVs.Add(FVector2D(U, 1.f));
        VertexColors.Add(Color);
    }

    if (CanUpdateMeshSection(FieldMesh, 1, Vertices.Num()))
        // 커튼도 정점 수가 항상 같으므로 정점만 갱신
        FieldMesh->UpdateMeshSection_LinearColor(1, Vertices, Normals, UVs, VertexColors, TArray<FProcMeshTangent>());
    else
    {
        // ---- 이웃한 두 점의 바닥/꼭대기 4개로 사각형(삼각형 2개) ----
        Triangles.Reserve((PointCount - 1) * 6);

        for (int32 Index = 0; Index < PointCount - 1; ++Index)
        {
            const int32 Bottom0 = Index * 2;
            const int32 Top0 = Bottom0 + 1;
            const int32 Bottom1 = Bottom0 + 2;
            const int32 Top1 = Bottom0 + 3;

            Triangles.Append({ Bottom0, Top0, Bottom1 });
            Triangles.Append({ Top0, Top1, Bottom1 });
        }

        FieldMesh->CreateMeshSection_LinearColor(1, Vertices, Triangles, Normals, UVs, VertexColors, TArray<FProcMeshTangent>(), false);
    }

    // 극성에 맞는 커튼 머티리얼 (바뀔 때만 교체)
    UMaterialInterface* CurtainMaterial = CurrentPolarity == EPetMagnetPolarity::N ? CurtainMaterialN.Get() : CurtainMaterialS.Get();
    SetMaterialIfChanged(FieldMesh, 1, CurtainMaterial);
}