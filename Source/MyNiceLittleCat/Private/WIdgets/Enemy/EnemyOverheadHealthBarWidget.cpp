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
	if (AEnemyCharacter* Character = Cast<AEnemyCharacter>(InActor))
	{
		if (UEnemyAttributeSet* Stat = Character->GetAttributeSet())	// Stat Attribute Set 사용하는지 확인용
		{
			UE_LOG(LogTemp, Log, TEXT("Character has Stat"));
			Stat->OnHealthChange.AddUObject(HealthBar, &UResourceBarWidget::UpdateResourceBar);
			Stat->OnMaxHealthChange.AddUObject(HealthBar, &UResourceBarWidget::UpdateResourceBar);

			HealthBar->UpdateResourceBar(Stat->GetHealth(), Stat->GetMaxHealth());
		}
	}
}
