#include "AnimNotify/AnimNotify_Tutorial_CameraShake.h"
#include "Camera/CameraShakeBase.h"
#include "Kismet/GameplayStatics.h"

void UAnimNotify_Tutorial_CameraShake::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (nullptr == MeshComp || nullptr == Shake) return;

	UWorld* World = MeshComp->GetWorld();
	if (nullptr == World || false == World->IsGameWorld()) return; // 몽타주 에디터 미리보기에서는 안 흔듦

	// (월드) 셰이크: 착지 지점과 플레이어 사이 거리로 세기가 정해짐
	// -> 가까이서 피했으면 크게, 멀리 있으면 약하게 흔들려서 거리감이 생김
	UGameplayStatics::PlayWorldCameraShake(World, Shake, MeshComp->GetComponentLocation(), InnerRadius, OuterRadius);
}
