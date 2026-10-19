/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 자성 코인 낙하 패턴 테스트용 스포너입니다.
/// 보스 페이즈 매니저가 준비되기 전에 UMagneticCoinPatternComponent를 대신 실행합니다.
/// 스폰 규칙(방식·주기·개수·극성·영역)은 Pattern 컴포넌트 디테일에서 설정하고,
/// SpawnArea 박스는 Pattern의 SpawnAreaExtent를 에디터에서 보여주는 용도입니다(위치 = 스폰 영역 중심).
/// 테스트 키 (기본): B 패턴 시작/정지, V 즉시 한 웨이브. 남은 코인 수·낙하 피격은 화면 디버그 메시지로 표시합니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Test_MagneticCoinSpawner.generated.h"

class UBoxComponent;
class UMagneticCoinPatternComponent;
class AMagneticCoin;

UCLASS()
class MYNICELITTLECAT_API ATest_MagneticCoinSpawner : public AActor
{
	GENERATED_BODY()

public:
	ATest_MagneticCoinSpawner();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "MagneticCoin|Test")
	void TogglePattern();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "MagneticCoin|Test")
	void SpawnWave();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "MagneticCoin|Test")
	void ClearCoins();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleCoinFallHit(UMagneticCoinPatternComponent* InPattern, AMagneticCoin* Coin, AActor* HitActor);

protected:
	// 스폰 영역 표시용 (Pattern의 SpawnAreaExtent에 맞춰짐, 콜리전 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> SpawnArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMagneticCoinPatternComponent> Pattern;

	// true면 BeginPlay에서 패턴 시작
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Test")
	bool bStartPatternOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Test|Input")
	bool bBindTestKeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey TogglePatternKey = EKeys::B;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey SpawnWaveKey = EKeys::V;

private:
	int32 FallHitCount = 0;
};
