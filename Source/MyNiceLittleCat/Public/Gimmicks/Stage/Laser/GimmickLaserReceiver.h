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
/// 색만 틀린 빛(정면·반사 조건은 맞음)이 들어오면 한 번 거부 알림(OnBeamRejected, On Beam Rejected)을 보내고
/// 본체·색 표시 머티리얼의 RejectFlashParameterName을 RejectFlashDuration 동안 1→0으로 깜빡입니다.
/// ColorIndicator = 받는 색 표시(통 입구 링 등). AcceptedColors 첫 색을 머티리얼 ColorParameterName에 넣고, 색 제한이 없으면 숨깁니다.
/// 위치는 ColorIndicatorOffset(정면·오른쪽·위 방향 cm)으로 정해 인스턴스 스케일과 상관없이 통 입구에 붙습니다. 메시는 BP에서 지정합니다.
/// Mesh(루트) = 수신기 본체(빔을 막는 콜리전 필요), 메시·머티리얼은 BP에서 지정합니다.
/// LampMesh = 보이는 등(가로등 등). 켜지면 LitLampMesh, 꺼지면 UnlitLampMesh로 바꾸고 LampLight를 켜고 끕니다.
/// CoreMesh = 통 안에서 빛을 받는 부분(수정 등). 충전·거부 깜빡임·받는 색을 본체와 같은 머티리얼 파라미터로 받습니다.
/// LampMesh·CoreMesh는 콜리전 없음·절대 스케일이고, 위치는 ColorIndicator처럼 정면 기준 오프셋(cm)으로 정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "Interfaces/LaserReceiverInterface.h"
#include "Interfaces/GimmickResettableInterface.h"
#include "Gimmicks/Laser/LaserBeamColor.h"
#include "GimmickLaserReceiver.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UArrowComponent;
class UPointLightComponent;
class ULaserBeamComponent;
class AGimmickLaserReceiver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLaserReceiverBeamRejected, AGimmickLaserReceiver*, Receiver, ELaserBeamColor, BeamColor);

UENUM(BlueprintType)
enum class EGimmickLaserReceiveMode : uint8
{
	// 한 번 켜지면 계속 켜짐
	Latch,
	// 빔이 떠나면 잠시 뒤 꺼짐
	Hold
};

UCLASS()
class MYNICELITTLECAT_API AGimmickLaserReceiver : public AGimmickActorBase, public ILaserReceiverInterface, public IGimmickResettableInterface
{
	GENERATED_BODY()

public:
	AGimmickLaserReceiver();

	virtual void Tick(float DeltaTime) override;

	// ILaserReceiverInterface
	virtual void ReceiveLaserHit_Implementation(ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount) override;
	virtual void EndLaserHit_Implementation(ULaserBeamComponent* Laser) override;

	// IGimmickResettableInterface (꺼지고 충전 0, 빔이 그대로 닿아 있으면 다시 충전)
	virtual void ResetGimmick_Implementation(AActor* ResetInstigator) override;

	// 충전 정도 (0~1)
	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserReceiver")
	float GetCharge() const { return Charge; }

	// 지금 조건에 맞는 빔을 받고 있는지
	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserReceiver")
	bool IsReceivingBeam() const;

	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserReceiver")
	EGimmickLaserReceiveMode GetReceiveMode() const { return ReceiveMode; }

public:
	// 색만 틀린 빛이 들어오기 시작할 때 (같은 레이저는 떠났다 다시 올 때까지 한 번)
	UPROPERTY(BlueprintAssignable, Category = "Gimmick|LaserReceiver")
	FOnLaserReceiverBeamRejected OnBeamRejected;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	// 이 빔을 받을지 (반사 횟수·맞은 부품·들어온 방향·색). 색만 틀렸으면 bOutColorMismatch = true
	bool AcceptsBeam(const ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount, bool* bOutColorMismatch = nullptr) const;

	// 색만 틀린 빛 처리 (새로 들어온 레이저면 알림·깜빡임)
	void HandleColorRejected(ULaserBeamComponent* Laser);

	void SetRejectFlash(float Alpha);

	// 색 표시 위치·색·보이기 갱신
	void UpdateColorIndicator();

	// 등·코어 위치와 코어 색 갱신
	void UpdateVisualParts();

	// 정면(수평) 기준 오프셋 위치에 부품 배치
	void PlaceByFront(USceneComponent* Part, const FVector& Offset, const FRotator& Rotation) const;

	// 등 메시·불빛을 켜짐/꺼짐에 맞춤
	void ApplyLampState(bool bLit);

