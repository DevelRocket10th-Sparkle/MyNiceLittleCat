// 작성자: 우진
// 작성일자: 2026-11-03
// 보스를 잡으면 끝 벽이 빛으로 녹아내리며 관문(포탈)으로 바뀌는 액터
// 순서: 닫힘(그냥 벽) -> 충전(마법진이 그려지며 벽이 빛남) -> 녹아내림(아치 구멍 + 소용돌이) -> 열림(들어가면 로그)
// 원래 벽(Wall_30_0_90)을 지우고 같은 자리에 놓음 -> 이 액터가 벽 메시를 직접 들고 있어서 벽이 사라지는 연출을 할 수 있음
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ActivatableInterface.h"
#include "TutorialPortal.generated.h"

UENUM()
enum class ETutorialPortalState : uint8
{
	Closed,     // 그냥 벽
	Charging,   // 마법진이 그려지며 벽이 빛남
	Dissolving, // 벽이 녹아 구멍이 뚫리고 소용돌이가 펼쳐짐
	Open        // 열림 (들어갈 수 있음)
};

UCLASS()
class MYNICELITTLECAT_API ATutorialPortal : public AActor, public IActivatableInterface
{
	GENERATED_BODY()
	
public:	
	ATutorialPortal();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	// =======================================================================
public:
	// IActivatableInterface: 매니저의 SetActivated(Portal, true) 로 열림
	virtual void Activate_Implementation(AActor* Activator) override { OpenPortal(); }
	virtual void Deactivate_Implementation(AActor* Activator) override {} // 한 번 열리면 닫지 않음

	// 벽 -> 관문 변신 시작 (이미 시작했으면 무시)
	UFUNCTION(BlueprintCallable, Category = "Tutorial|Portal")
	void OpenPortal();

	UFUNCTION(BlueprintPure, Category = "Tutorial|Portal")
	bool IsOpen() const { return ETutorialPortalState::Open == State; }

private:
	void ChangeState(ETutorialPortalState NewState);
	void ApplyVisuals();        // 지금 상태 + 진행도 -> 머티리얼 / 빛 값
	void HandlePlayerEnter();   // 플레이어가 관문에 들어옴 (지금은 로그만. 나중에 레벨 이동으로 바꿀 자리)

	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	// =======================================================================
protected:
	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Portal")
	TObjectPtr<class USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Portal")
	TObjectPtr<class UStaticMeshComponent> WallMesh;     // 녹아내릴 벽 (BP: SM_mdwallV1 + M_Tutorial_WallDissolve)

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Portal")
	TObjectPtr<class UDecalComponent> RuneDecal;         // 벽에 그려지는 마법진 (BP: M_Tutorial_PortalRune)

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Portal")
	TObjectPtr<class UStaticMeshComponent> SwirlPlane;   // 구멍을 채우는 소용돌이 면 (BP: Plane + M_Tutorial_PortalSwirl)

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Portal")
	TObjectPtr<class UNiagaraComponent> LoopFX;          // 열린 동안 반복 효과 (BP: NS_Tutorial_PortalLoop)

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Portal")
	TObjectPtr<class UBoxComponent> Trigger;             // 들어가는 범위 (열리기 전엔 꺼져 있음)

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Portal")
	TObjectPtr<class UPointLightComponent> PortalLight;  // 주변 벽/바닥을 물들이는 빛

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Portal")
	TObjectPtr<class UAudioComponent> LoopAudio;         // 열린 동안 웅웅거리는 소리 (BP 에서 Sound 지정, 반복 에셋)

	// ---- 시간 ----
	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal", meta = (ClampMin = "0.1"))
	float ChargeTime = 1.5f;     // 마법진이 다 그려지는 시간

	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal", meta = (ClampMin = "0.1"))
	float DissolveTime = 1.8f;   // 벽이 다 녹는 시간

	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float SwirlStart = 0.4f;     // 녹는 진행도가 이만큼 됐을 때부터 소용돌이가 펼쳐짐

	// ---- 밝기 ----
	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal", meta = (ClampMin = "0.0"))
	float ChargeGlow = 4.f;      // 충전 중 벽 아치 부분이 빛나는 최대 밝기

	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal", meta = (ClampMin = "0.0"))
	float LightIntensity = 8000.f; // 다 열렸을 때 조명 밝기

	// ---- 효과 ----
	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal|FX")
	TObjectPtr<class UNiagaraSystem> OpenBurstEffect;    // 벽이 뚫리는 순간 1회 (NS_Tutorial_PortalOpen)

	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal|FX")
	TObjectPtr<class USoundBase> ChargeSound;            // 충전 시작 (우르르)

	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal|FX")
	TObjectPtr<class USoundBase> OpenSound;              // 뚫리는 순간 (펑 / 쿠웅)

	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal|FX")
	TSubclassOf<class UCameraShakeBase> OpenShake;       // 충전 시작 + 뚫리는 순간 흔들림

	UPROPERTY(EditAnywhere, Category = "Tutorial|Portal|FX", meta = (ClampMin = "0.0"))
	float OpenShakeScale = 1.f;

private:
	ETutorialPortalState State = ETutorialPortalState::Closed;
	float StateElapsed = 0.f; // 지금 상태가 된 뒤 지난 시간

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WallMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SwirlMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RuneMID;
};
