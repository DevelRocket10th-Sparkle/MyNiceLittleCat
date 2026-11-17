// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.17.
* Enemy 연출 피드백(소리 + 이펙트) 한 묶음과 재생 도우미
* - 소리와 이펙트는 각각 비워 둘 수 있음: 둘 다 비면 아무 일도 하지 않음
* - 판정, 피해, AI와 무관한 연출 전용: 서버 판정 로직 안에서 호출해도 결과에 영향 없음
*/

#pragma once

#include "CoreMinimal.h"
#include "EnemyFeedback.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundAttenuation;
class USoundBase;

// 한 시점에 함께 재생할 소리와 이펙트
USTRUCT(BlueprintType)
struct FEnemyFeedbackCue
{
	GENERATED_BODY()

	// 재생할 소리 (비우면 소리 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<USoundBase> Sound;

	// 소리 감쇠 설정 (비우면 사운드 에셋 설정 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", AdvancedDisplay)
	TObjectPtr<USoundAttenuation> Attenuation;

	// 생성할 나이아가라 이펙트 (비우면 이펙트 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UNiagaraSystem> Effect;

	// 이펙트 크기 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0.01"))
	float EffectScale = 1.f;

	// 소리 또는 이펙트 중 하나라도 지정했는지
	bool IsSet() const { return Sound != nullptr || Effect != nullptr; }
};

// 붙여서 재생한 연출: 반복 소리/이펙트를 나중에 함께 끄기 위한 핸들
struct MYNICELITTLECAT_API FEnemyFeedbackInstance
{
	TWeakObjectPtr<UNiagaraComponent> Effect;
	TWeakObjectPtr<AActor> Visual;
	TWeakObjectPtr<UAudioComponent> Sound;

	// 이펙트는 새 입자 생성만 멈추고(남은 입자는 자연 소멸), 소리는 짧게 페이드아웃
	void Stop(float SoundFadeOutSeconds = 0.15f);
};

namespace EnemyFeedback
{
	// 월드 위치에서 소리와 이펙트 재생 (이펙트는 재생이 끝나면 자동 제거)
	MYNICELITTLECAT_API void PlayAtLocation(const UObject* WorldContext, const FEnemyFeedbackCue& Cue,
		const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, FName StageCue = NAME_None);

	// 컴포넌트(소켓)에 붙여 재생: 반복 연출을 끌 때 쓰는 핸들 반환
	MYNICELITTLECAT_API FEnemyFeedbackInstance PlayAttached(const FEnemyFeedbackCue& Cue, USceneComponent* AttachTo,
		FName Socket = NAME_None, const FVector& Offset = FVector::ZeroVector, FName StageCue = NAME_None);
}
