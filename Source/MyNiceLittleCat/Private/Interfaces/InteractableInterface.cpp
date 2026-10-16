/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 상호작용 인터페이스 (감지 기준점 외에는 기본 구현 없음, 구현체에서 _Implementation 정의)

#include "Interfaces/InteractableInterface.h"
#include "GameFramework/Actor.h"

FVector IInteractableInterface::GetInteractFocusLocation_Implementation(AActor* InteractInstigator) const
{
	const AActor* Self = Cast<const AActor>(this);
	return Self ? Self->GetActorLocation() : FVector::ZeroVector;
}
