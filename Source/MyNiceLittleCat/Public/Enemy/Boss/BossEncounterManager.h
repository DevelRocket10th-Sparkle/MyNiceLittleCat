// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 수정 일자: 2026.11.17. (Stage3 연출 직접 참조 제거 → IBossEncounterDirector, 재도전 자동 초기화 이동)
* 보스방 전투 진행 관리 (레벨 배치 액터)
* - 보스 생성/제거, 전투 시작, 플레이어 사망 시 대기, 전환 실패 시 재생성, 클리어
* - 등장 연출, 문, 출구처럼 스테이지마다 다른 처리는 연출 담당자(IBossEncounterDirector)에게 위임
* - 연출 담당자는 자신이 BeginPlay에서 SetEncounterDirector로 등록 (매니저는 담당자 클래스를 모름)
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Enemy/Data/BossMovementTypes.h"

#include "BossEncounterManager.generated.h"

class ABossCharacter;
class AController;
class AMainGameMode;
class AMainHUD;
class APawn;
class APlayerCharacter;
class IBossEncounterDirector;
class USceneComponent;
class USoundBase;
class UWorld;

// 보스방의 준비, 전투, 초기화, 클리어 및 재도전 대기 상태
UENUM(BlueprintType)
enum class EBossEncounterState : uint8
{
	Ready,		// 보스 생성 완료, 시작 대기
	Running,	// 전투 중
	Resetting,	// 초기화 진행 중
	Cleared,	// 보스 처치
	Waiting		// 플레이어 사망: 부활하면 초기화
};

UCLASS()
class MYNICELITTLECAT_API ABossEncounterManager : public AActor
{
	GENERATED_BODY()

public :

	// 위치 지정용 루트 생성 및 Tick 비활성화
	ABossEncounterManager();

	/* 전투 진행 */

	// 레벨 트리거의 시작 요청: 연출 담당자가 있으면 등장 연출을 거쳐, 없으면 바로 전투 시작
	UFUNCTION(BlueprintCallable, Category = "Boss")
	bool RequestEncounterStart(APlayerCharacter* Player);

	// 전달받은 플레이어를 타깃으로 첫 페이즈 시작 (등장 연출 종료 시 연출 담당자가 호출)
	UFUNCTION(BlueprintCallable, Category = "Boss")
	bool StartEncounter(APlayerCharacter* Player);

	// 보스와 기믹 정리 후 새 보스를 생성하여 체력 및 페이즈 초기화
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void ResetEncounter();

	/* 연출 담당자 */

	// 연출 담당자 등록: IBossEncounterDirector 구현 객체 (전환 담당자도 겸하면 보스에 함께 연결)
	// - 이미 생성된 보스가 있으면 즉시 OnBossSpawned 호출 → BeginPlay 순서와 무관하게 동작
	void SetEncounterDirector(UObject* Director);

	// 처치 후 다음 레벨(LevelAfterDefeat) 열기: 레벨 이동을 미룬 연출 담당자가 엔딩 연출 뒤 호출
	// - 클리어 상태가 아니거나 다음 레벨이 비어 있으면 무시
	void OpenLevelAfterDefeat();

	/* 조회 */

	ABossCharacter* GetCurrentBoss() const { return CurrentBoss; }
	EBossEncounterState GetEncounterState() const { return EncounterState; }

protected :

	// 첫 보스 생성
	virtual void BeginPlay() override;

	// 모든 콜백을 무효화하고 남아 있는 보스 및 타이머 정리
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* 보스 이벤트 */

	// 현재 보스 사망 시 클리어 처리: 새 보스를 생성하지 않음
	UFUNCTION()
	void HandleBossDefeated(ABossCharacter* Boss);

	// 사망 연출 종료 후 다음 레벨 이동
	UFUNCTION()
	void TryOpenLevelAfterDefeat(ABossCharacter* Boss);

	// 전환 실패 등의 이벤트를 받아 다음 Tick에 초기화
	UFUNCTION()
	void HandleBossFailed(ABossCharacter* Boss);

	/* 플레이어 이벤트 */

	// 사망 이벤트에서 즉시 전투를 중지하여 실패 콜백의 재생성보다 우선 처리
	UFUNCTION()
	void HandlePlayerDied();

