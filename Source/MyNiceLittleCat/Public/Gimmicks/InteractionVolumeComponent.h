/// 작성자: 최요환
/// 작성일자: 2026-10-16
///
/// 상호작용 감지 영역을 지정하는 박스 컴포넌트입니다.
/// 인터랙터블 BP에 추가하면 플레이어 측 상호작용 컴포넌트는 감지 반경 대신
/// 캐릭터(캡슐)가 이 박스에 닿아 있을 때만 그 액터를 감지합니다.
/// 정면 각도는 이 박스 표면에서 플레이어와 가장 가까운 점 기준으로 판정합니다.
/// 큰 액터는 외곽 전체를 감싸고, 특정 부분(손잡이, 앞면 등)에서만 상호작용하게 하려면 그 부분에만 배치합니다.
/// 여러 개 배치하면 어느 하나에만 닿아도 감지됩니다.
/// 감지 쿼리에만 걸리고 플레이어·물리는 막지 않으며, 게임에서는 보이지 않습니다.
/// 서는 위치·방향(정렬)은 별개로 InteractionPoint 컴포넌트가 담당합니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "InteractionVolumeComponent.generated.h"

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UInteractionVolumeComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UInteractionVolumeComponent();

	// 원기둥(캐릭터 캡슐 근사)이 이 박스에 닿아 있는지 (박스 회전 반영, 수평은 Radius, 상하는 HalfHeight만큼 여유)
	bool IsTouchingCylinder(const FVector& Center, float Radius, float HalfHeight) const;
};
