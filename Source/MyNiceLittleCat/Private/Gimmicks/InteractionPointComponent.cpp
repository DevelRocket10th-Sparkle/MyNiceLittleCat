/// 작성자: 최요환
/// 작성일자: 2026-10-16
///
/// 상호작용 지점 컴포넌트 구현

#include "Gimmicks/InteractionPointComponent.h"

UInteractionPointComponent::UInteractionPointComponent()
{
	ArrowColor = FColor(0, 200, 255);
	ArrowSize = 0.5f;
	bHiddenInGame = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}
