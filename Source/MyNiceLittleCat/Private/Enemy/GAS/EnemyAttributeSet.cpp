// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/GAS/EnemyAttributeSet.h"

UEnemyAttributeSet::UEnemyAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	
	InitAttackPower(20.f);

	InitDamage(0.f);
}

void UEnemyAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(0, NewValue);
	}
}

void UEnemyAttributeSet::PostAttributeChange(const FGameplayAttribute & Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		//UE_LOG(LogTemp, Log, TEXT("[UEnemyAttributeSet] Health Changed: (%.1f) -> (%.1f)"), OldValue, NewValue);

		OnHealthChange.Broadcast(NewValue, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		//UE_LOG(LogTemp, Log, TEXT("[UEnemyAttributeSet] Max Health Changed: (%.1f) -> (%.1f)"), OldValue, NewValue);

		AdjustAttributeForMaxChange(OldValue, NewValue, GetHealthAttribute());	// 변경된 MaxHealth값 기준으로 Health값 보정
		OnHealthChange.Broadcast(GetHealth(), NewValue);
	}
}

void UEnemyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData & Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.f);	// Damage 즉시 비우기

		//UE_LOG(LogTemp, Log, TEXT("[UEnemyAttributeSet] Damage Effected: %.1f"), LocalDamage);

		if (LocalDamage > 0)
		{
			float FinalDamage = LocalDamage;	// 방어력이 없으므로 Damage 그대로 받음 (필요 시 수식 추가)
			FinalDamage = FMath::Max(1.f, FinalDamage);	// 맞으면 최소 1
			
			const float NewHealth = FMath::Clamp(GetHealth() - FinalDamage, 0.f, GetMaxHealth());
			SetHealth(NewHealth);
		}
	}
}

void UEnemyAttributeSet::AdjustAttributeForMaxChange(float InOldValue, float InNewValue, const FGameplayAttribute & AffectedAttributeProperty)
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
		return;

	float Delta = InNewValue - InOldValue;

	if (Delta > 0)
	{
		ASC->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, Delta);
	}
	else
	{
		bool bFound = false;
		const float Current = ASC->GetGameplayAttributeValue(AffectedAttributeProperty, bFound);

		if (bFound && InNewValue < Current)
		{
			Delta = InNewValue - Current;
			ASC->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, Delta);
		}
	}
	
}
