// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Enemy/BossHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/BossPhaseComponent.h"
#include "Enemy/Data/BossPhaseConfigData.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Widgets/ResourceBarWidget.h"

UBossHealthBarWidget::UBossHealthBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPhaseMarkers = { 0.7f, 0.3f };

	// 경계선 기본: 골드 단색
	MarkerBrush.TintColor = FSlateColor(FLinearColor(0.888f, 0.584f, 0.072f));
	PipBrush.ImageSize = FVector2D(22.f, 22.f);
}

void UBossHealthBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너 미리보기 및 보스 연결 전 기본 상태
	SetBossName(DefaultBossName);
	SetShowHealthText(bShowHealthText);
	SetPhaseMarkers(DefaultPhaseMarkers);
}

void UBossHealthBarWidget::InitializeBossHealthBar(ABossCharacter* Boss)
{
	// 재도전이나 다른 보스로 연결될 때 이전 구독 정리
	UnbindBoss();

	if (!IsValid(Boss))
		return;

	ObservedBoss = Boss;

	// 이름: 보스에 지정된 이름 우선, 비어 있으면 기본 이름
	const FText BossName = Boss->GetBossDisplayName();
	SetBossName(BossName.IsEmpty() ? DefaultBossName : BossName);

	// 페이즈 경계: 보스의 페이즈 컴포넌트가 실제로 사용하는 PhaseConfig 기준
	// 경계를 얻지 못하면(설정 없음 등) 위젯 기본값 사용
	TArray<float> Markers;
	if (bUsePhaseConfigMarkers)
	{
		if (const UBossPhaseComponent* PhaseComponent = Boss->GetPhaseComponent())
		{
			Markers = BuildMarkersFromPhaseConfig(PhaseComponent->GetConfig());
		}
	}
	SetPhaseMarkers(Markers.IsEmpty() ? DefaultPhaseMarkers : Markers);

	UEnemyAttributeSet* Stat = Boss->GetAttributeSet();
	if (!Stat)
		return;

	ObservedStats = Stat;
	Stat->OnHealthChange.AddUObject(this, &UBossHealthBarWidget::HandleHealthChanged);

	// 구독 전에 이미 설정된 체력도 즉시 표시 (잔상 없이)
	const float MaxHealth = Stat->GetMaxHealth();
	LastRatio = FMath::IsNearlyZero(MaxHealth) ? 0.f : FMath::Clamp(Stat->GetHealth() / MaxHealth, 0.f, 1.f);
	LastPassedCount = CountPassedMarkers(LastRatio);
	if (HealthBar)
	{
		HealthBar->UpdateResourceBar(Stat->GetHealth(), MaxHealth);
		HealthBar->ResetTrail();
	}
	RefreshPips();
}

void UBossHealthBarWidget::UnbindBoss()
{
	if (UEnemyAttributeSet* Stat = ObservedStats.Get())
	{
		Stat->OnHealthChange.RemoveAll(this);
	}

	ObservedStats.Reset();
	ObservedBoss.Reset();
	RefreshLockDisplay();
}

void UBossHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// BP Tick(연출)이 먼저 돌고, 잠금 표시는 C++이 마지막에 덮어씀
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshLockDisplay(InDeltaTime);
}

EBossHealthBarLockMode UBossHealthBarWidget::GetLockMode() const
{
	const ABossCharacter* Boss = ObservedBoss.Get();
	if (!IsValid(Boss) || LastRatio <= 0.f)
		return EBossHealthBarLockMode::None;

	const UBossPhaseComponent* PhaseComponent = Boss->GetPhaseComponent();

	// 전환 요청부터 다음 페이즈 적용까지 (컷 종료 후 플레이어 도착 대기는 따로 안내)
	if (Boss->IsPhaseTransitionInProgress())
	{
		return (PhaseComponent && PhaseComponent->IsWaitingForPlayer())
			? EBossHealthBarLockMode::FollowBoss
			: EBossHealthBarLockMode::Transition;
	}

	// 배리어 페이즈 진행 중
	if (PhaseComponent && PhaseComponent->IsPhaseActive())
	{
		const FBossPhaseDefinition* Phase = PhaseComponent->GetCurrentPhase();
		if (Phase && Phase->Kind == EBossPhaseKind::Barrier)
			return EBossHealthBarLockMode::Barrier;
	}

	return EBossHealthBarLockMode::None;
}

