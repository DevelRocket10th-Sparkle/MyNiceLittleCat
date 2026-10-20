// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 패턴 정보 설정 및 보관용 데이터 에셋
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyPatternSetData.generated.h"

class UGameplayAbility;
class UAnimMontage;

UENUM(BlueprintType)
enum class EEnemyPatternResult : uint8
{
	Invalid,
	Running,
	Completed,
	Cancelled,
	Failed
};

USTRUCT(BlueprintType)
struct FEnemyPatternDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName PatternId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName MontageSection;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float MinRange = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float MaxRange = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float MaxHeightDifference = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bRequiresLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bFaceTargetOnStart = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bLockRotation = true;

	// GA 종료 누락을 감지하기 위한 제한 시간
	// 공격 시간이나 쿨다운이 아님
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float MaxExecutionSeconds = 15.f;
};

/**
 * 
 */
UCLASS(BlueprintType)
class MYNICELITTLECAT_API UEnemyPatternSetData : public UDataAsset
{
	GENERATED_BODY()

public :

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Patterns")
	TArray<FEnemyPatternDefinition> Patterns;
};
