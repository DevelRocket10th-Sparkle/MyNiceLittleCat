#include "Pet/GAS/Ability/GameplayAbility_PetMagnet.h"
#include "Pet/PetCharacter.h"
#include "Pet/GAS/Task/AbilityTask_PetTick.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

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

    //! EndAbility 호출 일부러 안 함
    
    UAbilityTask_PetTick* TickTask = UAbilityTask_PetTick::CreatePetTick(this);
    TickTask->OnTick.AddDynamic( this, &UGameplayAbility_PetMagnet::HandleMagnetTick);
    TickTask->ReadyForActivation();
}

void UGameplayAbility_PetMagnet::HandleMagnetTick(float DeltaTime)
{
    DrawMagnetDebugRange();
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
    const FColor Color = FColor::Cyan;

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
