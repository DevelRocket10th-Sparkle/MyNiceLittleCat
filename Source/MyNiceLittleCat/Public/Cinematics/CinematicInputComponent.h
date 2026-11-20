// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20. (APlayerMainController에서 분리)
* 컷신 동안의 플레이어 입력 담당 (플레이어 컨트롤러의 기본 컴포넌트)
* - 컷신 시작: 게임플레이 IMC를 빼고 컷신 IMC(스킵만)를 넣음 → 허용 목록 방식이라 액션이 늘어도 자동 차단
* - 컷신 종료: 시작 시점에 적용돼 있던 IMC만 원래 우선순위로 복구 (튜토리얼 등으로 꺼둔 IMC는 그대로)
* - 스킵: IA_SkipCinematic을 SkipHoldSeconds 동안 누르면 서브시스템의 스킵 대상 컷신을 넘김 (키는 IMC 매핑을 따름)
* - 스킵 안내 위젯 생성·표시, 표시할 키 이름 제공
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Cinematics/CinematicTypes.h"
#include "CinematicInputComponent.generated.h"

class UCinematicSkipWidget;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionInstance;

UCLASS(ClassGroup = (Cinematic), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UCinematicInputComponent : public UActorComponent
{
	GENERATED_BODY()

public :

	UCinematicInputComponent();

	// 컨트롤러의 SetupInputComponent에서 호출: 스킵 액션 바인딩
	void BindInput(UEnhancedInputComponent* EnhancedInput);

	// 진행 중인 스킵 홀드 초기화 (포커스 잃음, 일시정지 메뉴 등)
	void ResetSkip();

	// 컷신 동안 빼둔 IMC면 지금 넣고 빼지 않고 복구 목록만 갱신 (처리했으면 true)
	// - 컷신 도중 펫 입력 등이 다시 살아나지 않게
	bool DeferContextWhileBlocked(UInputMappingContext* Context, bool bEnable, int32 Priority);

	/* 스킵 안내 위젯용 조회 */

	bool IsSkipPromptVisible() const;
	float GetSkipProgress() const { return SkipProgress; }

	// 스킵 액션에 매핑된 키 이름 (컷신 시작 때 IMC에서 읽어 둠)
	const FText& GetSkipKeyText() const { return SkipKeyText; }

public :

	// 컷신 입력 차단 직전 알림: 소유 컨트롤러가 진행 중인 펫 다이얼 등을 취소 (IMC가 빠지면 Completed/Canceled를 못 받음)
	FSimpleMulticastDelegate OnBeforeInputBlocked;

protected :

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :

	void HandleCutsceneStateChanged(bool bCutscenePlaying);

	// 누르는 동안(Started·Ongoing·Triggered): 경과 시간으로 진행도 계산, 다 차면 스킵
	void HandleSkipHeld(const FInputActionInstance& Instance);
	// 뗌(Completed·Canceled)
	void HandleSkipReleased(const FInputActionInstance& Instance);

	FCinematicHandle GetSkipTarget() const;
	void ShowSkipWidget();

public :

	// 컷신 스킵 액션 (IMC_Cinematic에 매핑, 트리거 없이 둠: 홀드 시간은 SkipHoldSeconds로 처리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Input")
	TObjectPtr<UInputAction> SkipAction;

	// 컷신 중에만 적용하는 IMC: SkipAction 매핑만 둠
	// - 비어 있으면 스킵 입력까지 막히지 않도록 컷신 입력 차단을 하지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Input")
	TObjectPtr<UInputMappingContext> CinematicContext;

	// 컷신 IMC 우선순위: 게임플레이 IMC보다 높게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Input")
	int32 CinematicContextPriority = 100;

	// 컷신 동안 빼둘 게임플레이 IMC (IMC_Player, IMC_Pet 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Input")
	TArray<TObjectPtr<UInputMappingContext>> CutsceneBlockedContexts;

	// 스킵까지 누르고 있어야 하는 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Input", meta = (ClampMin = "0.2", Units = "s"))
	float SkipHoldSeconds = 1.f;

	// 스킵 안내 위젯 (BP 자식으로 색·문구를 바꿀 수 있음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|UI")
	TSubclassOf<UCinematicSkipWidget> SkipWidgetClass;

private :

	// 컷신 시작 시 빼둔 IMC와 원래 우선순위: 종료 시 그대로 복구
	UPROPERTY(Transient)
	TMap<TObjectPtr<UInputMappingContext>, int32> SuspendedContexts;

	UPROPERTY(Transient)
	TObjectPtr<UCinematicSkipWidget> SkipWidget;

	FDelegateHandle CutsceneStateChangedHandle;

	// 컷신 입력 차단 중인지
	bool bInputBlocked = false;

	// 진행도를 쌓고 있는 스킵 대상: 바뀌면 다시 눌러야 함
	FCinematicHandle SkipTargetHandle;
	float SkipProgress = 0.f;

	// 스킵 직후·대상이 바뀐 직후에는 키를 뗐다가 다시 눌러야 진행 (연속 컷신을 한 번에 넘기지 않게)
	bool bSkipNeedsRelease = false;

	FText SkipKeyText;
};
