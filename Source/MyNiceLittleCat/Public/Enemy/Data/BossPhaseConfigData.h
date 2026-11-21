// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 수정 일자: 2026.11.17. (허용 패턴 ID 목록 → 가중치 재정의 패턴 목록과 동시 실행 설정)
* 수정 일자: 2026.11.21. (이전 형식 AllowedPatternIds와 옮기기 PostLoad 삭제: 모든 페이즈 DA를 새 형식으로 다시 저장)
* 보스 페이즈 설정용 데이터 에셋
* - 페이즈 순서, 진행 조건(체력/기믹), 패턴 목록과 가중치, 동시 실행, 이동/회전 정책, 기본 전환 연출
* - 레벨 액터 참조와 실행 상태는 저장하지 않음 (레벨 참조는 BossEncounterManager가 보스에 전달)
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "Enemy/Data/BossMovementTypes.h"
#include "Enemy/Data/EnemyPatternSetData.h"

#include "BossPhaseConfigData.generated.h"

class ULevelSequence;

// 다음 페이즈로 넘어가는 조건
UENUM(BlueprintType)
enum class EBossPhaseKind : uint8
{
	Combat,		// 체력이 HealthFloorRatio에 도달하면 진행
	Barrier		// 기믹 컴포넌트가 완료를 통지하면 진행
};

// 페이즈 하나의 고정 설정
USTRUCT(BlueprintType)
struct FBossPhaseDefinition
{
	GENERATED_BODY()

	/* 진행 */

	// 페이즈 및 레벨 기믹/연출 연결에 사용하는 고유 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase")
	FName PhaseId;

	// 다음 페이즈를 체력 또는 기믹 완료로 요청할지 결정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase")
	EBossPhaseKind Kind = EBossPhaseKind::Combat;

