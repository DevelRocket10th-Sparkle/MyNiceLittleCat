/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 분광기입니다 (Archaica 빛 나누기 참고). 뒤(-X)에서 들어온 빛을 받는 동안 같은 색 빛 두 갈래를 내보냅니다.
/// 두 갈래는 정면(+X) 기준 좌우 SplitAngle 방향입니다 (기본 90° = 들어온 빛과 수직으로 양쪽).
/// 받는 면은 InputDirection 화살표(기본 뒤쪽), 허용 각도는 InputHalfAngle. 받는 동안 켜짐이라 LinkedActors에도 신호를 보냅니다.
/// 출력 표시(2026-11-05): 빛이 나가는 두 방향에 돌출된 노즐(LeftNozzle/RightNozzle + 끝 Tip), 받는 쪽에 InputMarker를 둡니다.
///   노즐은 출력 레이저에 붙어 SplitAngle·회전을 따라가고, 위치는 액터 스케일과 상관없이 레이저 시작점에서 cm 단위로 정합니다.
///   메시는 BP에서 지정, 콜리전 없음(빛을 막지 않음). 머티리얼에 "Glow" 파라미터가 있으면 빛을 받는 동안 밝아짐(장치 공통).
/// 양쪽 받기(bAcceptBothEnds, 2026-11-05 기본 켜짐): 받는 면 반대쪽(정면)으로 들어온 빛도 받아 똑같이 양옆으로 나눔. 정면 표시 InputMarkerFront.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/Stage/Laser/GimmickLaserDevice.h"
#include "GimmickLaserSplitter.generated.h"

UCLASS()
class MYNICELITTLECAT_API AGimmickLaserSplitter : public AGimmickLaserDevice
{
	GENERATED_BODY()

public:
	AGimmickLaserSplitter();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void GetOutputLasers(TArray<ULaserBeamComponent*>& OutLasers) const override;
	virtual bool AcceptsIncomingDirection(const FVector& Incoming) const override;

	// 두 출력의 방향을 SplitAngle에 맞춤
	void ApplySplitAngle();

	// 노즐·받는 쪽 표시를 레이저 시작점 기준 위치로
	void UpdateMarkers();

	// Part를 Anchor(레이저·화살표)의 수평 정면으로 Distance만큼, 그 방향 기준 Rotation으로 놓음 (Z는 ZBase + HeightOffset)
	static void PlaceAlongAnchor(USceneComponent* Part, const USceneComponent* Anchor, float ZBase, float Distance, float HeightOffset, const FRotator& Rotation);

protected:
	// 정면 기준 왼쪽(-SplitAngle)으로 나가는 빛
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserBeamComponent> OutputLeft;

	// 정면 기준 오른쪽(+SplitAngle)으로 나가는 빛
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserBeamComponent> OutputRight;

	// 왼쪽 출력 노즐 (몸통) / 끝 표시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftNozzle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftNozzleTip;

	// 오른쪽 출력 노즐 (몸통) / 끝 표시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightNozzle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightNozzleTip;

	// 빛을 받는 쪽 표시 (InputDirection 화살표를 따라감)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> InputMarker;

	// 정면(받는 면 반대쪽) 받는 표시 (bAcceptBothEnds일 때만 보임, 위치·회전은 InputMarker와 대칭)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> InputMarkerFront;

	// 받는 면 반대쪽(정면)으로 들어온 빛도 받음 (같은 허용 각도)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice")
	bool bAcceptBothEnds = true;

	// 정면(+X)에서 두 갈래가 벌어지는 각도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float SplitAngle = 90.f;

	// 레이저 시작점(장치 가운데)에서 노즐 몸통까지 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Marker", meta = (Units = "cm"))
	float NozzleDistance = 40.f;

	// 빛 방향 기준 노즐 몸통 회전 (메시 축을 빛 방향에 맞춤)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Marker")
	FRotator NozzleRotation = FRotator::ZeroRotator;

	// 레이저 시작점에서 노즐 끝 표시까지 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Marker", meta = (Units = "cm"))
	float NozzleTipDistance = 60.f;

	// 빛 방향 기준 노즐 끝 회전
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Marker")
	FRotator NozzleTipRotation = FRotator::ZeroRotator;

	// 노즐 높이 보정 (레이저 높이 기준)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Marker", meta = (Units = "cm"))
	float NozzleHeightOffset = 0.f;

	// 받는 쪽 표시: 장치 가운데(레이저 높이)에서 받는 면 쪽으로 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Marker", meta = (Units = "cm"))
	float InputMarkerDistance = 40.f;

	// 받는 쪽 표시 높이 보정 (레이저 높이 기준)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Marker", meta = (Units = "cm"))
	float InputMarkerHeightOffset = 0.f;

	// 받는 면 방향 기준 받는 쪽 표시 회전
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|LaserDevice|Marker")
	FRotator InputMarkerRotation = FRotator::ZeroRotator;
};
