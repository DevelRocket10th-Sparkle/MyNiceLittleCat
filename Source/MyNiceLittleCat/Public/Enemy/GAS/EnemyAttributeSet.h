// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.14.
* 수정 일자: 2026.11.17. (미사용 OnMaxHealthChange 삭제, 주석 정리)
* Enemy/보스에 연결된 AttributeSet
* - 각 Attribute 관리
*/

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"

#include "EnemyAttributeSet.generated.h"

// Health 변경 알림: (새 Health, MaxHealth)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStatChange, float /*NewHealth*/, float /*MaxHealth*/);

UCLASS()
class MYNICELITTLECAT_API UEnemyAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public :
	
	UEnemyAttributeSet();

	// 적용 전 Clamping 수행
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// 적용 후 값의 변화 감지나 UI에 반영하기 위해 사용
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	// 이펙트가 적용된 후에 실행되는 함수 (Damage용)
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

protected :

	void AdjustAttributeForMaxChange(
		float InOldValue,
		float InNewValue,
		const FGameplayAttribute& AffectedAttributeProperty
	);

public :

	/* Stat 변경 델리게이트 */

	// Health 또는 MaxHealth가 바뀌면 (Health, MaxHealth)로 방송: 체력바, 사망 판정이 구독
	FOnStatChange OnHealthChange;

	/* Attribute 타입 */
	
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UEnemyAttributeSet, Health);
	
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UEnemyAttributeSet, MaxHealth);
	
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS_BASIC(UEnemyAttributeSet, AttackPower);

	/* Meta Attribute */

	UPROPERTY(BlueprintReadOnly, Category = "Meta Attribute")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS_BASIC(UEnemyAttributeSet, Damage);
	
};
