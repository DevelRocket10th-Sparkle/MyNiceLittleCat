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
    // 우클릭 해제·모드 변경 등 모든 종료 경로에서 정리
    ReleaseHeldTarget(100.f);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGameplayAbility_PetMagnet::HandleMagnetTick(float DeltaTime)
{
    DrawMagnetDebugRange();

    TArray<AActor*> Candidates;
    CollectMagnetTargetsInRange(Candidates);

    if (false == HeldTarget.IsValid())
        BeginAttractTarget(FindNearestMagnetTarget(Candidates));

    UpdateHeldTarget(DeltaTime);
    ApplyNearbyRepulsion(Candidates);
}

void UGameplayAbility_PetMagnet::DrawMagnetDebugRange() const
{
#if ENABLE_DRAW_DEBUG
    const APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());

    if (false == IsValid(Pet) || nullptr == GetWorld())
        return;

    const FVector Forward =  Pet->GetActorForwardVector().GetSafeNormal2D();
    FVector Origin = Pet->GetActorLocation() + Forward * ForwardOffset;

    // 수평 범위를 보기 쉽게 캡슐 하단 높이에 표시
    Origin.Z -= Pet->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Origin.Z += 3.f;

    constexpr int32 SegmentCount = 24;
    const FColor Color = CurrentPolarity == EPetMagnetPolarity::N ? FColor::Red : FColor::Blue;

    FVector PreviousPoint = FVector::ZeroVector;

    for (int32 Index = 0; Index <= SegmentCount; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / SegmentCount;
        const float Angle = FMath::Lerp(-HalfAngleDegrees, HalfAngleDegrees, Alpha);
        const FVector Direction = Forward.RotateAngleAxis(Angle, FVector::UpVector);
        const FVector Point = Origin + Direction * RangeLength;

        // 부채꼴 양옆의 직선
        if (0 == Index || SegmentCount == Index)
            DrawDebugLine(GetWorld(), Origin, Point, Color, false, 0.f, 0, 2.f);

        // 앞쪽 곡선을 짧은 선들로 연결
        if (Index > 0)
            DrawDebugLine(GetWorld(), PreviousPoint, Point, Color, false, 0.f, 0, 2.f);

        PreviousPoint = Point;
    }
#endif
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
        OutTargets.Add(Candidate);
    }
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
    static const FName TestTargetTag(TEXT("Test.Pet.MagnetTarget"));
    return IsValid(_actor) && _actor->ActorHasTag(TestTargetTag);
}

void UGameplayAbility_PetMagnet::HandleSpecialAction(FGameplayEventData Payload)
{
    CurrentPolarity = (CurrentPolarity == EPetMagnetPolarity::N) ? EPetMagnetPolarity::S: EPetMagnetPolarity::N;
}

bool UGameplayAbility_PetMagnet::TryGetTargetPolarity(const AActor* Target, EPetMagnetPolarity& OutPolarity) const
{
    if (false == IsValid(Target)) return false;

    static const FName NTag(TEXT("Test.Pet.Pole.N"));
    static const FName STag(TEXT("Test.Pet.Pole.S"));

    const bool bN = Target->ActorHasTag(NTag);
    const bool bS = Target->ActorHasTag(STag);

    // 없거나, 두 극성이 동시에 지정되어 있으면 제외
    if (bN == bS) return false;

    OutPolarity = bN ? EPetMagnetPolarity::N : EPetMagnetPolarity::S;
    return true;
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

    //! 테스트.. 직접 이동시키는 동안 물리와 충돌을 잠시 꺼버려잇
    Body->SetSimulatePhysics(false);
    Target->SetActorEnableCollision(false);
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
        ReleaseHeldTarget(RepelSpeed, RepelUpwardRatio);
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
    }

    HeldTarget.Reset();
    HeldBody.Reset();
    bAttachedToCarryPoint = false;
}
