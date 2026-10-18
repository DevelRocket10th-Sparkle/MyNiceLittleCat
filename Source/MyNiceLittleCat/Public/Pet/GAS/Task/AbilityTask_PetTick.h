// 작성자: 우진
// 작성일자: 2026-10-18
#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_PetTick.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPetAbilityTickDelegate, float, DeltaTime);

UCLASS()
class MYNICELITTLECAT_API UAbilityTask_PetTick : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_PetTick();

public:
	virtual void TickTask(float DeltaTime) override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;
	// ========================================================================================
public:
	static UAbilityTask_PetTick* CreatePetTick(UGameplayAbility* OwningAbility);

	// ========================================================================================
public:	
	UPROPERTY(BlueprintAssignable)
	FPetAbilityTickDelegate OnTick;
};
