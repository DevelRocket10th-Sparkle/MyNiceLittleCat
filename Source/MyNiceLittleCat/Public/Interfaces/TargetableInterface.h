// 작성자: 강진구
// 작성일자: 2026-10-12
// 
// 플레이어가 락온 가능한 오브젝트가 구현하는 인터페이스입니다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TargetableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UTargetableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MYNICELITTLECAT_API ITargetableInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
	bool IsTargetAlive() const;
};
