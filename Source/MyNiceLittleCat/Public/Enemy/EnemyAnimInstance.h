// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.16.
* 수정 일자: 2026.11.20. (GroundSlam 나이아가라 노티파이를 이름으로 가로채던 처리 제거 → UAnimNotify_EnemyFeedbackCue)
* Enemy의 애니메이션 블루프린트에서 필요한 캐릭터 정보 갱신
*/

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EnemyAnimInstance.generated.h"

class AEnemyCharacter;

UCLASS()
class MYNICELITTLECAT_API UEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public :

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected :

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bShouldMove = false;

private :

	TObjectPtr<AEnemyCharacter> Owner = nullptr;
	
};
