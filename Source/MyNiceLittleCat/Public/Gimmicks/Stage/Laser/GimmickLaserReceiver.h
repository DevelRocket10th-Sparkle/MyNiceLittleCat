/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 레이저 빔을 받으면 켜지고 LinkedActors에 신호를 보내는 수신기입니다 (스테이지 2 반사 퍼즐, 마을에서는 꺼진 가로등).
/// ULaserBeamComponent가 빔 끝이 이 액터일 때 매 틱 ILaserReceiverInterface로 알려주며, 조건에 맞는 빔만 인정합니다:
///   - 반사 횟수가 MinReflections 이상 (기본 1: 발사기에서 곧장 온 빛은 인정하지 않음, 펫 반사판·거울 반사 모두 1회)
///   - bAcceptAnySide가 꺼져 있으면 ReceiveDirection 화살표 정면으로 ReceiveHalfAngle 안에서 들어온 빛만
/// 인정된 빔을 받는 동안 충전(Charge 0→1, ChargeTime)하고, 다 차면 켜집니다. 빔이 없으면 DrainRate로 줄어듭니다.
/// ReceiveMode:
///   Latch = 한 번 켜지면 계속 켜짐 (기본, 세이브 저장)
///   Hold  = 빔이 떠나고 ReleaseDelay가 지나면 꺼짐 (반사판 수명 안에 문을 통과하는 시간 압박 퍼즐)
/// 충전 정도는 메시 머티리얼 스칼라 파라미터(ChargeParameterName)로 전달하고, GetCharge로도 조회할 수 있습니다.
/// Mesh(루트) = 수신기 본체(빔을 막는 콜리전 필요), 메시·머티리얼은 BP에서 지정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "Interfaces/LaserReceiverInterface.h"
#include "GimmickLaserReceiver.generated.h"

class UStaticMeshComponent;
class UArrowComponent;
class ULaserBeamComponent;

UENUM(BlueprintType)
enum class EGimmickLaserReceiveMode : uint8
{
	// 한 번 켜지면 계속 켜짐
	Latch,
	// 빔이 떠나면 잠시 뒤 꺼짐
	Hold
};

UCLASS()
class MYNICELITTLECAT_API AGimmickLaserReceiver : public AGimmickActorBase, public ILaserReceiverInterface
{
	GENERATED_BODY()

public:
	AGimmickLaserReceiver();

	virtual void Tick(float DeltaTime) override;

	// ILaserReceiverInterface
	virtual void ReceiveLaserHit_Implementation(ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount) override;
	virtual void EndLaserHit_Implementation(ULaserBeamComponent* Laser) override;

	// 충전 정도 (0~1)
	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserReceiver")
	float GetCharge() const { return Charge; }

	// 지금 조건에 맞는 빔을 받고 있는지
	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserReceiver")
	bool IsReceivingBeam() const;

	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserReceiver")
	EGimmickLaserReceiveMode GetReceiveMode() const { return ReceiveMode; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	// 이 빔을 받을지 (반사 횟수·들어온 방향)
	bool AcceptsBeam(const FHitResult& Hit, int32 ReflectionCount) const;

	void SetCharge(float NewCharge);

	// 충전·해제 대기가 필요할 때만 틱
	void UpdateTickEnabled();

	void ApplyReceiveMode();

	// 조건에 맞는 빔을 받기 시작 / 모두 떠남 (BP 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserReceiver", meta = (DisplayName = "On Beam Received"))
	void K2_OnBeamReceived();

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserReceiver", meta = (DisplayName = "On Beam Lost"))
	void K2_OnBeamLost();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 빛을 받는 정면 방향 (bAcceptAnySide가 꺼져 있을 때)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> ReceiveDirection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver")
	EGimmickLaserReceiveMode ReceiveMode = EGimmickLaserReceiveMode::Latch;

	// 켜지는 데 필요한 빔 시간 (0이면 닿는 즉시). 회전 거울이 지나가며 잠깐 스치는 빛은 걸러짐
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver", meta = (ClampMin = "0.0", Units = "s"))
	float ChargeTime = 0.5f;

	// 빔이 없을 때 초당 줄어드는 충전량 (1 = 1초에 전부)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver", meta = (ClampMin = "0.0"))
	float DrainRate = 1.f;

	// Hold: 빔이 떠난 뒤 꺼지기까지 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver", meta = (ClampMin = "0.0", Units = "s", EditCondition = "ReceiveMode == EGimmickLaserReceiveMode::Hold", EditConditionHides))
	float ReleaseDelay = 0.3f;

	// 인정할 최소 반사 횟수 (0이면 발사기에서 곧장 온 빛도 인정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver", meta = (ClampMin = "0"))
	int32 MinReflections = 1;

	// true면 어느 면으로 들어온 빛이든 인정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver")
	bool bAcceptAnySide = false;

	// 정면 기준 허용 각도 (들어온 빛과 ReceiveDirection 반대 방향 사이)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg", EditCondition = "!bAcceptAnySide"))
	float ReceiveHalfAngle = 60.f;

	// 충전 정도를 넣을 메시 머티리얼 스칼라 파라미터 이름 (없으면 무시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual")
	FName ChargeParameterName = TEXT("Charge");

private:
	// 지금 조건에 맞는 빔을 비추고 있는 레이저
	TSet<TWeakObjectPtr<ULaserBeamComponent>> ReceivingLasers;

	float Charge = 0.f;

	// Hold: 빔이 떠난 뒤 지난 시간
	float TimeSinceBeamLost = 0.f;

	bool bWasReceiving = false;
};
