// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.19.
* 비행형 몬스터의 루트 모션 관련 회전 문제 처리를 위해
* 캐릭터 무브먼트 컴포넌트를 상속하여 일부 함수 오버라이드 구현
*/

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