	void SetCharge(float NewCharge);

	// 충전·해제 대기가 필요할 때만 틱
	void UpdateTickEnabled();

	void ApplyReceiveMode();

	// 조건에 맞는 빔을 받기 시작 / 모두 떠남 (BP 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserReceiver", meta = (DisplayName = "On Beam Received"))
	void K2_OnBeamReceived();

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserReceiver", meta = (DisplayName = "On Beam Lost"))
	void K2_OnBeamLost();

	// 색만 틀린 빛이 들어옴 (거부 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserReceiver", meta = (DisplayName = "On Beam Rejected"))
	void K2_OnBeamRejected(ELaserBeamColor BeamColor);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 빛을 받는 정면 방향 (bAcceptAnySide가 꺼져 있을 때)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> ReceiveDirection;

	// 받는 색 표시 (콜리전 없음, 절대 스케일). 메시는 BP에서 지정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ColorIndicator;

	// 보이는 등 (콜리전 없음, 절대 스케일). 메시는 UnlitLampMesh/LitLampMesh로 정함
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LampMesh;

	// 켜졌을 때만 보이는 불빛 (LampMesh 기준 위치)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> LampLight;

	// 통 안에서 빛을 받는 부분 (콜리전 없음, 절대 스케일). 충전·거부·색 파라미터를 받음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CoreMesh;

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

	// 받을 빛 색 (비어 있으면 모든 색). 예: Red만 넣으면 빨간 필터를 지난 빛만 인정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver")
	TArray<ELaserBeamColor> AcceptedColors;

	// 충전 정도를 넣을 메시 머티리얼 스칼라 파라미터 이름 (없으면 무시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual")
	FName ChargeParameterName = TEXT("Charge");

	// 거부 깜빡임 정도(1→0)를 넣을 스칼라 파라미터 이름 (본체·색 표시, 없으면 무시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual")
	FName RejectFlashParameterName = TEXT("RejectFlash");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual", meta = (ClampMin = "0.0", Units = "s"))
	float RejectFlashDuration = 0.3f;

	// 색이 맞지 않는 빛을 거부할 때 효과음 (레이저당 한 번)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Sound")
	TObjectPtr<USoundBase> RejectSound;

	// 색 표시에 받는 색을 넣을 벡터 파라미터 이름 (엔진 기본 도형 머티리얼은 Color)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual")
	FName ColorParameterName = TEXT("Color");

	// 색 표시 위치 (정면 방향 X, 오른쪽 Y, 위 Z, cm). 기본 = 통 입구, 레이저 높이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual")
	FVector ColorIndicatorOffset = FVector(140.f, 0.f, 60.f);

	// 정면 방향 기준 색 표시 회전 (기본 = 원기둥 축을 정면으로 눕힘)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual")
	FRotator ColorIndicatorRotation = FRotator(90.f, 0.f, 0.f);

	// 꺼졌을 때 등 메시 (비면 바꾸지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Lamp")
	TObjectPtr<UStaticMesh> UnlitLampMesh;

	// 켜졌을 때 등 메시 (비면 바꾸지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Lamp")
	TObjectPtr<UStaticMesh> LitLampMesh;

	// 등 위치 (정면 방향 X, 오른쪽 Y, 위 Z, cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Lamp")
	FVector LampOffset = FVector::ZeroVector;

	// 정면 방향 기준 등 회전
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Lamp")
	FRotator LampRotation = FRotator::ZeroRotator;

	// 코어 위치 (정면 방향 X, 오른쪽 Y, 위 Z, cm). 기본 = 통 안, 레이저 높이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual")
	FVector CoreOffset = FVector(0.f, 0.f, 60.f);

	// 정면 방향 기준 코어 회전
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual")
	FRotator CoreRotation = FRotator::ZeroRotator;

	// 색 제한이 없을 때 코어 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserReceiver|Visual")
	FLinearColor CoreDefaultColor = FLinearColor(1.f, 0.75f, 0.4f);

private:
	// 지금 조건에 맞는 빔을 비추고 있는 레이저
	TSet<TWeakObjectPtr<ULaserBeamComponent>> ReceivingLasers;

	float Charge = 0.f;

	// Hold: 빔이 떠난 뒤 지난 시간
	float TimeSinceBeamLost = 0.f;

	bool bWasReceiving = false;

	// 색만 틀린 빛을 비추고 있는 레이저 (같은 레이저 알림은 한 번)
	TSet<TWeakObjectPtr<ULaserBeamComponent>> RejectedLasers;

	float RejectFlashRemaining = 0.f;
};
