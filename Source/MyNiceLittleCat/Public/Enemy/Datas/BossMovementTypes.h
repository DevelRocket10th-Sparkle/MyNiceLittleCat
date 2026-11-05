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

// 페이즈 기본 이동과 다르게 움직일 패턴
USTRUCT(BlueprintType)
struct FBossPatternMovementOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PatternId;

	// 체크 시 아래 MovementMode로 페이즈 기본 이동을 덮어씀
	// 해제 시 페이즈 기본 이동을 따르고, 속도 배수만 적용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOverrideMovementMode = false;

	// Waypoints는 페이즈의 MovementRouteId를 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bOverrideMovementMode"))
	EBossPhaseMovementMode MovementMode = EBossPhaseMovementMode::Stop;

	// 패턴이 "활성 구간"이라고 알리는 동안 곱할 이동 속도 배수 (1 = 기본)
	// 활성 구간은 GA가 정함 (LaserBeam은 발사 중)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float ActiveMoveSpeedMultiplier = 1.f;
};
