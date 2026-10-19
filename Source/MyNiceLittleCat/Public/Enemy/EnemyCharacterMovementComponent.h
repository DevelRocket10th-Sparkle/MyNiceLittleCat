// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnemyCharacterMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UEnemyCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public :

	virtual bool CanStartPathFollowing() const override;

	virtual FVector GetActorFeetLocation() const override;
	
	virtual void RequestDirectMove(
		const FVector& MoveVelocity,
		bool bForceMaxSpeed
	) override;

	virtual void RequestPathMove(const FVector& MoveInput) override;

	void InitializeHoverHeight();

	virtual FVector ConstrainAnimRootMotionVelocity(
		const FVector& RootMotionVelocity,
		const FVector& CurrentVelocity
	) const override;

	virtual bool ShouldRemainVertical() const override;

protected :

	// 평평한 지면 위 일정 높이 비행에 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path Find")
	bool bUseGroundPathForFlight = false;

private :

	// 캡슐 바닥과 지면 사이의 높이
	UPROPERTY(VisibleInstanceOnly, Category = "Path Find")
	float HoverHeight = 0.f;

	// 길 찾기 기준으로 사용할 바닥 높이
	float NavigationGroundZ = 0.f;

	// 길 찾기용 바닥 높이 초기화 완료 여부
	bool bNavigationGroundInitialized = false;
	
};
