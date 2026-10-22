/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 자성 코인 낙하 패턴 컴포넌트입니다.
/// 보스(또는 패턴 실행 액터)에 붙이고 StartPattern / StopPattern으로 켜고 끕니다.
/// 스폰 영역(SpawnCenterActor 또는 소유 액터 위치 기준 SpawnAreaExtent) 안 랜덤 XY, SpawnHeight 위에서
/// 코인을 스폰하고 StartFall(극성)을 호출합니다. 극성은 랜덤 또는 N/S 교대입니다.
/// 스폰 방식 (SpawnMode):
/// - Interval:     SpawnInterval마다 한 웨이브(CoinsPerWave개)
/// - WhenDepleted: 남은 코인(낙하 중 + 바닥)이 RespawnThreshold 이하가 된 뒤 RespawnDelay가 지나면 한 웨이브
/// 두 방식 모두 남은 코인이 MaxActiveCoins 이상이면 더 스폰하지 않습니다.
/// 보스전에서는 UBossGimmickComponent가 배리어 페이즈 진입 시 StartPattern, 페이즈 종료 시 StopPattern을 호출합니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gimmicks/Magnetic/MagneticTypes.h"
#include "MagneticCoinPatternComponent.generated.h"

class AMagneticCoin;
class UMagneticCoinPatternComponent;

UENUM(BlueprintType)
enum class EMagneticCoinSpawnMode : uint8
{
	// 일정 주기마다 웨이브
	Interval		UMETA(DisplayName = "Interval"),
	// 남은 코인이 기준 이하가 되면 웨이브
	WhenDepleted	UMETA(DisplayName = "When Depleted"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPatternCoinSpawned, UMagneticCoinPatternComponent*, Pattern, AMagneticCoin*, Coin);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPatternCoinFallHit, UMagneticCoinPatternComponent*, Pattern, AMagneticCoin*, Coin, AActor*, HitActor);

UCLASS(ClassGroup = (Gimmick), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UMagneticCoinPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMagneticCoinPatternComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 패턴 시작 (bSpawnOnStart면 바로 한 웨이브)
	UFUNCTION(BlueprintCallable, Category = "MagneticCoinPattern")
	void StartPattern();

	// 패턴 정지 (bClearCoins면 남은 코인도 제거)
	UFUNCTION(BlueprintCallable, Category = "MagneticCoinPattern")
	void StopPattern(bool bClearCoins);

	UFUNCTION(BlueprintPure, Category = "MagneticCoinPattern")
	bool IsPatternActive() const { return bPatternActive; }

	// 코인 CoinsPerWave개 낙하 (MaxActiveCoins까지)
	UFUNCTION(BlueprintCallable, Category = "MagneticCoinPattern")
	void SpawnWave();

	// 이 패턴이 스폰한 코인 전부 제거
	UFUNCTION(BlueprintCallable, Category = "MagneticCoinPattern")
	void ClearCoins();

	// 남은 코인 수 (bIncludeFalling이 false면 예고·낙하 중인 코인 제외: 착지·잡힘·던져짐만)
	UFUNCTION(BlueprintPure, Category = "MagneticCoinPattern")
	int32 GetActiveCoinCount(bool bIncludeFalling = true) const;

	UFUNCTION(BlueprintPure, Category = "MagneticCoinPattern")
	FVector GetSpawnCenter() const;

	// 스폰 영역 중심 액터 지정 (런타임 생성 보스처럼 레벨에서 지정할 수 없을 때, nullptr이면 소유 액터 위치)
	UFUNCTION(BlueprintCallable, Category = "MagneticCoinPattern")
	void SetSpawnCenterActor(AActor* InCenterActor) { SpawnCenterActor = InCenterActor; }

	UFUNCTION(BlueprintPure, Category = "MagneticCoinPattern")
	FVector GetSpawnAreaExtent() const { return SpawnAreaExtent; }

	int32 GetMaxActiveCoins() const { return MaxActiveCoins; }

public:
	UPROPERTY(BlueprintAssignable, Category = "MagneticCoinPattern")
	FOnPatternCoinSpawned OnCoinSpawned;

	// 낙하 중인 코인이 플레이어 등에게 낙하 피해를 줬을 때
	UPROPERTY(BlueprintAssignable, Category = "MagneticCoinPattern")
	FOnPatternCoinFallHit OnCoinFallHit;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	AMagneticCoin* SpawnCoin();

	EMagneticPolarity PickPolarity();

	void RemoveInvalidCoins();

	UFUNCTION()
	void HandleCoinFallHit(AMagneticCoin* Coin, AActor* HitActor);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern")
	TSubclassOf<AMagneticCoin> CoinClass;

	// true면 BeginPlay에서 바로 StartPattern (테스트용, 보스전에선 보통 false)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern")
	bool bAutoStart = false;

	// true면 StartPattern 직후 첫 웨이브
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern")
	bool bSpawnOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Spawn")
	EMagneticCoinSpawnMode SpawnMode = EMagneticCoinSpawnMode::Interval;

	// Interval: 웨이브 간격 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Spawn", meta = (ClampMin = "0.1", EditCondition = "SpawnMode == EMagneticCoinSpawnMode::Interval"))
	float SpawnInterval = 3.f;

	// WhenDepleted: 남은 코인(낙하 중 포함)이 이 수 이하가 되면 재생성
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Spawn", meta = (ClampMin = "0", EditCondition = "SpawnMode == EMagneticCoinSpawnMode::WhenDepleted"))
	int32 RespawnThreshold = 0;

	// WhenDepleted: 기준 이하가 된 뒤 재생성까지 대기 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Spawn", meta = (ClampMin = "0.0", EditCondition = "SpawnMode == EMagneticCoinSpawnMode::WhenDepleted"))
	float RespawnDelay = 1.f;

	// 한 웨이브에 떨어뜨릴 코인 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Spawn", meta = (ClampMin = "1"))
	int32 CoinsPerWave = 3;

	// 동시에 남아 있을 수 있는 최대 코인 수 (낙하 중 + 바닥)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Spawn", meta = (ClampMin = "1"))
	int32 MaxActiveCoins = 12;

	// true면 N/S 랜덤, false면 N/S 교대
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Spawn")
	bool bRandomPolarity = true;

	// 스폰 영역 중심 액터 (보스 아레나 중앙 등, 비어 있으면 소유 액터 위치)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "MagneticCoinPattern|Area")
	TObjectPtr<AActor> SpawnCenterActor;

	// 스폰 영역 반 크기 (XY만 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Area")
	FVector SpawnAreaExtent = FVector(600.f, 600.f, 0.f);

	// 영역 중심 높이에서 이만큼 위에서 스폰 (코인 GroundTraceDistance 안이어야 바닥을 찾음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Area", meta = (ClampMin = "0.0"))
	float SpawnHeight = 1000.f;

	// 스폰 코인 스케일 (코인 BP 루트가 메시라 여기서 지정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoinPattern|Area")
	FVector CoinScale = FVector(0.5f, 0.5f, 0.2f);

private:
	TArray<TWeakObjectPtr<AMagneticCoin>> SpawnedCoins;

	bool bPatternActive = false;

	float IntervalTimer = 0.f;

	float DepletedTimer = 0.f;

	EMagneticPolarity LastPolarity = EMagneticPolarity::S;
};
