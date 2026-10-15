// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "EnemyTypes.h"

#include "EnemyAnimInstance.generated.h"

class UPawnMovementComponent;

UCLASS()
class MYNICELITTLECAT_API UEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public :

	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;

protected :
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float MoveSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsInCombat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EEnemyLifeState LifeState;

private :

	TWeakObjectPtr<UPawnMovementComponent> OwnerMovementComponent = nullptr;
	
};
