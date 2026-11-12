/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 레이저 빛을 받는 동안 자기 레이저(출력)를 새로 쏘는 빛 장치의 공통 베이스입니다 (스테이지 2: 중계 장치·분광기·색 필터).
/// 받은 빛을 직접 꺾거나 나누지 않고, 수신기처럼 빛을 받은 뒤(ILaserReceiverInterface) 출력 레이저를 켜는 방식이라
/// 레이저 계산(ULaserBeamComponent)과 반사 인터페이스는 그대로 쓰며, 나가는 빛도 거울·펫 반사판으로 다시 꺾을 수 있습니다.
/// - 받는 조건: InputDirection 화살표 정면으로 InputHalfAngle 안에서 들어온 빛(bAcceptAnySide면 어느 면이든), 반사 MinReflections 이상,
///   색은 자식 클래스가 정함(ComputeOutputColor가 false면 막음)
/// - 출력: GetOutputLasers의 레이저를 각자 정면으로 발사. 색 = ComputeOutputColor 결과, 장치 거친 횟수 = 받은 빛 중 가장 적은 값 + 1
/// - 빛을 받는 동안 켜짐(Activated)이라 LinkedActors(문·표시등)에도 신호를 보냅니다. 빛이 떠나고 ReleaseDelay가 지나면 꺼짐
/// - 장치끼리 빛을 주고받는 고리: 장치를 MaxChainDepth번 넘게 거친 빛은 받지 않아, 원래 발사기가 꺼지면 고리도 몇 틱 안에 꺼짐
/// 출력은 피해 없는 퍼즐용 빛입니다(Pawn 통과). 출력 레이저 높이는 바닥(액터 원점)에서 BeamHeight(60cm)로 맞춥니다.
/// Mesh(루트) = 장치 본체(빛을 막는 콜리전 필요), 메시·출력 레이저 이펙트는 BP에서 지정합니다.
/// 회전(2026-11-05): RotateMode가 Interact면 E로, Signal이면 신호가 켜질 때마다 장치 전체(받는 면·출력 포함)가 RotateStep만큼 돎 (기본 Fixed).
///   켜짐 상태는 빛을 받는지 표시라 상호작용·신호로 바뀌지 않습니다. 되돌리기 스위치는 시작 각도로 되돌림.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "Gimmicks/Stage/GimmickRotateMode.h"
#include "Gimmicks/Laser/LaserBeamColor.h"
#include "Interfaces/LaserReceiverInterface.h"
#include "Interfaces/GimmickResettableInterface.h"
#include "GimmickLaserDevice.generated.h"

class UStaticMeshComponent;
class UArrowComponent;
class ULaserBeamComponent;

UCLASS(Abstract)
class MYNICELITTLECAT_API AGimmickLaserDevice : public AInteractableBase, public ILaserReceiverInterface, public IGimmickResettableInterface
{
	GENERATED_BODY()

public:
	AGimmickLaserDevice();

	virtual void Tick(float DeltaTime) override;

	// IInteractableInterface (Interact 모드에서 E로 회전)
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;

	// IGimmickResettableInterface (도는 중이면 멈추고 시작 각도로)
	virtual void ResetGimmick_Implementation(AActor* ResetInstigator) override;

	// RotateStep만큼 회전 시작 (Fixed거나 도는 중이면 무시)
	UFUNCTION(BlueprintCallable, Category = "Gimmick|LaserDevice|Rotate")
	void RotateOnce();

	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserDevice|Rotate")
	bool IsRotating() const { return bRotating; }

	// ILaserReceiverInterface
	virtual void ReceiveLaserHit_Implementation(ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount) override;
	virtual void EndLaserHit_Implementation(ULaserBeamComponent* Laser) override;
	virtual bool GetPreviewOutputLasers(ELaserBeamColor InputColor, const FHitResult& Hit, int32 ReflectionCount,
		TArray<ULaserBeamComponent*>& OutLasers, ELaserBeamColor& OutColor) const override;

	// 지금 조건에 맞는 빛을 받고 있는지
	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserDevice")
	bool IsReceivingBeam() const;

