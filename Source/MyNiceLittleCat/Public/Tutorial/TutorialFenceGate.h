// 작성자: 우진
// 작성일자: 2026-11-10
//
// 울타리 줄 가운데 뚫린 입구를 막았다 열었다 하는 가림막
// 평소엔 열려 있어서(가림막이 땅속) 자유롭게 드나들고, 닫히면 가림막이 땅에서 솟아올라 막음
// 의도: 자성 튜토리얼 동안 플레이어가 구역 밖으로 나가지 못하게 하려고 만듦
//       (자성 튜토리얼 매니저가 레벨 인스턴스 안에 있어서 직접 참조 대신 태그로 찾아 열고 닫음)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ActivatableInterface.h"
#include "TutorialFenceGate.generated.h"

UCLASS()
class MYNICELITTLECAT_API ATutorialFenceGate : public AActor, public IActivatableInterface
{
	GENERATED_BODY()

public:
	ATutorialFenceGate();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	// =======================================================================
public:
	// 입구를 막음 (가림막이 솟아오름)
	UFUNCTION(BlueprintCallable, Category = "Tutorial|FenceGate")
	void Close();

	// 입구를 엶 (가림막이 땅속으로 내려감)
	UFUNCTION(BlueprintCallable, Category = "Tutorial|FenceGate")
	void Open();

	UFUNCTION(BlueprintPure, Category = "Tutorial|FenceGate")
	bool IsClosed() const { return bWantClosed; }

	// IActivatableInterface: Activate = 막기, Deactivate = 열기
	virtual void Activate_Implementation(AActor* Activator) override { Close(); }
	virtual void Deactivate_Implementation(AActor* Activator) override { Open(); }

private:
	void ApplyAlpha();  // 닫힌 정도(0-1) -> 가림막 높이 / 충돌
	void PlayFX(class USoundBase* _sound);
	// =======================================================================
protected:
	UPROPERTY(VisibleAnywhere, Category = "Tutorial|FenceGate")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|FenceGate")
	TObjectPtr<class UStaticMeshComponent> BlockerMesh; // 솟아오르는 가림막 (BP: SM_vifencegateblocker 등)

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|FenceGate")
	TObjectPtr<class UBoxComponent> BlockVolume;        // 실제로 막는 충돌 (닫혔을 때만 켜짐. 메시 충돌 모양에 의존하지 않으려고 따로 둠)

	UPROPERTY(EditAnywhere, Category = "Tutorial|FenceGate")
	bool bStartClosed = false;  // 게임 시작 시 막혀 있을지 (기본: 열림 = 자유롭게 드나듦)

	UPROPERTY(EditAnywhere, Category = "Tutorial|FenceGate")
	float SinkDepth = 250.f;    // 열렸을 때 가림막이 땅속으로 내려가 있는 깊이 (메시 높이보다 크게)

	UPROPERTY(EditAnywhere, Category = "Tutorial|FenceGate", meta = (ClampMin = "0.01", Units = "s"))
	float MoveDuration = 0.6f;  // 솟거나 내려가는 시간

	UPROPERTY(EditAnywhere, Category = "Tutorial|FenceGate|FX")
	TObjectPtr<class UNiagaraSystem> MoveEffect; // 솟거나 내려갈 때 바닥 먼지 (NS_Tutorial_DoorDust 등)

	UPROPERTY(EditAnywhere, Category = "Tutorial|FenceGate|FX")
	TObjectPtr<USoundBase> CloseSound;           // 막힐 때 소리

	UPROPERTY(EditAnywhere, Category = "Tutorial|FenceGate|FX")
	TObjectPtr<USoundBase> OpenSound;            // 열릴 때 소리

private:
	bool bWantClosed = false; // 목표 상태
	float CloseAlpha = 0.f;   // 0 = 열림(땅속), 1 = 닫힘(솟아오름)
};
