// 작성자: 강진구
// 작성일자: 2026-10-15
// 
// 플레이어 GAS 어트리뷰트 셋 입니다.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PlayerAttributeSet.generated.h"

#define PLAYER_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class MYNICELITTLECAT_API UPlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
    UPlayerAttributeSet();

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

public:
    UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData Health;
    PLAYER_ATTRIBUTE_ACCESSORS(UPlayerAttributeSet, Health)

       UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData MaxHealth;
    PLAYER_ATTRIBUTE_ACCESSORS(UPlayerAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData AttackPower;
    PLAYER_ATTRIBUTE_ACCESSORS(UPlayerAttributeSet, AttackPower)

    UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData Damage;
    PLAYER_ATTRIBUTE_ACCESSORS(UPlayerAttributeSet, Damage)

};
