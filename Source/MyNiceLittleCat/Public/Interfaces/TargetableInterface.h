// 작성자: 강진구
// 작성일자: 2026-10-12
// 
// 공격/락온 대상이 구현하는 인터페이스입니다 (적, 플레이어.
// - 플레이어 락온: 자기 자신을 제외한 구현체를 후보로 사용
// - 적 공격: 대상이 살아 있는지 확인할 때 사용 (EnemyTargetUtils)

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
