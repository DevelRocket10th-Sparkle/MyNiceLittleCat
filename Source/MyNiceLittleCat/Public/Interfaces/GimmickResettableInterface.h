/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 퍼즐을 처음 상태로 되돌릴 수 있는 기믹이 구현하는 인터페이스입니다 (수신기, 거울 등).
/// 켜짐/꺼짐 신호(IActivatableInterface)와 별개로, 되돌리기 스위치(AGimmickResetSwitch)가 Targets에 호출합니다.
/// 순서를 잘못 풀면 더 진행할 수 없는 퍼즐(신호로 도는 거울 + Latch 수신기 등)이 막히지 않게 하는 용도입니다.
/// BP 구현체도 호출되도록 Implements<UGimmickResettableInterface>() 확인 후 Execute_ResetGimmick으로 호출합니다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GimmickResettableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UGimmickResettableInterface : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IGimmickResettableInterface
{
	GENERATED_BODY()

public:
	// 처음(레벨 시작) 상태로 되돌림 (ResetInstigator: 되돌리기를 요청한 액터)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Gimmick|Reset")
	void ResetGimmick(AActor* ResetInstigator);
};
