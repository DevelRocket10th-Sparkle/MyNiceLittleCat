// 보스 패턴 "폭탄 낙하"의 실행 설정입니다.
// 플레이어의 예측 위치(착지 시점에 있을 곳) 앞쪽으로 폭탄 몬스터를 하늘에서 떨어뜨립니다.
#pragma once

#include "CoreMinimal.h"
#include "Enemy/Datas/EnemyPatternExecutionSettings.h"
#include "BombDropPatternSettings.generated.h"

class ABombEnemy;
class UNiagaraSystem;
class USoundBase;

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class MYNICELITTLECAT_API UBombDropPatternSettings : public UEnemyPatternExecutionSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop")
	TSubclassOf<ABombEnemy> BombClass;

	// 한 번 실행에 떨어뜨릴 폭탄 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop", meta = (ClampMin = "1", ClampMax = "16"))
	int32 BombCount = 3;

	// 폭탄 사이 낙하 간격 (매번 그 시점의 플레이어 위치로 다시 조준)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop", meta = (ClampMin = "0", Units = "s"))
	float DropInterval = 0.5f;

	// 살아 있는 폭탄이 이 수 이상이면 더 떨어뜨리지 않음 (0 = 제한 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop", meta = (ClampMin = "0"))
	int32 MaxAliveBombs = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop")
	bool bRandomizePolarity = true;

	/* 조준 */

	// 1 = 착지 순간 플레이어가 있을 위치로 예측, 0 = 현재 위치 (0.5 정도면 살짝 덜 정확해서 피할 여지가 생김)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop|Aim", meta = (ClampMin = "0", ClampMax = "2"))
	float PredictionScale = 1.f;

	// 예측 위치에서 진행 방향(멈춰 있으면 바라보는 방향)으로 이만큼 앞에 떨어뜨림
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop|Aim", meta = (ClampMin = "0", Units = "cm"))
	float ForwardOffset = 250.f;

	// 첫 폭탄 이후는 조준점에서 이 반경 안 랜덤 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop|Aim", meta = (ClampMin = "0", Units = "cm"))
	float ScatterRadius = 150.f;

	/* 낙하 */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop|Fall", meta = (ClampMin = "100", Units = "cm"))
	float DropHeight = 1500.f;

	// 낙하 시작 속도 (아래 방향)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop|Fall", meta = (ClampMin = "0", Units = "cm/s"))
	float DropSpeed = 1000.f;

	/* 연출 */

	// 낙하 지점 경고 (예: BP_V1_AttackWarningDecal). 착지 순간까지 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop|FX")
	TSubclassOf<AActor> WarningClass;

	// 폭탄이 나타나는 하늘 위치에 터뜨릴 이펙트 (선택)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop|FX")
	TObjectPtr<UNiagaraSystem> SpawnEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BombDrop|FX")
	TObjectPtr<USoundBase> DropSound;

	bool IsValidConfiguration() const;
	float GetExecutionSeconds() const;

	// DropHeight에서 DropSpeed로 떨어질 때 땅에 닿기까지 시간 (GravityZ는 음수)
	float GetFallSeconds(float GravityZ) const;
};
