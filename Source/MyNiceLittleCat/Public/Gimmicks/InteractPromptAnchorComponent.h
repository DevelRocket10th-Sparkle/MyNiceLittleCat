/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 상호작용 안내 UI(프롬프트)를 띄울 위치를 표시하는 컴포넌트입니다.
/// 인터랙터블 BP에 추가하고 위치를 옮기면, 메시 바운드 윗면 대신 이 위치(+ PromptOffset)에 안내 UI가 표시됩니다.
/// 큰 기계처럼 기본 위치가 플레이어가 서는 쪽과 먼 액터에 사용합니다. 여러 개면 첫 번째를 사용합니다.
/// 화살표(위쪽)는 에디터에서만 보이고 게임에서는 숨겨집니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ArrowComponent.h"
#include "InteractPromptAnchorComponent.generated.h"

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UInteractPromptAnchorComponent : public UArrowComponent
{
	GENERATED_BODY()

public:
	UInteractPromptAnchorComponent();
};
