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

void UGameplayAbility_PetMagnet::HandleMagnetTick(float DeltaTime)
{
    DrawMagnetDebugRange();

    AActor* Target = FindNearestMagnetTarget();

#if ENABLE_DRAW_DEBUG
    if (IsValid(Target) && GetWorld())
    {
        FVector BoundsOrigin;
        FVector BoundsExtent;
        Target->GetActorBounds(false, BoundsOrigin, BoundsExtent);

        const FVector MarkerPosition = BoundsOrigin + FVector(0.f, 0.f, BoundsExtent.Z + 35.f);
        DrawDebugSphere(GetWorld(), MarkerPosition, 20.f, 16, FColor::Yellow, false, 0.f, 0, 2.f);
    }
#endif
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

AActor* UGameplayAbility_PetMagnet::FindNearestMagnetTarget() const
{
    const APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    if (false == IsValid(Pet) || nullptr == GetWorld())
        return nullptr;

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

    AActor* NearestTarget = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();

    TSet<AActor*> CheckedActors;

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();
        if (false == IsValid(Candidate) || true == CheckedActors.Contains(Candidate))
            continue;

        CheckedActors.Add(Candidate);

        if (false == IsMagnetCandidate(Candidate))
            continue;

        FVector ToTarget = Candidate->GetActorLocation() - Origin;

        if (FMath::Abs(ToTarget.Z) > TargetHeightTolerance)
            continue;

        ToTarget.Z = 0.f;

        // 부채꼴 길이 검사
        const float DistanceFromOriginSquared = ToTarget.SizeSquared();
        if (DistanceFromOriginSquared > RangeSquared)
            continue;

        // 부채꼴 각도 검사
        if (false == ToTarget.IsNearlyZero())
        {
            const float Dot = FVector::DotProduct(Forward, ToTarget.GetSafeNormal());
            if (Dot < MinimumDot)
                continue;
        }

        // 범위 안의 후보 중 펫과 가장 가까운 액터를 고릅니다.
        const float DistanceFromPetSquared = FVector::DistSquared2D(Pet->GetActorLocation(), Candidate->GetActorLocation());

        if (DistanceFromPetSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceFromPetSquared;
            NearestTarget = Candidate;
        }
    }

    return NearestTarget;
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
