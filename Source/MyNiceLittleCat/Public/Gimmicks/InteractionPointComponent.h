/// 작성자: 최요환
/// 작성일자: 2026-10-16
///
/// 상호작용할 때 캐릭터가 설 위치와 바라볼 방향을 표시하는 컴포넌트입니다.
/// 인터랙터블 BP에 추가하고 위치·회전을 배치하면, 정렬 모드가 SnapToPoint일 때
/// 캐릭터가 이 지점으로 이동해 화살표(X축) 방향을 바라봅니다.
/// 여러 개 배치하면 플레이어와 가장 가까운 지점을 사용합니다 (예: 양면 레버).
/// 화살표는 에디터에서만 보이고 게임에서는 숨겨집니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ArrowComponent.h"
#include "InteractionPointComponent.generated.h"

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UInteractionPointComponent : public UArrowComponent
{
	GENERATED_BODY()

public:
	UInteractionPointComponent();
};
