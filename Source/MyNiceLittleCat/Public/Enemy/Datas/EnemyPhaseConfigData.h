// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 보스 페이즈 설정용 데이터 에셋
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "Enemy/Datas/BossMovementTypes.h"

#include "EnemyPhaseConfigData.generated.h"

class ULevelSequence;

// 체력으로 진행하는 전투와 기믹 완료로 진행하는 배리어 구분
UENUM(BlueprintType)
enum class EBossPhaseKind : uint8
{
	Combat,
	Barrier,
	LogOnly
};

// 페이즈 하나의 고정 설정: 실행 상태와 레벨 액터 참조는 저장하지 않음
USTRUCT(BlueprintType)
struct FBossPhaseDefinition
{
	GENERATED_BODY()

	// 페이즈 및 레벨 배리어 연결에 사용하는 고유 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName PhaseId;

	// 다음 페이즈를 체력 또는 배리어 붕괴로 요청할지 결정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EBossPhaseKind Kind = EBossPhaseKind::Combat;

	// 전투 중 체력 하한 비율: 0은 마지막 전투로 사망까지 허용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthFloorRatio = 0.f;

	// 해당 페이즈에서 허용할 PatternSet의 패턴 ID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FName> AllowedPatternIds;

	// 전환 연출을 사용할지 설정; false면 시퀀스를 만들지 않고 다음 페이즈 적용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transition")
	bool bUseTransitionSequence = false;

	// 진입 전에 한 번 재생할 연출; 미지정 또는 재생 실패 시 연출을 건너뜀
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transition", meta = (EditCondition = "bUseTransitionSequence"))
	TObjectPtr<ULevelSequence> TransitionSequence;

	// 전환 GA가 종료되지 않는 경우 실패 처리할 제한 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.1", EditCondition = "bUseTransitionSequence"))
	float TransitionTimeout = 15.f;

	// 기믹과 전환 연출 없이 진입 로그를 출력하고 수동 완료 요청까지 대기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bLogOnly = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	EBossPhaseMovementMode MovementMode = EBossPhaseMovementMode::Chasing;

	// 기본 이동이 Stop이어도 예외가 Waypoints면 경로 필요
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	FName MovementRouteId;

	// 추격, Waypoint 도착 반경 (중심 간 수평 거리)
	// Chasing의 접근 허용 거리와 Waypoints의 지점 도착 허용 반경
	// 추격 후 공격을 시작할 수 있도록 공격 사거리 안쪽에 여유를 두어 설정
	// 실제 공격 가능 여부는 거리, 높이, 시야, GA 활성화 조건을 별도로 검사
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", Units = "cm"))
	float MovementAcceptanceRadius = 100.f;

	// 기본 이동과 다르게 움직일 패턴만 추가 (목록에 없는 패턴은 MovementMode 사용)
	// 적용 구간: 패턴 선택(방향 정렬) ~ 실행 ~ 후딜(후딜은 항상 정지)
	// 단, 공격 실행 중 Stop/AbilityDriven이 우선되며, 후딜에는 이동 잠금으로 정지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (TitleProperty = "PatternId"))
	TArray<FBossPatternMovementOverride> PatternMovementOverrides;

	// 페이즈의 기본 회전 목표
	// Hold는 페이즈 시작 방항 사용 (공격 회전 정렬 Focus가 일시적으로 우선함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotation")
	EBossPhaseRotationMode RotationMode = EBossPhaseRotationMode::FaceTarget;

	// 패턴별 예외가 있으면 해당 모드, PatternId가 None이거나 예외가 없으면 기본 모드 반환
	// 공격의 Stop/AbilityDriven 정책과 이동 잠금은 호출자가 별도로 적용
	EBossPhaseMovementMode ResolveMovementMode(FName PatternId) const
	{
		if (!PatternId.IsNone())
		{
			if (const FBossPatternMovementOverride* Found =
				PatternMovementOverrides.FindByPredicate(
					[PatternId](const FBossPatternMovementOverride& Entry)
					{
						return Entry.PatternId == PatternId;
					}
				))
			{
				return Found->MovementMode;
			}
		}

		return MovementMode;
	}

	// 기본 이동 또는 예외 중 Waypoints 사용 여부 확인
	// 현재 실행 중인 이동 모드를 판단하는 함수는 아님
	bool UsesWaypoints() const
	{
		return MovementMode == EBossPhaseMovementMode::Waypoints
			|| PatternMovementOverrides.ContainsByPredicate(
				[](const FBossPatternMovementOverride& Entry)
				{
					return Entry.MovementMode == EBossPhaseMovementMode::Waypoints;
				}
			);
	}
};

/**
 * 
 */
UCLASS(BlueprintType)
class MYNICELITTLECAT_API UEnemyPhaseConfigData : public UDataAsset
{
	GENERATED_BODY()

public :

	// 개별 설정과 현재 담당 범위로 로그 전용 페이즈인지 확인
	bool IsLogOnlyPhase(int32 Index) const;

	// 중복 ID, 체력 하한 순서 및 마지막 전투 상태 검사
	// 페이즈 순서, 중복 ID 및 체력 하한의 감소 여부 검사
	bool ValidateConfiguration(FString& OutError) const;

public :

	// 실행 순서: CombatA(0.7), Barrier70, CombatB(0.3), Barrier30, CombatC(0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phases")
	TArray<FBossPhaseDefinition> Phases;

	// 첫 페이즈 이후는 기믹 없이 로그만 출력하고 각 페이즈에서 수동 완료까지 대기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phases")
	bool bLogOnlyAfterFirstPhase = false;
};
