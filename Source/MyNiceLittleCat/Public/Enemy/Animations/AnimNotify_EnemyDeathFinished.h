// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.16.
* 수정 일자: 2026.11.17. (주석 정리)
* Enemy의 사망 몽타주 재생 후 처리할 내용 구현
* - 사망 몽타주의 연출이 끝나는 프레임에 배치: 소유 Enemy의 FinishDeath 호출
*/

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_EnemyDeathFinished.generated.h"

UCLASS(meta = (DisplayName = "Enemy Death Finished"))
class MYNICELITTLECAT_API UAnimNotify_EnemyDeathFinished : public UAnimNotify
{
	GENERATED_BODY()

public :

	// 소유 Enemy에게 사망 연출 종료 전달
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;
	
};
