// 작성자: 최요환
// 작성일자: 2026-10-15
//
// 다른 기믹으로부터 활성화/비활성화 신호를 받는 액터가 구현하는 인터페이스입니다.
// 예: 레버(AInteractableBase)의 LinkedActors에 등록된 문, 배리어 등
// BP 구현체도 호출되도록 Implements<UActivatableInterface>() 확인 후
// IActivatableInterface::Execute_XXX(Actor, ...) 형태로 호출해야 합니다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ActivatableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UActivatableInterface : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IActivatableInterface
{
	GENERATED_BODY()

public:
	// 활성화 신호 수신 (Activator: 신호를 보낸 액터)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Activation")
	void Activate(AActor* Activator);

	// 비활성화 신호 수신
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Activation")
	void Deactivate(AActor* Activator);
};
