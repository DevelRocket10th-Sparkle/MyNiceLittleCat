// Fill out your copyright notice in the Description page of Project Settings.


#include "Cinematics/CinematicInputComponent.h"

#include "Cinematics/CinematicLog.h"
#include "Cinematics/CinematicSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Widgets/Cinematics/CinematicSkipWidget.h"

UCinematicInputComponent::UCinematicInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SkipWidgetClass = UCinematicSkipWidget::StaticClass();
}

void UCinematicInputComponent::BindInput(UEnhancedInputComponent* EnhancedInput)
{
	if (nullptr == EnhancedInput || nullptr == SkipAction)
	{
		return;
	}

	// 액션의 Hold 트리거는 한 번 발동 뒤 경과 시간이 끊겨 진행도와 맞지 않음 → 홀드는 여기서만 처리
	for (const UInputTrigger* Trigger : SkipAction->Triggers)
	{
		if (Trigger && Trigger->IsA<UInputTriggerHold>())
		{
			UE_LOG(LogCinematic, Warning, TEXT("%s has a Hold trigger: remove it, hold time is SkipHoldSeconds"), *SkipAction->GetName());
			break;
		}
	}

	EnhancedInput->BindAction(SkipAction, ETriggerEvent::Started, this, &UCinematicInputComponent::HandleSkipHeld);
	EnhancedInput->BindAction(SkipAction, ETriggerEvent::Ongoing, this, &UCinematicInputComponent::HandleSkipHeld);
	EnhancedInput->BindAction(SkipAction, ETriggerEvent::Triggered, this, &UCinematicInputComponent::HandleSkipHeld);
	EnhancedInput->BindAction(SkipAction, ETriggerEvent::Completed, this, &UCinematicInputComponent::HandleSkipReleased);
	EnhancedInput->BindAction(SkipAction, ETriggerEvent::Canceled, this, &UCinematicInputComponent::HandleSkipReleased);
}

void UCinematicInputComponent::BeginPlay()
{
	Super::BeginPlay();

	// 레벨 시작 컷신처럼 구독 전에 이미 재생 중일 수 있어 현재 상태도 바로 반영
	if (UCinematicSubsystem* Cinematics = GetWorld() ? GetWorld()->GetSubsystem<UCinematicSubsystem>() : nullptr)
	{
		CutsceneStateChangedHandle = Cinematics->OnCutsceneStateChanged.AddUObject(this, &UCinematicInputComponent::HandleCutsceneStateChanged);
		if (true == Cinematics->IsCutscenePlaying())
			HandleCutsceneStateChanged(true);
	}
}

void UCinematicInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SkipWidget)
	{
		SkipWidget->RemoveFromParent();
		SkipWidget = nullptr;
	}
	ResetSkip();

	if (UCinematicSubsystem* Cinematics = GetWorld() ? GetWorld()->GetSubsystem<UCinematicSubsystem>() : nullptr)
		Cinematics->OnCutsceneStateChanged.Remove(CutsceneStateChangedHandle);
	CutsceneStateChangedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

/* 컷신 입력 차단 */

void UCinematicInputComponent::HandleCutsceneStateChanged(bool bCutscenePlaying)
{
	if (bInputBlocked == bCutscenePlaying) return;

	const APlayerController* PC = GetOwner<APlayerController>();
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = (nullptr != LocalPlayer) ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (nullptr == Subsystem) return;

	// 컷신 전후로 누르고 있던 키는 뗄 때까지 무시 → 컷신이 끝나는 순간 공격/회피가 바로 나가지 않게
	FModifyContextOptions Options;
	Options.bIgnoreAllPressedKeysUntilRelease = true;

	if (true == bCutscenePlaying)
	{
		// 컷신 IMC가 없으면 게임플레이 IMC를 빼는 순간 스킵 입력도 사라지므로 차단하지 않음
		if (nullptr == CinematicContext)
		{
			UE_LOG(LogCinematic, Warning, TEXT("CinematicContext is not set on %s: cutscene input is not blocked"), *GetNameSafe(GetOwner()));
			return;
		}

		OnBeforeInputBlocked.Broadcast();

		SuspendedContexts.Reset();
		for (UInputMappingContext* Context : CutsceneBlockedContexts)
		{
			int32 Priority = 0;
			if (nullptr == Context || false == Subsystem->HasMappingContext(Context, Priority)) continue;

			SuspendedContexts.Add(Context, Priority);
			Subsystem->RemoveMappingContext(Context, Options);
		}

		Subsystem->AddMappingContext(CinematicContext, CinematicContextPriority, Options);

		// 안내 문구의 키 이름은 IMC 매핑을 따름 (키를 바꿔도 문구가 맞게)
		const TArray<FKey> Keys = SkipAction ? Subsystem->QueryKeysMappedToAction(SkipAction) : TArray<FKey>();
		SkipKeyText = Keys.Num() > 0 ? Keys[0].GetDisplayName(false) : FText::GetEmpty();

		ShowSkipWidget();
	}
	else
	{
		if (nullptr != CinematicContext)
			Subsystem->RemoveMappingContext(CinematicContext, Options);

		for (const TPair<TObjectPtr<UInputMappingContext>, int32>& Pair : SuspendedContexts)
		{
			if (nullptr != Pair.Key && false == Subsystem->HasMappingContext(Pair.Key))
				Subsystem->AddMappingContext(Pair.Key, Pair.Value, Options);
		}
		SuspendedContexts.Reset();
		ResetSkip();
	}

	bInputBlocked = bCutscenePlaying;
}

