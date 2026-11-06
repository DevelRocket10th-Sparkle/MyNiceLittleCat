// 보스 패턴 "포탑 낙하"의 실행 설정입니다.
// 플레이어 주위 원형으로 포탑을 하늘에서 떨어뜨리고, ActiveSeconds 뒤 전부 파괴합니다. Cycles만큼 반복합니다.
#pragma once

#include "CoreMinimal.h"
#include "Enemy/Datas/EnemyPatternExecutionSettings.h"
#include "TurretRainPatternSettings.generated.h"

class ATurret;
class UNiagaraSystem;
class USoundBase;
class USkeletalMesh;
class UAnimSequence;

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class MYNICELITTLECAT_API UTurretRainPatternSettings : public UEnemyPatternExecutionSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain")
	TSubclassOf<ATurret> TurretClass;

	// 한 번에 떨어뜨릴 포탑 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain", meta = (ClampMin = "1", ClampMax = "16"))
	int32 TurretCount = 5;

	// 플레이어 중심 원 반지름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain", meta = (ClampMin = "100", Units = "cm"))
	float Radius = 700.f;

	// 땅에서 이만큼 위에서 떨어짐
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain", meta = (ClampMin = "100", Units = "cm"))
	float DropHeight = 1500.f;

	// 낙하 시작 속도 (빠르게 꽂히게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain", meta = (ClampMin = "0", Units = "cm/s"))
	float DropSpeed = 1500.f;

	// 포탑 사이 낙하 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain", meta = (ClampMin = "0", Units = "s"))
	float DropInterval = 0.15f;

	// 마지막 포탑이 떨어진 뒤 파괴까지 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain", meta = (ClampMin = "0.5", Units = "s"))
	float ActiveSeconds = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain")
	bool bRandomizePolarity = true;

	// true: 포탑을 다 떨어뜨리면 패턴을 바로 끝냄 → 보스는 계속 추격·근접 공격하고, 포탑은 ActiveSeconds 뒤 스스로 파괴
	//       이전 포탑이 남아 있는 동안 이 패턴은 실행되지 않음 (Cycles 무시)
	// false: 포탑이 살아 있는 동안 패턴이 계속 실행됨 (보스는 다른 공격 못 함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain")
	bool bRunInBackground = true;

	// 떨어뜨리기 → 파괴를 몇 번 반복할지 (bRunInBackground가 false일 때만)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain", meta = (ClampMin = "1", ClampMax = "10"))
	int32 Cycles = 2;

	// 파괴 후 다음 낙하까지 대기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain", meta = (ClampMin = "0", Units = "s"))
	float CycleGapSeconds = 1.f;

	// true면 플레이어가 펫으로 들고 있는 포탑은 파괴하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretRain")
	bool bSpareCarriedTurrets = false;

	// 낙하 지점 경고 (예: BP_V1_AttackWarningDecal). 착지 무렵 사라지도록 WarningSeconds 수명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warning")
	TSubclassOf<AActor> WarningClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warning", meta = (ClampMin = "0", Units = "s"))
	float WarningSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TObjectPtr<UNiagaraSystem> DestroyEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TObjectPtr<USoundBase> DestroySound;

	// 터렛 파괴 시 재생할 애니메이션 폭발 (SK_TurretExplosion + 애님)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TObjectPtr<USkeletalMesh> DestroyMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TObjectPtr<UAnimSequence> DestroyAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (ClampMin = "0.01"))
	float DestroyMeshScale = 1.f;

	bool IsValidConfiguration() const;
	float GetExecutionSeconds() const;
};
