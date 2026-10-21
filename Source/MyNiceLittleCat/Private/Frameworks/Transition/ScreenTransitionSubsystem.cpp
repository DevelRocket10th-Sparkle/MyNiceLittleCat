/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 화면 전환(페이드) 연출 월드 서브시스템 구현

#include "Frameworks/Transition/ScreenTransitionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool UScreenTransitionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UScreenTransitionSubsystem::Deinitialize()
{
	ResetState();

	Super::Deinitialize();
}

bool UScreenTransitionSubsystem::PlayTransition(APlayerController* PlayerController, const FScreenTransitionSettings& Settings,
	TFunction<void()> OnMidpoint, TFunction<void()> OnFinished)
{
	if (bPlaying || !IsValid(PlayerController))
	{
		return false;
	}

	bPlaying = true;
	ActiveController = PlayerController;
	ActiveSettings = Settings;
	MidpointCallback = MoveTemp(OnMidpoint);
	FinishedCallback = MoveTemp(OnFinished);

	UE_LOG(LogTemp, Log, TEXT("[ScreenTransition] Start (FadeOut %.2f / Hold %.2f / FadeIn %.2f, BlockInput %d)"),
		ActiveSettings.FadeOutTime, ActiveSettings.HoldTime, ActiveSettings.FadeInTime, ActiveSettings.bBlockInput ? 1 : 0);

	if (ActiveSettings.bBlockInput)
	{
		SetInputBlocked(true);
	}

	// 어두워진 뒤에도 유지 (밝아지는 페이드를 따로 시작할 때까지)
	StartFade(0.f, 1.f, ActiveSettings.FadeOutTime, true);
	ScheduleNextPhase(ActiveSettings.FadeOutTime, &UScreenTransitionSubsystem::HandleFadeOutFinished);

	return true;
}

bool UScreenTransitionSubsystem::K2_PlayTransition(APlayerController* PlayerController, FScreenTransitionSettings Settings)
{
	return PlayTransition(PlayerController, Settings);
}

void UScreenTransitionSubsystem::HandleFadeOutFinished()
{
	if (!ActiveController.IsValid())
	{
		ResetState();
		return;
	}

	// 화면이 완전히 어두운 순간 (부활 텔레포트·체력 회복 등)
	UE_LOG(LogTemp, Log, TEXT("[ScreenTransition] Midpoint (screen dark)"));
	if (MidpointCallback)
	{
		MidpointCallback();
	}
	OnTransitionMidpoint.Broadcast();

	ScheduleNextPhase(ActiveSettings.HoldTime, &UScreenTransitionSubsystem::HandleHoldFinished);
}

void UScreenTransitionSubsystem::HandleHoldFinished()
{
	if (!ActiveController.IsValid())
	{
		ResetState();
		return;
	}

	StartFade(1.f, 0.f, ActiveSettings.FadeInTime, false);
	ScheduleNextPhase(ActiveSettings.FadeInTime, &UScreenTransitionSubsystem::HandleFadeInFinished);
}

void UScreenTransitionSubsystem::HandleFadeInFinished()
{
	// 콜백 안에서 다음 전환을 바로 시작할 수 있게 상태를 먼저 정리
	TFunction<void()> Finished = MoveTemp(FinishedCallback);
	ResetState();
	UE_LOG(LogTemp, Log, TEXT("[ScreenTransition] Finished"));

	if (Finished)
	{
		Finished();
	}
	OnTransitionFinished.Broadcast();
}

void UScreenTransitionSubsystem::CancelTransition()
{
	if (!bPlaying)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ScreenTransition] Cancelled"));

	if (APlayerController* PlayerController = ActiveController.Get())
	{
		if (PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->StopCameraFade();
		}
	}

	ResetState();
}

void UScreenTransitionSubsystem::ScheduleNextPhase(float Delay, void (UScreenTransitionSubsystem::*Phase)())
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (Delay > 0.f)
	{
		TimerManager.SetTimer(PhaseTimerHandle, this, Phase, Delay, false);
	}
	else
	{
		PhaseTimerHandle = TimerManager.SetTimerForNextTick(this, Phase);
	}
}

void UScreenTransitionSubsystem::StartFade(float FromAlpha, float ToAlpha, float Duration, bool bHoldWhenFinished) const
{
	const APlayerController* PlayerController = ActiveController.Get();
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	PlayerController->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, Duration,
		ActiveSettings.FadeColor, ActiveSettings.bFadeAudio, bHoldWhenFinished);
}

void UScreenTransitionSubsystem::SetInputBlocked(bool bBlocked)
{
	APlayerController* PlayerController = ActiveController.Get();
	if (!PlayerController || bInputBlocked == bBlocked)
	{
		return;
	}

	bInputBlocked = bBlocked;

	if (bBlocked)
	{
		// 컨트롤러 입력(펫 조작 등)과 폰 입력(이동·공격·구르기)을 모두 끔
		PlayerController->DisableInput(PlayerController);

		if (APawn* Pawn = PlayerController->GetPawn())
		{
			Pawn->DisableInput(PlayerController);
			BlockedPawn = Pawn;
		}
		return;
	}

	PlayerController->EnableInput(PlayerController);

	// 입력을 끈 폰을 아직 조종 중일 때만 다시 켬 (부활로 바뀐 새 폰은 원래 켜져 있음)
	APawn* Pawn = BlockedPawn.Get();
	if (Pawn && Pawn->GetController() == PlayerController)
	{
		Pawn->EnableInput(PlayerController);
	}
	BlockedPawn.Reset();
}

void UScreenTransitionSubsystem::ResetState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
	}

	SetInputBlocked(false);

	bPlaying = false;
	bInputBlocked = false;
	ActiveController.Reset();
	BlockedPawn.Reset();
	MidpointCallback = nullptr;
	FinishedCallback = nullptr;
}
