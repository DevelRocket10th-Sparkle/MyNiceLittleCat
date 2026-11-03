/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 상호작용 안내 UI 위치 컴포넌트 구현

#include "Gimmicks/InteractPromptAnchorComponent.h"

UInteractPromptAnchorComponent::UInteractPromptAnchorComponent()
{
	ArrowColor = FColor(80, 255, 120);
	ArrowSize = 0.5f;
	// 위쪽을 가리켜 안내 UI가 뜨는 방향을 표시
	SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	bHiddenInGame = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}
