#include "AnimNotify/AnimNotifyState_Telegraph.h"
#include "Components/SceneComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

void UAnimNotifyState_Telegraph::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	UpdateTelegraph(MeshComp, 0.f); // 빈 장판(진행도 0)으로 시작 -> 이전 공격의 꽉 찬 상태가 잠깐 보이지 않게
	SetTelegraphVisible(MeshComp, true); // 준비 동작 시작 -> 예고 장판 표시
}

void UAnimNotifyState_Telegraph::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	// 돌진 직전에 숨김. 몽타주가 중간에 끊겨도(보스 사망 등) End 가 불려서 장판이 남지 않음
	SetTelegraphVisible(MeshComp, false);
}

void UAnimNotifyState_Telegraph::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	// 예고 시간 동안 장판이 보스 쪽에서 앞으로 차오름 -> 다 차는 순간 = 돌진 시작 (피할 타이밍이 눈에 보임)
	UpdateTelegraph(MeshComp, GetNotifyProgress(MeshComp, Animation, EventReference));
}

void UAnimNotifyState_Telegraph::SetTelegraphVisible(USkeletalMeshComponent* MeshComp, bool bVisible) const
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (nullptr == Owner) return;

	for (UActorComponent* Comp : Owner->GetComponentsByTag(USceneComponent::StaticClass(), ComponentTag))
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
			SceneComp->SetVisibility(bVisible);
	}
}

float UAnimNotifyState_Telegraph::GetNotifyProgress(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const
{
	const FAnimNotifyEvent* Event = EventReference.GetNotify();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	if (nullptr == Event || nullptr == AnimInstance || nullptr == Montage) return 0.f;

	const float Duration = Event->GetDuration();
	if (Duration <= 0.f) return 1.f;

	// 노티파이는 몽타주 하나에 객체 하나라 여기에 시간을 쌓으면 보스가 여럿일 때 섞임
	// -> 몽타주 재생 위치로 계산 (재생 속도가 바뀌어도 정확)
	const float Position = AnimInstance->Montage_GetPosition(Montage);
	return FMath::Clamp((Position - Event->GetTriggerTime()) / Duration, 0.f, 1.f);
}

void UAnimNotifyState_Telegraph::UpdateTelegraph(USkeletalMeshComponent* MeshComp, float Progress) const
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (nullptr == Owner) return;

	// 끝나기 직전 구간에서 0 -> 1 (주황 -> 빨강, 밝기 올라감)
	const float DangerAlpha = FMath::Clamp((Progress - DangerStart) / FMath::Max(1.f - DangerStart, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const FLinearColor Color = FMath::Lerp(WarnColor, DangerColor, DangerAlpha);
	const float Emissive = (Progress >= FlashStart) ? FlashEmissive : FMath::Lerp(BaseEmissive, DangerEmissive, DangerAlpha);

	for (UActorComponent* Comp : Owner->GetComponentsByTag(UDecalComponent::StaticClass(), ComponentTag))
	{
		UDecalComponent* Decal = Cast<UDecalComponent>(Comp);
		if (nullptr == Decal) continue;

		// 처음 한 번만 다이내믹 머티리얼 생성 (이후엔 데칼이 가진 걸 재사용)
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial());
		if (nullptr == MID)
		{
			MID = Decal->CreateDynamicMaterialInstance();
			if (nullptr == MID) continue;

			// 실제 크기(cm)를 넘겨서 테두리 두께가 장판 크기와 상관없이 일정하게 보이게
			// (아래로 투영하는 데칼: DecalSize.Z = 앞뒤 절반, Y = 좌우 절반)
			const FVector HalfSize = Decal->DecalSize * Decal->GetComponentScale();
			MID->SetScalarParameterValue(TEXT("SizeX"), HalfSize.Z * 2.f);
			MID->SetScalarParameterValue(TEXT("SizeY"), HalfSize.Y * 2.f);
		}

		MID->SetScalarParameterValue(TEXT("Progress"), Progress);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("EmissiveIntensity"), Emissive);
	}
}