	// 전투 페이즈의 체력 하한 비율: 0은 마지막 전투로 사망까지 허용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0.0", ClampMax = "1.0",
		EditCondition = "Kind == EBossPhaseKind::Combat", EditConditionHides))
	float HealthFloorRatio = 0.f;

	/* 패턴 */

	// 이 페이즈에서 쓸 패턴과 가중치 재정의 (PatternSet의 패턴 ID)
	// - 목록에 없는 패턴은 이 페이즈에서 실행하지 않음 (직접 지정도 불가)
	// - 재정의 가중치 0: 자동 선택에서만 제외
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Patterns", meta = (TitleProperty = "PatternId"))
	TArray<FEnemyPatternSelectionEntry> Patterns;

	// 동시 실행 상한, 보조 패턴 확률, 위험도 예산
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Patterns")
	FEnemyPatternConcurrency Concurrency;

	// 상시 패턴: 위 랜덤 선택과 별개로 페이즈 내내 따로 굴러가는 패턴
	// - Patterns 목록에 넣지 않아도 됨 (넣으면 랜덤 선택에도 함께 나옴)
	// - 패턴의 Can Run As Companion이 켜져 있어야 실행 (근접, 돌진, 레이저 빔 불가)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Patterns", meta = (TitleProperty = "PatternId"))
	TArray<FEnemyPersistentPatternEntry> PersistentPatterns;

	/* 기본 전환 연출: 레벨에 전환 담당자(IBossPhaseTransitionHandler)가 있으면 그쪽이 우선 */

	// 이 페이즈로 들어오기 전에 시퀀스를 재생할지 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transition")
	bool bUseTransitionSequence = false;

	// 진입 전에 한 번 재생할 시퀀스: 시퀀서에서 보스 바인딩에 BossBindingTag 태그 필요
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transition", meta = (EditCondition = "bUseTransitionSequence"))
	TObjectPtr<ULevelSequence> TransitionSequence;

	// 컷신으로 재생 (입력 차단, HUD 숨김)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transition", meta = (EditCondition = "bUseTransitionSequence"))
	bool bTransitionIsCutscene = false;

	// 컷신일 때 스킵 허용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transition", meta = (EditCondition = "bUseTransitionSequence && bTransitionIsCutscene"))
	bool bTransitionSkippable = false;

	// 전환 연출 제한 시간: 초과하면 연출을 끊고 다음 페이즈 진행 (레벨 담당자에게도 전달)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0.1", Units = "s"))
	float TransitionTimeout = 15.f;

	/* 이동 */

	// 페이즈 기본 이동
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	EBossPhaseMovementMode MovementMode = EBossPhaseMovementMode::Chasing;

	// Waypoints에 사용할 경로 ID: 기본 이동이 Stop이어도 패턴 예외가 Waypoints면 필요
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	FName MovementRouteId;

	// 추격 접근 거리 / Waypoint 도착 반경 (중심 간 수평 거리)
	// - 추격 후 공격할 수 있도록 공격 사거리 안쪽에 여유를 두어 설정
	// - 실제 공격 가능 여부는 거리, 높이, 시야, GA 활성화 조건을 별도로 검사
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", Units = "cm"))
	float MovementAcceptanceRadius = 100.f;

	// 페이즈 시작 시 경로의 첫 Waypoint로 순간이동 (기본 이동이 Waypoints일 때만)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement",
		meta = (EditCondition = "MovementMode == EBossPhaseMovementMode::Waypoints", EditConditionHides))
	bool bTeleportToFirstWaypoint = false;

	// 페이즈 기본 이동과 다르게 처리할 패턴 (목록에 없는 패턴은 기본 이동, 속도 배수 1)
	// - 이동 모드: bOverrideMovementMode 체크 시 덮어씀 (패턴 선택 ~ 실행 ~ 후딜, 후딜은 이동 잠금으로 정지)
	//   단, 패턴 Movement 정책이 Stop/AbilityDriven이면 실행 중에는 패턴 정책이 우선
	// - 이동 속도: ActiveMoveSpeedMultiplier를 GA가 정한 활성 구간에만 적용 (LaserBeam: 발사 중)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (TitleProperty = "PatternId"))
	TArray<FBossPatternMovementOverride> PatternMovementOverrides;

	/* 회전 */

	// 페이즈 기본 회전 목표: Hold는 페이즈 시작 방향 유지 (공격 정렬 Focus가 일시적으로 우선)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotation")
	EBossPhaseRotationMode RotationMode = EBossPhaseRotationMode::FaceTarget;

	/* 조회 */

	// 이 페이즈 패턴 목록에 해당 ID가 있는지
	bool HasPattern(FName PatternId) const;

	// 패턴별 예외가 있고 bOverrideMovementMode가 켜져 있으면 해당 모드, 그 외에는 페이즈 기본 모드
	EBossPhaseMovementMode ResolveMovementMode(FName PatternId) const;

	// 패턴별 예외의 속도 배수: 예외가 없으면 1
	float ResolveActiveMoveSpeedMultiplier(FName PatternId) const;

	// 기본 이동 또는 예외 중 Waypoints 사용 여부 (현재 실행 중인 이동 모드 판단용 아님)
	bool UsesWaypoints() const;

private :

	// 패턴 ID로 이동 예외 검색
	const FBossPatternMovementOverride* FindMovementOverride(FName PatternId) const;
};

UCLASS(BlueprintType)
class MYNICELITTLECAT_API UBossPhaseConfigData : public UDataAsset
{
	GENERATED_BODY()

public :

	// 페이즈 순서, 중복 ID, 체력 하한 감소, 이동 설정 검사
	bool ValidateConfiguration(FString& OutError) const;

#if WITH_EDITOR
	// 에디터에서 저장 전/검사 시 설정 오류를 Message Log에 표시
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

public :

	// 실행 순서: CombatPhase1(0.7), Barrier70, CombatPhase2(0.3), Barrier30, CombatPhase3(0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phases", meta = (TitleProperty = "PhaseId"))
	TArray<FBossPhaseDefinition> Phases;
};
