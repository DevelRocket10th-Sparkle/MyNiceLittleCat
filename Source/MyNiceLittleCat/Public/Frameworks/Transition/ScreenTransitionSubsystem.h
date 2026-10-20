/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 화면 전환(페이드) 연출 월드 서브시스템입니다.
/// PlayTransition 한 번으로 "입력 잠금 + 화면 어둡게 → 어두운 상태 유지 → 밝게 → 입력 해제"를 재생합니다.
/// 화면이 완전히 어두워진 순간 중간 콜백(OnMidpoint)을, 다 밝아지고 입력이 풀린 뒤 끝 콜백(OnFinished)을 호출합니다.
/// - 페이드: PlayerCameraManager::StartCameraFade (별도 위젯 없음)
/// - 입력 잠금: 플레이어 컨트롤러와 조종 중인 폰의 입력을 끔 (이동·공격·구르기·펫 조작 등). 테스트 액터가 따로 받는 키는 막지 않음
/// 체크포인트 휴식 연출, 사망 후 부활 연출 등에서 함께 씁니다. 한 번에 하나만 재생하며 재생 중 요청은 무시합니다.
/// BP는 Play Transition 노드 + OnTransitionMidpoint / OnTransitionFinished 델리게이트로 사용합니다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScreenTransitionSubsystem.generated.h"

class APlayerController;
class APawn;

// 화면 전환 설정 (액터마다 디테일에서 수치 조정)
USTRUCT(BlueprintType)
struct FScreenTransitionSettings
{
	GENERATED_BODY()

	// 어두워지는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScreenTransition", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutTime = 0.5f;

	// 완전히 어두운 상태로 유지하는 시간 (이 동안 중간 처리)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScreenTransition", meta = (ClampMin = "0.0", Units = "s"))
	float HoldTime = 1.f;

	// 다시 밝아지는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScreenTransition", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScreenTransition")
	FLinearColor FadeColor = FLinearColor::Black;

	// 전환하는 동안 플레이어 입력 잠금
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScreenTransition")
	bool bBlockInput = true;

	// 화면과 함께 소리도 줄이기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScreenTransition")
	bool bFadeAudio = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScreenTransitionEvent);

UCLASS()
class MYNICELITTLECAT_API UScreenTransitionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 화면 전환 재생 (C++용). 재생 중이거나 컨트롤러가 없으면 false
	bool PlayTransition(APlayerController* PlayerController, const FScreenTransitionSettings& Settings,
		TFunction<void()> OnMidpoint = nullptr, TFunction<void()> OnFinished = nullptr);

	// 화면 전환 재생 (BP용, 중간·끝은 델리게이트로 받음)
	UFUNCTION(BlueprintCallable, Category = "ScreenTransition", meta = (DisplayName = "Play Transition"))
	bool K2_PlayTransition(APlayerController* PlayerController, FScreenTransitionSettings Settings);

	UFUNCTION(BlueprintPure, Category = "ScreenTransition")
	bool IsTransitionPlaying() const { return bPlaying; }

	// 재생 중인 전환을 즉시 끝냄 (화면 밝게·입력 해제, 남은 콜백은 호출하지 않음)
	UFUNCTION(BlueprintCallable, Category = "ScreenTransition")
	void CancelTransition();

public:
	// 화면이 완전히 어두워졌을 때
	UPROPERTY(BlueprintAssignable, Category = "ScreenTransition")
	FOnScreenTransitionEvent OnTransitionMidpoint;

	// 다시 밝아지고 입력이 풀렸을 때
	UPROPERTY(BlueprintAssignable, Category = "ScreenTransition")
	FOnScreenTransitionEvent OnTransitionFinished;

protected:
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleFadeOutFinished();
	void HandleHoldFinished();
	void HandleFadeInFinished();

	// Delay가 0 이하면 다음 틱에 호출 (타이머는 0초를 받지 않음)
	void ScheduleNextPhase(float Delay, void (UScreenTransitionSubsystem::*Phase)());

	void StartFade(float FromAlpha, float ToAlpha, float Duration, bool bHoldWhenFinished) const;

	void SetInputBlocked(bool bBlocked);

	// 상태 초기화 (입력 해제 포함)
	void ResetState();

private:
	TWeakObjectPtr<APlayerController> ActiveController;

	// 입력을 끈 폰 (전환 중 부활 등으로 폰이 바뀌면 새 폰은 건드리지 않음)
	TWeakObjectPtr<APawn> BlockedPawn;

	FScreenTransitionSettings ActiveSettings;

	TFunction<void()> MidpointCallback;

	TFunction<void()> FinishedCallback;

	FTimerHandle PhaseTimerHandle;

	bool bPlaying = false;

	bool bInputBlocked = false;
};
