// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.17.
* 수정 일자: 2026.11.20. (절차적 연출을 큐의 ProceduralVisual로 지정: 에셋 이름/경로로 고르던 방식 제거)
* 수정 일자: 2026.11.21. (Enemy/Combat → Frameworks/Feedback, FEnemyFeedbackCue → FFeedbackCue: 적·스테이지·컷신 공용, 에셋은 CoreRedirects로 이어 읽음)
* 연출 피드백(소리 + 이펙트) 한 묶음과 재생 도우미: 적 공격, 스테이지 연출, 컷신 마커가 함께 사용
* - 소리와 이펙트는 각각 비워 둘 수 있음: 둘 다 비면 아무 일도 하지 않음
* - 판정, 피해, AI와 무관한 연출 전용: 서버 판정 로직 안에서 호출해도 결과에 영향 없음
*/

#pragma once

#include "CoreMinimal.h"
#include "FeedbackCue.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundAttenuation;
class USoundBase;

// 나이아가라 대신 재생할 절차적 연출 (AFeedbackVisual: 발광 조각 메시로 그리는 짧은 연출)
// - None이면 큐의 나이아가라 이펙트를 재생
UENUM(BlueprintType)
enum class EFeedbackVisualStyle : uint8
{
	None		UMETA(DisplayName = "없음 (나이아가라 사용)"),
	Impact		UMETA(DisplayName = "타격"),
	Landing		UMETA(DisplayName = "착지·내려찍기"),
	BossLanding	UMETA(DisplayName = "보스 착지"),
	Shatter		UMETA(DisplayName = "파괴"),
	DeathBurst	UMETA(DisplayName = "사망 파편"),
	Dissolve	UMETA(DisplayName = "시체 사라짐"),
	RushStart	UMETA(DisplayName = "돌진 시작"),
	RushTrail	UMETA(DisplayName = "돌진 궤적"),
	GoldGather	UMETA(DisplayName = "금화 모임"),
	GoldFlash	UMETA(DisplayName = "금화 섬광"),
	GoldTrail	UMETA(DisplayName = "금화 궤적"),
	GoldImpact	UMETA(DisplayName = "금화 충돌"),
	Arrival		UMETA(DisplayName = "도착"),
	Unlock		UMETA(DisplayName = "잠금 해제"),
	Stumble		UMETA(DisplayName = "비틀거림"),
	Warning		UMETA(DisplayName = "경고"),
	Staff		UMETA(DisplayName = "지팡이"),
	Levitate	UMETA(DisplayName = "공중 부양")
};

// 한 시점에 함께 재생할 소리와 이펙트
USTRUCT(BlueprintType)
struct FFeedbackCue
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

	// 나이아가라 대신 재생할 절차적 연출 (None이면 위 Effect 재생, 크기는 EffectScale 공용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	EFeedbackVisualStyle ProceduralVisual = EFeedbackVisualStyle::None;

	// 소리, 이펙트, 절차적 연출 중 하나라도 지정했는지
	bool IsSet() const { return Sound != nullptr || Effect != nullptr || ProceduralVisual != EFeedbackVisualStyle::None; }
};

// 붙여서 재생한 연출: 반복 소리/이펙트를 나중에 함께 끄기 위한 핸들
struct MYNICELITTLECAT_API FFeedbackInstance
{
	TWeakObjectPtr<UNiagaraComponent> Effect;
	TWeakObjectPtr<AActor> Visual;
	TWeakObjectPtr<UAudioComponent> Sound;

	// 이펙트는 새 입자 생성만 멈추고(남은 입자는 자연 소멸), 소리는 짧게 페이드아웃
	// - bDestroyEffect: 남은 입자를 기다리지 않고 이펙트를 바로 제거 (발사 순간 사라져야 하는 충전 이펙트 등)
	void Stop(float SoundFadeOutSeconds = 0.15f, bool bDestroyEffect = false);
};

namespace FeedbackCue
{
	// 월드 위치에서 소리와 이펙트 재생 (이펙트는 재생이 끝나면 자동 제거)
	MYNICELITTLECAT_API void PlayAtLocation(const UObject* WorldContext, const FFeedbackCue& Cue,
		const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);

	// 컴포넌트(소켓)에 붙여 재생: 반복 연출을 끌 때 쓰는 핸들 반환
	MYNICELITTLECAT_API FFeedbackInstance PlayAttached(const FFeedbackCue& Cue, USceneComponent* AttachTo,
		FName Socket = NAME_None, const FVector& Offset = FVector::ZeroVector);
}
