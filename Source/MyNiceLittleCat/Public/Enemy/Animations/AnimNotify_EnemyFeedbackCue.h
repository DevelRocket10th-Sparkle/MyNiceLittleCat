// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20.
* 몽타주의 특정 프레임에서 Enemy 연출 큐(소리 + 이펙트 또는 절차적 연출)를 재생
* - 내려찍기, 착지처럼 "맞든 안 맞든" 나는 연출용 (판정, 피해와 무관)
* - 위치: 메시 기준 앞쪽 거리 + 바닥 맞추기 (내려찍는 지점의 바닥에 깔리게)
* - 예전 EnemyAnimInstance가 나이아가라 노티파이를 이름으로 가로채던 처리를 대체
*/

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Enemy/Combat/EnemyFeedback.h"
#include "AnimNotify_EnemyFeedbackCue.generated.h"

UCLASS(meta = (DisplayName = "Enemy Feedback Cue"))
class MYNICELITTLECAT_API UAnimNotify_EnemyFeedbackCue : public UAnimNotify
{
	GENERATED_BODY()

public :

	// 지정한 위치에서 큐 재생 (게임 월드에서만)
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

#if WITH_EDITOR
	// 노티파이 트랙에 큐 이름 대신 알아보기 쉬운 이름 표시
	virtual FString GetNotifyName_Implementation() const override;
#endif

public :

	// 재생할 연출 (크기는 큐의 EffectScale)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FEnemyFeedbackCue Cue;

	// 메시 위치에서 캐릭터 앞쪽으로 떨어진 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (Units = "cm"))
	float ForwardOffset = 80.f;

	// 바닥에 맞출지: 위치 위아래로 WorldStatic 바닥을 찾아 그 지점에서 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	bool bSnapToFloor = true;

	// 바닥 찾기: 위치보다 이만큼 위에서 시작
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (EditCondition = "bSnapToFloor", ClampMin = "0.0", Units = "cm"))
	float FloorTraceUp = 50.f;

	// 바닥 찾기: 위치보다 이만큼 아래까지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (EditCondition = "bSnapToFloor", ClampMin = "0.0", Units = "cm"))
	float FloorTraceDown = 180.f;
};
