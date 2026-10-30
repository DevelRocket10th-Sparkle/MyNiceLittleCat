// Fill out your copyright notice in the Description page of Project Settings.


#include "WIdgets/Enemy/EnemyOverheadHealthBarWidget.h"

#include "WIdgets/ResourceBarWidget.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyAttributeSet.h"

void UEnemyOverheadHealthBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetShowHealthText(bShowHealthText);
}

void UEnemyOverheadHealthBarWidget::NativeDestruct()
{
	UnbindOverheadHealthBar();
	Super::NativeDestruct();
}

void UEnemyOverheadHealthBarWidget::UnbindOverheadHealthBar()
{
	if (UEnemyAttributeSet* Stat = ObservedStats.Get())
	{
		if (IsValid(HealthBar))
		{
			// 실제 구독자는 내부 ResourceBar
			Stat->OnHealthChange.RemoveAll(HealthBar);
			//Stat->OnMaxHealthChange.RemoveAll(HealthBar);
		}
	}

	ObservedStats.Reset();
}


void UEnemyOverheadHealthBarWidget::SetShowHealthText(bool bShow)
{
	bShowHealthText = bShow;
	if (HealthBar)
	{
		HealthBar->SetShowValueText(bShowHealthText);
	}
}

void UEnemyOverheadHealthBarWidget::InitializeOverheadHealthBar(AActor* InActor)
{
	// 재초기화 시 중복 구독 방지
	UnbindOverheadHealthBar();

	if (AEnemyCharacter* Character = Cast<AEnemyCharacter>(InActor))
	{
		if (UEnemyAttributeSet* Stat = Character->GetAttributeSet())	// Stat Attribute Set 사용하는지 확인용
		{
			Stat->OnHealthChange.AddUObject(HealthBar, &UResourceBarWidget::UpdateResourceBar);
			//Stat->OnMaxHealthChange.AddUObject(HealthBar, &UResourceBarWidget::UpdateResourceBar);

			HealthBar->UpdateResourceBar(Stat->GetHealth(), Stat->GetMaxHealth());
		}
	}
}