	// 재도전 대기 중 게임모드가 플레이어를 부활시킴 (AMainGameMode::OnPlayerRespawned)
	UFUNCTION()
	void HandlePlayerRespawned(AController* Controller, APawn* NewPawn);

private :

	/* 내부 처리 */

	// 레벨 참조를 먼저 전달한 뒤 보스 생성과 초기화 확인
	bool SpawnBoss();

	// 이벤트 해제 및 전투 정리 후 현재 보스 제거
	void RemoveCurrentBoss();

	// 플레이어가 사망하거나 제거되면 보스를 유지하고 재도전 대기
	void CheckPlayer();
	void EnterWaitingState();
	void ClearEncounterPlayer();

	// 재도전 대기: 부활 감시 시작/해제
	void BeginWaitForRespawn();
	void StopWaitForRespawn();

	// 다음 Tick에 초기화 예약 (현재 콜백 스택에서 보스를 파괴하지 않음)
	void ScheduleReset();

	// 등록된 연출 담당자
	IBossEncounterDirector* GetDirector() const;

	// 보스 체력바 표시/숨김
	void ShowEncounterHealthBar();
	void HideEncounterHealthBar();

	// 보스 BGM을 BGM 서브시스템 이벤트 스택에 Push/Pop (BossBGM이 비어 있으면 Push 안 함)
	void PushBossBGM();
	void PopBossBGM();

protected :

	// 레벨 배치 위치를 제공하는 루트 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent> SceneRoot;

	/* 설정 */

	// 생성할 보스 BP 클래스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TSubclassOf<ABossCharacter> BossClass;

	// 보스 처치 후 이동할 레벨 (비우면 이동하지 않음)
	UPROPERTY(EditInstanceOnly, Category = "Boss|Clear")
	TSoftObjectPtr<UWorld> LevelAfterDefeat;

	// 보스 전투 BGM: 등장 연출이 끝나고 전투가 실제로 시작될 때(StartEncounter) 재생
	// - 이벤트 BGM 스택은 Zone보다 우선이라 아레나 존 곡을 덮어씀
	// - 처치/리셋 시 빠지면서 존·레벨 기본곡으로 자동 복귀. 비우면 BGM 변경 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|BGM")
	TObjectPtr<USoundBase> BossBGM;

	// 플레이어가 부활하면 보스방을 자동으로 초기화할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Retry")
	bool bResetWhenPlayerRespawns = true;

	// PhaseId별 레벨 기믹 액터 (배리어 등): 보스 생성마다 기믹 컴포넌트에 전달
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss|Gimmick")
	TMap<FName, TObjectPtr<AActor>> PhaseGimmickActors;

	// 기믹 영역 중심 (코인 스폰 기준 등): 비우면 기믹 쪽 기본값 사용
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss|Gimmick")
	TObjectPtr<AActor> GimmickAreaCenter;

	// Waypoints 이동 정책에서 사용할 경로
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss|Movement")
	TMap<FName, FBossMovementRoute> MovementRoutes;

	/* 상태 */

	// 이번 보스방 실행에서 생성한 보스
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<ABossCharacter> CurrentBoss;

	// 보스방의 현재 진행 상태
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss")
	EBossEncounterState EncounterState = EBossEncounterState::Resetting;

private :

	// 연출 담당자 (IBossEncounterDirector 구현 객체)
	TWeakObjectPtr<UObject> EncounterDirector;

	// 이번 전투의 단일 플레이어
	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> EncounterPlayer;

	// 부활 이벤트를 구독 중인 게임모드
	TWeakObjectPtr<AMainGameMode> RespawnGameMode;

	// 생존 확인 반복 타이머
	FTimerHandle PlayerCheckHandle;

	// 다음 Tick 초기화 예약을 취소하기 위한 타이머
	FTimerHandle ResetTimerHandle;

	// 새 전투 및 초기화 시 증가하는 세션 번호: 이전 전투의 지연 콜백 무시
	int64 EncounterGeneration = 0;

	// 보스 체력바를 표시한 HUD: 유효할 때만 사용
	TWeakObjectPtr<AMainHUD> EncounterHUD;
};
