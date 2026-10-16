// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/*
* 작성자: 전선현
* 작성 일자: 2026.10.15.
* 몬스터의 능력치(Attributes) 
*/

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyConfigData.generated.h"

class UBehaviorTree;

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UEnemyConfigData : public UDataAsset
{
	GENERATED_BODY()

public :

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float AttackPower = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 150.f;
	
};
