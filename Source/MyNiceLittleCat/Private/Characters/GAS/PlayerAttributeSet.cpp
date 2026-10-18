// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/GAS/PlayerAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectExtension.h"

UPlayerAttributeSet::UPlayerAttributeSet()
{
    InitHealth(10.0f);
    InitMaxHealth(10.0f);
    InitAttackPower(20.0f);
    InitDamage(0.0f);
}

void UPlayerAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetMaxHealthAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
    else if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
    else if (Attribute == GetAttackPowerAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
}

void UPlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
    {
        SetMaxHealth(FMath::Max(0.0f, GetMaxHealth()));
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (Data.EvaluatedData.Attribute == GetDamageAttribute())
    {
        const float LocalDamage = GetDamage();
        SetDamage(0.0f);

        if (LocalDamage > 0.0f)
        {
            const float NewHealth = FMath::Clamp(
                GetHealth() - LocalDamage,
                0.0f,
                GetMaxHealth()
            );

            SetHealth(NewHealth);
        }
    }
}