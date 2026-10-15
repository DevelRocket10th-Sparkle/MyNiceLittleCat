// 작성자: 최요환
// 작성일자: 2026-10-15
//
// 플레이어가 상호작용할 수 있는 액터가 구현하는 인터페이스입니다.
// 감지(포커스 대상 선정, 입력 처리)는 플레이어 측 상호작용 컴포넌트가 담당하고,
// 이 인터페이스는 감지된 액터가 어떻게 반응할지만 정의합니다.
// BP 구현체도 호출되도록 반드시 Implements<UInteractableInterface>() 확인 후
// IInteractableInterface::Execute_XXX(Actor, ...) 형태로 호출해야 합니다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IInteractableInterface
{
	GENERATED_BODY()

public:
	// 상호작용 실행
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* InteractInstigator);

	// 현재 상호작용 가능 여부
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* InteractInstigator) const;

	// 상호작용 대상으로 포커스됨 (하이라이트, 안내 UI 등)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnFocusBegin(AActor* InteractInstigator);

	// 포커스 해제
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnFocusEnd(AActor* InteractInstigator);
};
