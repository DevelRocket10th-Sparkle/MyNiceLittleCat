// 공격 중 앞으로 전진시키는 노티파이 스테이트입니다.
// 노티파이 구간 동안 캐릭터 정면으로 Distance만큼 이동합니다 (몽타주 루트 모션에 더해짐, 벽에 막히면 멈춤).
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_Lunge.generated.h"

UCLASS(meta = (DisplayName = "Lunge"))
class MYNICELITTLECAT_API UAnimNotifyState_Lunge : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Lunge"); }

protected:
	// 구간 전체 동안 전진할 거리 (cm)
	UPROPERTY(EditAnywhere, Category = "Lunge", meta = (ClampMin = "0.0"))
	float Distance = 150.f;
private:
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, float> SpeedByMesh;
};
