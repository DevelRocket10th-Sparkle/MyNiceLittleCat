#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Enemy_RecoveryTest.generated.h"

// 실제 공격 에셋 없이 정상 종료와 취소 이후의 요청 상태를 검사하기 위한 어빌리티다.
UCLASS(NotBlueprintable, Transient)
class UGA_Enemy_RecoveryTest : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Enemy_RecoveryTest()
	{
		InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
		NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	}

	void FinishNormally()
	{
		if (IsActive())
		{
			// 스펙이나 기본 객체가 아닌 실행 인스턴스의 컨텍스트로 정상 종료를 전달한다.
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
		}
	}
};
