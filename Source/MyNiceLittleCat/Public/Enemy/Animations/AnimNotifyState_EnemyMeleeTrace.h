// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.21.
* 수정 일자: 2026.11.17. (주석 정리)
* 근접 공격 판정 구간을 표시하는 AnimNotifyState
* - 에셋에 실행 상태를 저장하지 않고, 소유 Enemy에게 구간 시작/끝 Gameplay Event만 보냄
* - Event Payload
*   OptionalObject: 이 Notify 인스턴스 (판정 구간 식별: 같은 몽타주의 여러 구간 구분)
*   OptionalObject2: 이벤트를 보낸 메시 (Enemy 본체 메시인지 확인: 다른 메시의 같은 애니메이션 무시)
*/

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_EnemyMeleeTrace.generated.h"

UCLASS(meta = (DisplayName = "Enemy Melee Trace"))
class MYNICELITTLECAT_API UAnimNotifyState_EnemyMeleeTrace : public UAnimNotifyState
{
	GENERATED_BODY()

public :

	// 에디터 미리보기에서는 실행하지 않음
	UAnimNotifyState_EnemyMeleeTrace();

	// 판정 구간 시작 이벤트 전송 (서버, Enemy 본체 메시만)
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	// 판정 구간 끝 이벤트 전송
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	// 몽타주 타임라인에 표시할 이름
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Enemy Melee Trace"); }
};
