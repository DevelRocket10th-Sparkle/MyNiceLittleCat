#pragma once
// 작성자: 우진
// 작성일자: 2026-11-03
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_Tutorial_CameraShake.generated.h"

// 애니메이션의 특정 프레임에 주변 카메라를 흔드는 노티파이 (보스 착지 등)
UCLASS()
class MYNICELITTLECAT_API UAnimNotify_Tutorial_CameraShake : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	// ================================================================================
protected:
	UPROPERTY(EditAnywhere, Category = "Camera Shake")
	TSubclassOf<class UCameraShakeBase> Shake; // 재생할 카메라 셰이크

	UPROPERTY(EditAnywhere, Category = "Camera Shake", meta = (ClampMin = "0.0"))
	float InnerRadius = 800.f;  // 이 거리 안이면 최대 세기

	UPROPERTY(EditAnywhere, Category = "Camera Shake", meta = (ClampMin = "0.0"))
	float OuterRadius = 3000.f; // 이 거리 밖이면 안 흔들림 (사이는 거리에 따라 약해짐)

};
