// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/*
* 작성자: 전선현
* 작성 일자: 2026.10.15.
* 몬스터를 구현하며 사용되는 enum, struct를 포함하는 일반 헤더 파일
*/

#include "CoreMinimal.h"
#include "EnemyTypes.generated.h"

UENUM(BlueprintType)
enum class EEnemyLifeState : uint8
{
	Alive,
	Dying,
	Dead
};

// Enemy 사망 정보 (현재 사용 안 함)
//USTRUCT(BlueprintType)
//struct FEnemyDeathContext
//{
//	GENERATED_BODY()
//
//	UPROPERTY(BlueprintReadOnly)
//	TWeakObjectPtr<AActor> InstigatorActor;
//
//	UPROPERTY(BlueprintReadOnly)
//	TWeakObjectPtr<AActor> DamageCause;
//};
