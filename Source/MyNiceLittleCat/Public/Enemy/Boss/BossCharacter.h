// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 보스 전용 기능 구현을 위한 EnemyCharacter 상속 클래스
*/

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyCharacter.h"
#include "BossCharacter.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API ABossCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public :

	//virtual float CalculateHealthAfterDamage(float CurrentHealth, float MaxHealth, float Damage) const override;


	
};
