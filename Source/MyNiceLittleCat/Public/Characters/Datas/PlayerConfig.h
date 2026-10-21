// 작성자: 강진구
// 작성일자: 2026-10-20
//
// 플레이어 능력치 데이터 에셋

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlayerConfig.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UPlayerConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float AttackPower = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 600.f;
};
