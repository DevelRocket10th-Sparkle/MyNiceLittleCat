/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 체크포인트 테스트용 조작 액터입니다.
/// 플레이어 사망 → 부활 연결이 준비되기 전에 사망을 대신해 UGameProgressSubsystem::RespawnPlayer를 호출합니다.
/// 테스트 키 (기본): K 부활(사망 흉내), L 세이브 초기화. 화면에 현재 체크포인트·세이브 상태를 표시합니다.
/// 디테일 패널(PIE 중 F8로 빠져나와 선택)의 버튼으로도 같은 조작을 할 수 있습니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Test_CheckpointControl.generated.h"

UCLASS()
class MYNICELITTLECAT_API ATest_CheckpointControl : public AActor
{
	GENERATED_BODY()

public:
	ATest_CheckpointControl();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Checkpoint|Test")
	void RespawnPlayer();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Checkpoint|Test")
	void ResetProgress();

protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Test")
	bool bShowDebugText = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Test|Input")
	bool bBindTestKeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey RespawnKey = EKeys::K;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey ResetProgressKey = EKeys::L;
};