void UBossHealthBarWidget::RefreshLockDisplay(float DeltaTime)
{
	const EBossHealthBarLockMode Mode = GetLockMode();
	const bool bLocked = Mode != EBossHealthBarLockMode::None;
	const float LockOpacity = bLocked ? 1.f : 0.f;

	// 잠금 중에만 시간 누적: 잠금이 시작될 때 항상 밝은 상태에서 깜빡임 시작
	LockBlinkTime = bLocked ? LockBlinkTime + DeltaTime : 0.f;

	if (LockShade)
		LockShade->SetRenderOpacity(LockOpacity);
	if (LockStripes)
		LockStripes->SetRenderOpacity(LockOpacity);
	if (LockText)
	{
		float TextOpacity = LockOpacity;
		if (bLocked && LockTextBlinkRate > 0.f)
		{
			// cos 파형: 1(밝음) → MinOpacity → 1
			const float Wave = 0.5f + 0.5f * FMath::Cos(LockBlinkTime * LockTextBlinkRate * UE_TWO_PI);
			TextOpacity = FMath::Lerp(LockTextMinOpacity, 1.f, Wave);
		}
		LockText->SetRenderOpacity(TextOpacity);
		if (Mode != EBossHealthBarLockMode::None)
		{
			switch (Mode)
			{
			case EBossHealthBarLockMode::Barrier:		LockText->SetText(BarrierLockMessage); break;
			case EBossHealthBarLockMode::FollowBoss:	LockText->SetText(FollowBossLockMessage); break;
			default:									LockText->SetText(TransitionLockMessage); break;
			}
		}
	}
}

void UBossHealthBarWidget::NativeDestruct()
{
	UnbindBoss();
	Super::NativeDestruct();
}

void UBossHealthBarWidget::SetShowHealthText(bool bShow)
{
	bShowHealthText = bShow;
	if (HealthBar)
	{
		HealthBar->SetShowValueText(bShowHealthText);
	}
}

void UBossHealthBarWidget::SetBossName(const FText& InName)
{
	if (NameText)
	{
		NameText->SetText(InName);
	}
}

void UBossHealthBarWidget::SetPhaseMarkers(const TArray<float>& InRatios)
{
	CurrentMarkers = NormalizeMarkers(InRatios);
	LastPassedCount = CountPassedMarkers(LastRatio);
	RebuildMarkers();
	RebuildPips();
}

void UBossHealthBarWidget::ClearPhaseMarkers()
{
	SetPhaseMarkers(TArray<float>());
}

TArray<float> UBossHealthBarWidget::BuildMarkersFromPhaseConfig(const UBossPhaseConfigData* Config)
{
	TArray<float> Result;
	if (!Config)
		return Result;

	// 전투 페이즈의 체력 하한 = 다음 페이즈로 넘어가는 지점 (0은 사망 지점이므로 제외)
	for (const FBossPhaseDefinition& Phase : Config->Phases)
	{
		if (Phase.Kind == EBossPhaseKind::Combat && Phase.HealthFloorRatio > 0.f)
		{
			Result.Add(Phase.HealthFloorRatio);
		}
	}
	return Result;
}