	// 지금 내보내는 빛 색 (꺼져 있으면 마지막 값)
	UFUNCTION(BlueprintPure, Category = "Gimmick|LaserDevice")
	ELaserBeamColor GetOutputColor() const { return OutputColor; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void HandleSignalChanged(AActor* Source, bool bSignalOn) override;

	// 회전 방식에 맞춰 상호작용·신호 수신 설정
	void ApplyRotateMode();

	// 회전 중 매 틱 각도 갱신 (끝나면 false)
	bool UpdateRotation(float DeltaTime);

	// 회전 시작 / 끝 (BP 연출용: 소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserDevice|Rotate", meta = (DisplayName = "On Rotate Started"))
	void K2_OnRotateStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserDevice|Rotate", meta = (DisplayName = "On Rotate Finished"))
	void K2_OnRotateFinished();

	// 출력 레이저 목록 (자식 클래스가 만든 컴포넌트)
	virtual void GetOutputLasers(TArray<ULaserBeamComponent*>& OutLasers) const {}

	// 받은 빛 색으로 내보낼 색을 정함. false면 이 빛은 받지 않음 (기본 = 같은 색 그대로)
	virtual bool ComputeOutputColor(ELaserBeamColor InputColor, ELaserBeamColor& OutColor) const;

	// 이 빛을 받을지 (들어온 방향·반사 횟수·장치 거친 횟수·색)
	bool AcceptsBeam(const ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount) const;

	// AcceptsBeam의 판정 (레이저 없이 색·장치 거친 횟수로, 프리뷰에서도 사용)
	bool AcceptsInput(ELaserBeamColor InputColor, int32 InputChainDepth, const FHitResult& Hit, int32 ReflectionCount) const;

	// 들어온 빛 진행 방향(수평, 단위 벡터)을 받을지. 기본 = 받는 면(InputDirection) 기준 InputHalfAngle 안 (bAcceptAnySide면 전부)
	virtual bool AcceptsIncomingDirection(const FVector& Incoming) const;

	// 출력을 켜기 전 매 틱, 기준 빛의 진행 방향(수평)과 빔 중심이 장치에 닿은 위치로 출력 배치 (통과형 장치용, 기본 = 아무것도 안 함)
	virtual void PlaceOutputsForInput(const FVector& InputDirection2D, const FVector& InputLocation) {}

	// 받은 빛 기준으로 출력 켜기/끄기·색·장치 거친 횟수 갱신
	void UpdateOutputs();

	void StopOutputs();

	// 출력 레이저 높이·반사·퍼즐용 빛 설정
	void SetupOutputLasers();

	// 빛을 받기 시작 / 모두 떠남 (BP 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserDevice", meta = (DisplayName = "On Beam Received"))
	void K2_OnBeamReceived(ELaserBeamColor InputColor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|LaserDevice", meta = (DisplayName = "On Beam Lost"))
	void K2_OnBeamLost();

protected:
	// 빛을 받는 면 (화살표가 빛이 들어오는 쪽을 가리킴)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> InputDirection;

	// true면 어느 면으로 들어온 빛이든 받음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice")
	bool bAcceptAnySide = false;

	// 받는 면 기준 허용 각도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg", EditCondition = "!bAcceptAnySide"))
	float InputHalfAngle = 45.f;

	// 받을 최소 반사 횟수 (0이면 발사기에서 곧장 온 빛도 받음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice", meta = (ClampMin = "0"))
	int32 MinReflections = 0;

	// 빛이 떠난 뒤 출력이 꺼지기까지 시간 (빛이 잠깐 끊겨도 깜빡이지 않게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice", meta = (ClampMin = "0.0", Units = "s"))
	float ReleaseDelay = 0.1f;

	// 장치를 이 횟수 넘게 거친 빛은 받지 않음 (장치끼리 빛을 주고받는 고리 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice", meta = (ClampMin = "1"))
	int32 MaxChainDepth = 8;

	// 출력 레이저 높이 (바닥 = 액터 원점 기준, 스테이지 레이저 공통 60cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice", meta = (Units = "cm"))
	float BeamHeight = 60.f;

	// 회전 방식 (장치 전체가 액터 원점 기준 Z축으로 돎)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Rotate")
	EGimmickMirrorRotateMode RotateMode = EGimmickMirrorRotateMode::Fixed;

	// 한 번에 도는 각도 (양수 = 위에서 볼 때 시계 방향)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Rotate", meta = (Units = "deg", EditCondition = "RotateMode != EGimmickMirrorRotateMode::Fixed"))
	float RotateStep = 90.f;

	// 한 번 도는 데 걸리는 시간 (0이면 즉시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Rotate", meta = (ClampMin = "0.0", Units = "s", EditCondition = "RotateMode != EGimmickMirrorRotateMode::Fixed"))
	float RotateDuration = 0.3f;

	// 돌기 시작할 때 효과음 (볼륨·2D 여부는 Gimmick|Sound 설정을 따름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Sound")
	TObjectPtr<USoundBase> RotateSound;

private:
	bool bRotating = false;

	float RotateElapsed = 0.f;

	float RotateStartYaw = 0.f;

	float RotateTargetYaw = 0.f;

	// 레벨 시작 때 각도 (되돌리기용)
	float InitialYaw = 0.f;

	// 지금 조건에 맞는 빛을 비추고 있는 레이저
	TSet<TWeakObjectPtr<ULaserBeamComponent>> ReceivingLasers;

	// 받는 레이저별 마지막 진행 방향(수평)·빔 중심이 닿은 위치 (PlaceOutputsForInput용)
	struct FInputBeamInfo
	{
		FVector Direction2D = FVector::ForwardVector;
		FVector Location = FVector::ZeroVector;
	};
	TMap<TWeakObjectPtr<ULaserBeamComponent>, FInputBeamInfo> InputBeamInfos;

	ELaserBeamColor OutputColor = ELaserBeamColor::White;

	// 빛이 떠난 뒤 지난 시간
	float TimeSinceBeamLost = 0.f;

	bool bWasReceiving = false;
};
