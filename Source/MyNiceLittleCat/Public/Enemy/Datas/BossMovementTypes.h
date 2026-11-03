// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BossMovementTypes.generated.h"

class ATargetPoint;

// 페이즈별 Boss 이동 정책
UENUM(BlueprintType)
enum class EBossPhaseMovementMode : uint8
{
	Stop,
	Chasing,
	Waypoints
};

// 레벨에 배치된 Encounter가 보유하는 경로
USTRUCT(BlueprintType)
struct FBossMovementRoute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TObjectPtr<ATargetPoint>> Points;
};

// 페이즈의 기본 회전 방식
UENUM(BlueprintType)
enum class EBossPhaseRotationMode : uint8
{
	FaceTarget,	// 타깃을 바라보는 회전 허용
	Hold		// 페이즈 시작 방향으로 복귀하고 유지
};
