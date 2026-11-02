// Fill out your copyright notice in the Description page of Project Settings.


#include "WIdgets/Enemy/BossHealthBarWidget.h"

#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Widgets/ResourceBarWidget.h"

void UBossHealthBarWidget::InitializeBossHealthBar(ABossCharacter* Boss)
{
	// 재도전이나 다른 보스로 연결될 때 이전 구독 정리
	UnbindBoss();

	UEnemyAttributeSet* Stat = IsValid(Boss) ? Boss->GetAttributeSet() : nullptr;
	if (!Stat)
		return;

	ObservedStats = Stat;

	Stat->OnHealthChange.AddUObject(HealthBar, &UResourceBarWidget::UpdateResourceBar);
	//Stat->OnMaxHealthChange.AddUObject(HealthBar, &UResourceBarWidget::UpdateResourceBar);

	// 구독 전에 이미 설정된 체력도 즉시 표시
	HealthBar->UpdateResourceBar(Stat->GetHealth(), Stat->GetMaxHealth());
}

void UBossHealthBarWidget::UnbindBoss()
{
	if (UEnemyAttributeSet* Stat = ObservedStats.Get())
	{
		Stat->OnHealthChange.RemoveAll(HealthBar);
	}

	ObservedStats.Reset();
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