void UBossHealthBarWidget::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	if (HealthBar)
	{
		HealthBar->UpdateResourceBar(NewHealth, MaxHealth);
	}

	const float NewRatio = FMath::IsNearlyZero(MaxHealth) ? 0.f : FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f);
	const float OldRatio = LastRatio;
	LastRatio = NewRatio;

	// 체력이 0이 되거나 0에서 회복될 때도 아이콘 갱신
	if ((NewRatio <= 0.f) != (OldRatio <= 0.f))
	{
		RefreshPips();
	}

	const int32 PassedCount = CountPassedMarkers(NewRatio);
	if (PassedCount != LastPassedCount)
	{
		const bool bPassedNew = PassedCount > LastPassedCount;
		LastPassedCount = PassedCount;
		RefreshPips();
		if (bPassedNew)
		{
			OnPhaseMarkerPassed(PassedCount);
		}
	}

	if (!FMath::IsNearlyEqual(NewRatio, OldRatio))
	{
		OnHealthRatioChanged(NewRatio, OldRatio);
	}
}

void UBossHealthBarWidget::RebuildMarkers()
{
	if (!MarkerLayer || !WidgetTree)
		return;

	MarkerLayer->ClearChildren();

	for (const float Ratio : CurrentMarkers)
	{
		UImage* Marker = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Marker->SetBrush(MarkerBrush);
		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);

		// 가로는 비율 위치에 고정, 세로는 바 높이에 맞춰 늘림
		if (UCanvasPanelSlot* CanvasSlot = MarkerLayer->AddChildToCanvas(Marker))
		{
			CanvasSlot->SetAnchors(FAnchors(Ratio, 0.f, Ratio, 1.f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.f));
			CanvasSlot->SetOffsets(FMargin(0.f, 0.f, MarkerWidth, 0.f));	// Left=X, Top, Right=Width, Bottom
		}
	}
}

void UBossHealthBarWidget::RebuildPips()
{
	if (!PhasePipBox || !WidgetTree)
		return;

	PhasePipBox->ClearChildren();
	PipImages.Reset();

	// 경계가 없으면 단일 페이즈: 아이콘 숨김
	if (CurrentMarkers.IsEmpty())
		return;

	const int32 PipCount = CurrentMarkers.Num() + 1;
	for (int32 Index = 0; Index < PipCount; ++Index)
	{
		UImage* Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Pip->SetBrush(PipBrush);
		Pip->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UHorizontalBoxSlot* BoxSlot = PhasePipBox->AddChildToHorizontalBox(Pip))
		{
			BoxSlot->SetPadding(FMargin(Index == 0 ? 0.f : PipSpacing, 0.f, 0.f, 0.f));
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}
		PipImages.Add(Pip);
	}

	RefreshPips();
}

void UBossHealthBarWidget::RefreshPips()
{
	// 지나간 페이즈만큼 왼쪽부터 꺼짐, 체력 0이면 마지막 페이즈까지 모두 꺼짐
	const int32 OffCount = (LastRatio <= 0.f) ? PipImages.Num() : LastPassedCount;
	for (int32 Index = 0; Index < PipImages.Num(); ++Index)
	{
		if (UImage* Pip = PipImages[Index])
		{
			Pip->SetColorAndOpacity(Index < OffCount ? PipInactiveColor : PipActiveColor);
		}
	}
}

int32 UBossHealthBarWidget::CountPassedMarkers(float Ratio) const
{
	// 페이즈 하한에 정확히 멈추는 경우도 통과로 처리
	int32 Count = 0;
	for (const float Marker : CurrentMarkers)
	{
		if (Ratio <= Marker + KINDA_SMALL_NUMBER)
		{
			++Count;
		}
	}
	return Count;
}

TArray<float> UBossHealthBarWidget::NormalizeMarkers(const TArray<float>& InRatios)
{
	// 0~1 사이 유효값만 남기고 중복 제거 후 내림차순 정렬
	TArray<float> Result;
	for (const float Ratio : InRatios)
	{
		if (FMath::IsFinite(Ratio) && Ratio > 0.f && Ratio < 1.f)
		{
			const bool bDuplicate = Result.ContainsByPredicate([Ratio](float Existing)
			{
				return FMath::IsNearlyEqual(Existing, Ratio, KINDA_SMALL_NUMBER);
			});
			if (!bDuplicate)
			{
				Result.Add(Ratio);
			}
		}
	}
	Result.Sort(TGreater<float>());
	return Result;
}
