#include "Pet/StateTree/PetMournTask.h"
#include "Pet/PetCharacter.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Pet/SpeechBubbleComponent.h"

EStateTreeRunStatus FPetMournTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    APetCharacter* Pet = Data.Pet.Get();
    if (false == IsValid(Pet)) return EStateTreeRunStatus::Failed;

    // 쓰러진 플레이어 쪽을 바라봐요 (수평 방향만)
    if (const APawn* Player = Pet->GetPartnerPlayer())
    {
        const FVector ToPlayer = (Player->GetActorLocation() - Pet->GetActorLocation()).GetSafeNormal2D();
        if (false == ToPlayer.IsNearlyZero())
            Pet->SetActorRotation(ToPlayer.Rotation());
    }

    // 몽타주가 없어도 실패하지 않고 그냥 서 있어요 (에셋 준비 전에도 동작)
    if (true == IsValid(Data.MournMontage.Get()))
        Pet->PlayAnimMontage(Data.MournMontage.Get(), Data.PlayRate);

    // 반응 시작과 동시에 말풍선 한 번
    // 의도: 대사는 StateTree에서 적고, 비워두면 그냥 몽타주만 재생해요
    //       말풍선 소리는 Say 안에서 같이 나요 (DA_PetSound의 SpeechBubbleSound)
    if (false == Data.MournMessage.IsEmpty())
    {
        if (USpeechBubbleComponent* Bubble = Pet->GetSpeechBubble())
            Bubble->Say(Data.MournMessage, Data.MessageDuration);
    }

    return EStateTreeRunStatus::Running;
}

void FPetMournTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);

    // 리스폰으로 빠르게 나가면 말풍선이 남아 있을 수 있어서 같이 닫아요
    if (APetCharacter* BubblePet = Data.Pet.Get())
        if (USpeechBubbleComponent* Bubble = BubblePet->GetSpeechBubble())
            Bubble->HideBubble();

    APetCharacter* Pet = Data.Pet.Get();
    if (false == IsValid(Pet) || false == IsValid(Data.MournMontage.Get())) return;

    // 리스폰으로 나갈 때 루프 몽타주를 끊어줘요 (안 끊으면 계속 재생돼요)
    if (UAnimInstance* Anim = Pet->GetMesh() ? Pet->GetMesh()->GetAnimInstance() : nullptr)
        Anim->Montage_Stop(Data.BlendOutTime, Data.MournMontage.Get());
}