bool UCinematicInputComponent::DeferContextWhileBlocked(UInputMappingContext* Context, bool bEnable, int32 Priority)
{
	if (false == bInputBlocked || nullptr == Context || false == CutsceneBlockedContexts.Contains(Context))
	{
		return false;
	}

	if (true == bEnable)
		SuspendedContexts.FindOrAdd(Context, Priority);
	else
		SuspendedContexts.Remove(Context);
	return true;
}

/* 스킵 */

FCinematicHandle UCinematicInputComponent::GetSkipTarget() const
{
	const UCinematicSubsystem* Cinematics = GetWorld() ? GetWorld()->GetSubsystem<UCinematicSubsystem>() : nullptr;
	return Cinematics ? Cinematics->GetActiveSkippableCutscene() : FCinematicHandle();
}

bool UCinematicInputComponent::IsSkipPromptVisible() const
{
	const APlayerController* PC = GetOwner<APlayerController>();
	return PC && PC->IsLocalController() && !PC->IsPaused() && GetSkipTarget().IsValid();
}

void UCinematicInputComponent::ResetSkip()
{
	SkipProgress = 0.f;
	bSkipNeedsRelease = true;
}

void UCinematicInputComponent::HandleSkipHeld(const FInputActionInstance& Instance)
{
	// 새로 누름: 뗐다가 다시 누른 것이므로 대기 해제
	if (ETriggerEvent::Started == Instance.GetTriggerEvent())
	{
		bSkipNeedsRelease = false;
	}

	const FCinematicHandle Target = GetSkipTarget();
	if (Target != SkipTargetHandle)
	{
		// 누르는 도중 대상이 바뀌면 다시 눌러야 함
		SkipTargetHandle = Target;
		SkipProgress = 0.f;
		if (ETriggerEvent::Started != Instance.GetTriggerEvent())
			bSkipNeedsRelease = true;
	}

	const APlayerController* PC = GetOwner<APlayerController>();
	if (!Target.IsValid() || bSkipNeedsRelease || nullptr == PC || PC->IsPaused())
	{
		SkipProgress = 0.f;
		return;
	}

	SkipProgress = FMath::Min(1.f, Instance.GetElapsedTime() / FMath::Max(0.2f, SkipHoldSeconds));
	if (SkipProgress >= 1.f)
	{
		// 콜백이 바로 다음 컷신을 시작할 수 있어 대기 상태를 먼저 설정
		ResetSkip();
		if (UCinematicSubsystem* Cinematics = GetWorld() ? GetWorld()->GetSubsystem<UCinematicSubsystem>() : nullptr)
			Cinematics->TrySkipActiveCutscene();
	}
}

void UCinematicInputComponent::HandleSkipReleased(const FInputActionInstance& Instance)
{
	SkipProgress = 0.f;
	bSkipNeedsRelease = false;
}

void UCinematicInputComponent::ShowSkipWidget()
{
	APlayerController* PC = GetOwner<APlayerController>();
	if (nullptr == PC || false == PC->IsLocalController() || nullptr == SkipWidgetClass)
	{
		return;
	}

	if (nullptr == SkipWidget)
	{
		SkipWidget = CreateWidget<UCinematicSkipWidget>(PC, SkipWidgetClass);
		if (SkipWidget) SkipWidget->AddToPlayerScreen(2000);
	}

	// 위젯은 다 사라지면 스스로 접혀 Tick을 멈춤 → 컷신마다 다시 깨움
	if (SkipWidget) SkipWidget->Wake(this);
}
