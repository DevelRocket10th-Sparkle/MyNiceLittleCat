// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/*
* 작성자: 전선현
* 작성 일자: 2026.10.16.
* Enemy의 사망 몽타주 재생 후 처리할 내용 구현
*/

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_EnemyDeathFinished.generated.h"

/**
 * 
 */
UCLASS(meta = (DisplayName = "Enemy Death Finished"))
class MYNICELITTLECAT_API UAnimNotify_EnemyDeathFinished : public UAnimNotify
{
	GENERATED_BODY()

public :

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;
	
};
