// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.30.
* 수정 일자: 2026.11.20. (BP Tick 연출을 C++로, 잠금 상태는 페이즈 진행 상태 알림으로 갱신)
* 보스 체력바 위젯 (메인 HUD에 포함)
* - 보스 이름, 페이즈 구간 표시, 잠금(무적) 문구와 깜빡임
* - 연출: 등장 채움, 피격 번쩍임, 흔들림, 위험 체력 맥박 (진행 중인 연출이 있을 때만 계산)
*/

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "BossHealthBarWidget.generated.h"

class ABossCharacter;
class UEnemyAttributeSet;
class UBossPhaseComponent;
class UBossPhaseConfigData;
class UResourceBarWidget;
class UTextBlock;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UWidget;

// 체력바 잠금 상태
UENUM(BlueprintType)
enum class EBossHealthBarLockMode : uint8
{
	None,		// 일반 전투
	Transition,	// 페이즈 전환 요청 또는 진행 중
	Barrier,	// 배리어(기믹) 페이즈: 기믹 완료 전까지 피가 닳지 않음
	FollowBoss	// 전환 컷이 끝나고 플레이어가 다음 아레나에 들어오기를 기다리는 중
};

/**
 * 여러 보스가 공유하는 보스 체력바
 * - 보스 이름: ABossCharacter::BossDisplayName 또는 SetBossName()
 * - 페이즈 경계: 보스 PhaseComponent의 PhaseConfig 전투 페이즈 체력 하한으로 자동 생성 (없으면 DefaultPhaseMarkers)
 *   SetPhaseMarkers()로 직접 지정도 가능
 */
UCLASS()
class MYNICELITTLECAT_API UBossHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public :

	UBossHealthBarWidget(const FObjectInitializer& ObjectInitializer);

	// 체력바 초기화: 보스에게 연결할 때 이름, 페이즈 경계, 체력 구독을 모두 갱신
	void InitializeBossHealthBar(ABossCharacter* Boss);

	// 현재 연결된 보스 델리게이트 해제
	void UnbindBoss();

	UFUNCTION(BlueprintCallable, Category = "Health Bar")
	void SetShowHealthText(bool bShow);

	// 보스 이름 텍스트 변경
	UFUNCTION(BlueprintCallable, Category = "Health Bar")
	void SetBossName(const FText& InName);

	// 페이즈 경계 비율(0~1 사이) 지정: 개수와 위치 모두 배열로 결정 (예: {0.7, 0.3})
	UFUNCTION(BlueprintCallable, Category = "Health Bar|Phase")
	void SetPhaseMarkers(const TArray<float>& InRatios);

	// 페이즈 경계 표시 제거 (단일 페이즈 보스용)
	UFUNCTION(BlueprintCallable, Category = "Health Bar|Phase")
	void ClearPhaseMarkers();

	// Getter: 현재 적용된 페이즈 경계 (내림차순)
	UFUNCTION(BlueprintPure, Category = "Health Bar|Phase")
	const TArray<float>& GetPhaseMarkers() const { return CurrentMarkers; }

	// PhaseConfig에서 전투 페이즈의 체력 하한(0 초과)만 뽑아 경계 배열 생성
	static TArray<float> BuildMarkersFromPhaseConfig(const UBossPhaseConfigData* Config);

protected :

	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public :

	// 현재 체력바 잠금 상태: 페이즈 전환 중이거나 배리어 페이즈면 피가 닳지 않음
	UFUNCTION(BlueprintPure, Category = "Health Bar|Lock")
	EBossHealthBarLockMode GetLockMode() const;

protected :

	// 페이즈 전환 중 잠금 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Lock")
	FText TransitionLockMessage = NSLOCTEXT("BossHealthBar", "TransitionLock", "페이즈 전환 중");

	// 배리어 페이즈 잠금 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Lock")
	FText BarrierLockMessage = NSLOCTEXT("BossHealthBar", "BarrierLock", "배리어를 파괴하라!");

	// 전환 컷 종료 후 다음 아레나 진입 대기 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Lock")
	FText FollowBossLockMessage = NSLOCTEXT("BossHealthBar", "FollowBossLock", "보스를 따라가라!");

	// 잠금 문구 깜빡임 속도 (초당 횟수, 0이면 깜빡이지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Lock", meta = (ClampMin = "0.0"))
	float LockTextBlinkRate = 1.5f;

	// 깜빡일 때 가장 어두운 순간의 투명도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Lock", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockTextMinOpacity = 0.2f;

	// 잠금 시 바를 덮는 어두운 막
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockShade = nullptr;

	// 잠금 시 빗금
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockStripes = nullptr;

	// 잠금 문구
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LockText = nullptr;

	// 체력 비율이 바뀔 때 BP에서 피격 플래시, 흔들림, 위험 상태 연출을 붙이기 위한 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Health Bar")
	void OnHealthRatioChanged(float NewRatio, float OldRatio);

	// 페이즈 경계를 넘었을 때 호출: PassedCount = 지금까지 넘은 경계 수
	UFUNCTION(BlueprintImplementableEvent, Category = "Health Bar|Phase")
	void OnPhaseMarkerPassed(int32 PassedCount);

