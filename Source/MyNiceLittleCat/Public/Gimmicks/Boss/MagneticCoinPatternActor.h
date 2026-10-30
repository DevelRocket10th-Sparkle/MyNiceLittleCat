/// 작성자: 최요환
/// 작성일자: 2026-10-28
///
/// 경기장에 배치하는 자성 코인 낙하 패턴 액터입니다.
/// UMagneticCoinPatternComponent를 들고 있으며, 액터 위치가 낙하 영역 중심입니다.
/// 무대(실내·야외·옥상 등)마다 인스턴스를 배치해 영역 크기·높이·주기·코인 클래스를 따로 설정합니다.
/// SpawnArea 박스는 Pattern의 SpawnAreaExtent를 에디터에서 보여주는 용도입니다(콜리전 없음).
/// 보스전에서는 매니저의 PhaseGimmickActors에 이 액터를 키와 함께 넣고,
/// 보스의 UBossGimmickComponent::CoinPatternPhases에 PhaseId → 그 키를 지정하면 페이즈 진입 시 시작, 종료 시 정지됩니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MagneticCoinPatternActor.generated.h"

class UBoxComponent;
class UMagneticCoinPatternComponent;

UCLASS()
class MYNICELITTLECAT_API AMagneticCoinPatternActor : public AActor
{
	GENERATED_BODY()

public:
	AMagneticCoinPatternActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category = "MagneticCoinPattern")
	UMagneticCoinPatternComponent* GetPattern() const { return Pattern; }

	// 에디터·PIE 확인용: 패턴 시작/정지 전환
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "MagneticCoinPattern")
	void TogglePattern();

	// 에디터·PIE 확인용: 즉시 한 웨이브
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "MagneticCoinPattern")
	void SpawnWave();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "MagneticCoinPattern")
	void ClearCoins();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	// 낙하 영역 표시용 (Pattern의 SpawnAreaExtent에 맞춰짐, 콜리전 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> SpawnArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMagneticCoinPatternComponent> Pattern;
};
