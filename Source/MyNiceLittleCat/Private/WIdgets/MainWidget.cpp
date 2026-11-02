#include "WIdgets/MainWidget.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

#include "Characters/PlayerCharacter.h"
#include "Characters/GAS/PlayerAttributeSet.h"
#include "Characters/PlayerComponents/LockOnComponent.h"

#include "Enemy/Boss/BossCharacter.h"
#include "Widgets/Enemy/BossHealthBarWidget.h"

void UMainWidget::InitForPlayer(APlayerCharacter* InPlayer)
{
	UnbindFromPlayer();
	Player = InPlayer;
	BindToPlayer();
	RefreshOrbs();
}

void UMainWidget::ShowBossHealthBar(ABossCharacter* Boss)
{
	if (!BossHealthBar || !IsValid(Boss))
		return;
	
	BossHealthBar->InitializeBossHealthBar(Boss);	// 보스 구독 및 체력 바 현재 보스전에 맞게 갱신
	BossHealthBar->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMainWidget::HideBossHealthBar()
{
	// 보스 체력 바를 숨기는 시점에는 보스가 이미 죽고 없을 수도 있음
	if (!BossHealthBar)
		return;

	BossHealthBar->UnbindBoss();	// 보스 체력 변경 델리게이트 구독 해제
	BossHealthBar->SetVisibility(ESlateVisibility::Collapsed);
}

void UMainWidget::BindToPlayer()
{
	if (!Player.IsValid()) return;

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (!ASC) return;

	PlayerASC = ASC;

	// 체력/최대 체력이 바뀔 때마다 오브 갱신
	HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UPlayerAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UMainWidget::OnHealthAttributeChanged);
	MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UPlayerAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UMainWidget::OnHealthAttributeChanged);
}

void UMainWidget::UnbindFromPlayer()
{
	if (UAbilitySystemComponent* ASC = PlayerASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UPlayerAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UPlayerAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
	}

	HealthHandle.Reset();
	MaxHealthHandle.Reset();
	PlayerASC.Reset();
	Player.Reset();
}

void UMainWidget::NativeDestruct()
{
	UnbindFromPlayer();
	Super::NativeDestruct();
}

void UMainWidget::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshOrbs();
}

void UMainWidget::RefreshOrbs()
{
	UAbilitySystemComponent* ASC = PlayerASC.Get();
	if (!OrbBox || !ASC) return;

	// 체력 1 = 목숨 1개
	const int32 MaxLives = FMath::Max(0, FMath::CeilToInt(
		ASC->GetNumericAttribute(UPlayerAttributeSet::GetMaxHealthAttribute())));
	const int32 Lives = FMath::Clamp(FMath::CeilToInt(
		ASC->GetNumericAttribute(UPlayerAttributeSet::GetHealthAttribute())), 0, MaxLives);

	// 최대 목숨 수가 바뀌었을 때만 오브를 다시 생성
	if (Orbs.Num() != MaxLives)
	{
		OrbBox->ClearChildren();
		Orbs.Reset();

		for (int32 i = 0; i < MaxLives; ++i)
		{
			UImage* Orb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Orb->SetBrushFromTexture(OrbTexture);
			FSlateBrush Brush = Orb->GetBrush();
			Brush.ImageSize = OrbSize;
			Orb->SetBrush(Brush);
			OrbBox->AddChildToHorizontalBox(Orb);
			Orbs.Add(Orb);
		}
	}

	// 남은 목숨은 원래 색, 잃은 목숨은 반투명
	for (int32 i = 0; i < Orbs.Num(); ++i)
	{
		Orbs[i]->SetColorAndOpacity(i < Lives ? FLinearColor::White : EmptyOrbColor);
	}
}

void UMainWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (BossHealthBar)
	{
		BossHealthBar->SetShowHealthText(false);
		BossHealthBar->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!LockOnMarker) return;

	const ULockOnComponent* LockOn = Player.IsValid() ? Player->FindComponentByClass<ULockOnComponent>() : nullptr;
	AActor* Target = LockOn ? LockOn->GetLockedTarget() : nullptr;

	FVector2D ScreenPos;
	bool bVisible = IsValid(Target);

	if (bVisible)
	{
		// 대상 머리 위 위치를 화면 좌표로 변환
		FVector Origin;
		FVector Extent;
		Target->GetActorBounds(true, Origin, Extent);
		const FVector MarkerWorld = Origin + FVector(0.f, 0.f, Extent.Z + LockOnMarkerHeightOffset);

		bVisible = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			GetOwningPlayer(), MarkerWorld, ScreenPos, false);
	}

	if (!bVisible)
	{
		LockOnMarker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LockOnMarker->Slot))
	{
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));	// 마커 아래쪽 중앙이 대상 위치
		CanvasSlot->SetPosition(ScreenPos);
	}

	LockOnMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
}