private :

	void HandleHealthChanged(float NewHealth, float MaxHealth);
	void HandlePhaseRunStateChanged();
	void RefreshLockDisplay(float DeltaTime = 0.f);

	/* 연출 */
	void PlayHealthChangedFx(float NewRatio, float OldRatio);
	void PlayPhasePassedFx();
	void TickFx(float DeltaTime);
	void RebuildMarkers();
	void RebuildPips();
	void RefreshPips();
	int32 CountPassedMarkers(float Ratio) const;
	static TArray<float> NormalizeMarkers(const TArray<float>& InRatios);

protected :

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar", meta = (DisplayName = "Show Health Text"))
	bool bShowHealthText = true;

	// 보스에 이름이 없을 때 / 디자이너 미리보기용 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar")
	FText DefaultBossName = NSLOCTEXT("BossHealthBar", "DefaultBossName", "Boss");

	// true면 보스 PhaseComponent의 PhaseConfig 체력 하한으로 경계 자동 생성
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Phase")
	bool bUsePhaseConfigMarkers = true;

	// 보스 쪽에서 경계를 얻지 못했을 때 사용하는 기본값 (디자이너 미리보기에도 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Phase")
	TArray<float> DefaultPhaseMarkers;	// 생성자에서 {0.7, 0.3}으로 초기화

	// 경계선 이미지 (골드 선 텍스처 또는 단색)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Phase")
	FSlateBrush MarkerBrush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Phase", meta = (ClampMin = "1.0"))
	float MarkerWidth = 4.f;

	// 남은 페이즈 아이콘 (다이아몬드 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Phase")
	FSlateBrush PipBrush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Phase")
	FLinearColor PipActiveColor = FLinearColor(0.328f, 0.074f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Phase")
	FLinearColor PipInactiveColor = FLinearColor(0.024f, 0.010f, 0.054f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Phase", meta = (ClampMin = "0.0"))
	float PipSpacing = 10.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UResourceBarWidget> HealthBar = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText = nullptr;

	// HealthBar와 같은 크기로 겹쳐 둔 CanvasPanel: 경계선이 여기에 생성됨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> MarkerLayer = nullptr;

	// 페이즈 아이콘을 담는 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PhasePipBox = nullptr;

	// 피격·페이즈 경계 통과 때 번쩍이는 이미지 (위험 체력일 때 맥박도 여기에)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HitFlash = nullptr;

	// 흔들림을 줄 테두리 묶음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> FrameOverlay = nullptr;

	/* 연출 설정 */

	// 체력바가 다시 보일 때 바가 0에서 채워지는 시간 (이만큼 Tick이 끊겼다가 돌아오면 다시 보인 것으로 봄)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.0", Units = "s"))
	float IntroSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.0", Units = "s"))
	float IntroReshowGapSeconds = 0.3f;

	// 피격 번쩍임이 꺼지는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.01", Units = "s"))
	float FlashFadeSeconds = 0.12f;

	// 이 비율 이상 한 번에 줄면 흔들림
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShakeMinRatioDrop = 0.05f;

	// 피격 흔들림 시간 (흔들림 세기의 기준 시간이기도 함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.01", Units = "s"))
	float HitShakeSeconds = 0.18f;

	// 페이즈 경계 통과 흔들림 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.0", Units = "s"))
	float PhaseShakeSeconds = 0.3f;

	// 흔들림 진동 속도(라디안/초)와 최대 이동량(px)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.0"))
	float ShakeFrequency = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.0"))
	float ShakeAmplitude = 5.f;

	// 이 비율 이하(0 초과)면 위험 체력: 바 색 변경 + 번쩍임 이미지 맥박
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthRatio = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects")
	FLinearColor NormalBarColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects")
	FLinearColor LowHealthBarColor = FLinearColor(1.f, 0.45f, 0.6f, 1.f);

	// 위험 체력 맥박: 불투명도 Amount × (1 + sin(t × Speed))
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float LowHealthPulseAmount = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar|Effects", meta = (ClampMin = "0.0"))
	float LowHealthPulseSpeed = 6.981f;

private :

	// 현재 연결된 보스의 스탯
	TWeakObjectPtr<UEnemyAttributeSet> ObservedStats;

	// 현재 연결된 보스 (잠금 상태 조회용)
	TWeakObjectPtr<ABossCharacter> ObservedBoss;

	// 진행 상태 알림을 구독한 페이즈 컴포넌트
	TWeakObjectPtr<UBossPhaseComponent> ObservedPhase;
	FDelegateHandle PhaseRunStateHandle;

	// 마지막으로 판정한 잠금 상태: 전환 중(Transition·FollowBoss)일 때만 매 프레임 다시 판정
	EBossHealthBarLockMode CachedLockMode = EBossHealthBarLockMode::None;

	// 현재 적용된 경계 (내림차순, 0~1 사이)
	TArray<float> CurrentMarkers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PipImages;

	float LastRatio = 1.f;
	int32 LastPassedCount = 0;

	// 잠금 문구 깜빡임 진행 시간 (잠금이 풀리면 0으로 초기화)
	float LockBlinkTime = 0.f;

	/* 연출 진행 값 */
	double LastTickRealTime = 0.0;
	float IntroTime = 0.f;
	float ShakeTime = 0.f;
	float PulseTime = 0.f;
	bool bLowHealth = false;

};
