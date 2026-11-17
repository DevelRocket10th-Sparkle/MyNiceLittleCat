// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.17.
* 보스방 레벨 연출 담당의 공통 베이스 (레벨 배치 액터, 보스방 하나당 하나)
* - 보스방 매니저에 연출 담당자로 등록, 등장 컷 → 전투 시작, 페이즈 전환 컷 → 아레나 진입 대기 → 다음 페이즈
* - 공통 마커 처리: Door:<번호>:Open|Close[:초], Door:<번호>:Lock|Unlock, Event:<이름>[:Off], Cue:<이름>
* - Cue:<이름>: CinematicCues에 등록한 소리/이펙트 재생 (시퀀스를 다시 생성해도 레벨의 연출 설정은 유지)
* - 문은 연출 중에만 강제하고, 초기화·처치 때 강제를 풀어 문 자체 규칙(신호, 플레이어, 잠금)에 돌려줌
* - 페이즈가 끝난 방의 문(DoorsFreedOnPhaseEnd)과 처치 후의 문은 플레이어에게 넘김 (잠금 해제 + 여닫기), 초기화 때 원래대로
* - 스테이지마다 다른 부분은 파생 클래스(C++ 또는 BP)가 아래 확장 지점에서 처리
*
* 확장 지점 (BlueprintNativeEvent: C++ override 또는 BP 이벤트로 구현)
*   OnBossReady            보스 생성 직후 (등장 전 숨김 등)
*   CanBeginIntro          등장 컷을 시작할 수 있는 단계인지
*   OnIntroStarting        등장 컷 직전 (보스 보이기 등)
*   OnIntroFinished        등장 컷 종료 후, 전투 시작 직전 (문 닫기 등)
*   OnEncounterStarted     전투 시작 성공 후
*   OnPhaseStagingBegin    페이즈 전환 컷 시작 시 (효과, 데칼)
*   OnPhaseStagingFinished 페이즈 전환 컷 종료 시 (보스 외형)
*   OnStagePhaseStarted    다음 페이즈가 실제로 시작된 뒤
*   HandleStageMarker      공통 마커가 아닌 마커
*   OnCutFinished          스테이지가 직접 재생한 컷(태그) 종료
*   OnStageReset           전투 초기화 (문 복원 이후)
*   OnStageCleared         보스 처치 (출구 개방 이후)
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Cinematics/CinematicTypes.h"
#include "Enemy/Combat/EnemyFeedback.h"
#include "Enemy/Interfaces/BossEncounterDirector.h"
#include "Enemy/Interfaces/BossPhaseTransitionHandler.h"

#include "StageEncounterDirector.generated.h"

class ABossCharacter;
class ABossEncounterManager;
class AGimmickDoor;
class APlayerCharacter;
class ULevelSequence;
class USceneComponent;

// 페이즈 하나의 공통 연출 설정 (스테이지 전용 설정은 이 구조체를 상속해서 추가)
USTRUCT(BlueprintType)
struct FStagePhaseStaging
{
	GENERATED_BODY()

	// 이 페이즈로 들어가는 컷 (등장 페이즈는 보스 등장 컷)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage")
	TObjectPtr<ULevelSequence> Sequence;

	// 컷 종료 후 보스를 옮겨 둘 위치 (비우면 이동하지 않음)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage")
	TObjectPtr<AActor> BossStart;

	// 플레이어가 이 영역(스테이지 로컬 좌표)에 완전히 들어와야 다음 페이즈 시작 (크기 0이면 대기 없음)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage")
	FVector EntryCenter = FVector::ZeroVector;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage")
	FVector EntryExtent = FVector(200.f);

	// 페이즈가 시작될 때 닫을 문 번호 (INDEX_NONE이면 없음)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage")
	int32 DoorToClose = INDEX_NONE;

	// 이 페이즈가 끝나면 플레이어에게 넘길 문 번호 (이 페이즈를 치른 방의 문)
	// - 다음 페이즈로 가는 전환 컷이 끝날 때 적용 (플레이어가 다음 아레나로 이동하는 동안부터 여닫기 가능)
	// - 다음 페이즈의 DoorToClose와 같은 번호는 넘기지 않음
	// - 연출 강제를 풀되 지금 열림/닫힘은 유지, 잠금 해제, 플레이어 여닫기 허용
	// - 마지막 페이즈의 문은 보스 처치 때 함께 넘어감 (bPlayerControlsDoorsAfterClear)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage")
	TArray<int32> DoorsFreedOnPhaseEnd;
};

// 컷 마커 Cue:<이름>의 연출 기준 위치
UENUM(BlueprintType)
enum class EStageCueTarget : uint8
{
	Stage	UMETA(DisplayName = "Stage (스테이지 로컬 좌표)"),
	Boss	UMETA(DisplayName = "Boss (보스 메시)"),
	Player	UMETA(DisplayName = "Player (플레이어 메시)"),
	Actor	UMETA(DisplayName = "Actor (레벨 액터)")
};

// 컷 마커 Cue:<이름> 하나가 재생할 소리와 이펙트
USTRUCT(BlueprintType)
struct FStageCinematicCue
{
	GENERATED_BODY()

	// 재생할 소리와 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FEnemyFeedbackCue Feedback;

	// 기준 위치: 스테이지 좌표, 보스, 플레이어, 지정 액터
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	EStageCueTarget Target = EStageCueTarget::Stage;

	// Target이 Actor일 때 기준 액터 (계단 막이, 대문 등 레벨에 배치된 액터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue", meta = (EditCondition = "Target == EStageCueTarget::Actor", EditConditionHides))
	TObjectPtr<AActor> TargetActor;

	// Target이 Boss/Player일 때 붙일 메시 소켓 (None이면 메시 원점 = 발밑)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FName Socket;

	// Stage: 스테이지 로컬 좌표 / 그 외: 기준 위치에서 기준의 로컬 방향으로 더할 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FVector Offset = FVector::ZeroVector;

	// 기준을 따라 움직이도록 붙여서 재생 (Stage 기준은 무시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	bool bAttach = false;

	// 붙여서 재생한 반복 연출을 끌 시간 (0이면 이펙트가 스스로 끝날 때까지, 초기화/처치/종료 때는 항상 정지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bAttach"))
	float StopAfterSeconds = 0.f;

	// 스킵으로 한꺼번에 전달될 때도 재생할지 (기본: 지나간 연출은 재생하지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	bool bPlayWhenSkipped = false;
};

// 스테이지가 플레이어에게 넘긴 문의 원래 상태 (전투 초기화 때 복원)
struct FStageDoorDefaultState
{
	bool bLocked = false;
	bool bPlayerCanToggle = false;
	bool bActivated = false;
};

UCLASS(Abstract, Blueprintable)
class MYNICELITTLECAT_API AStageEncounterDirector : public AActor,
	public IBossEncounterDirector, public IBossPhaseTransitionHandler
{
	GENERATED_BODY()

public :

	// Tick 없음: 주기 확인은 필요한 단계에서만 타이머로
	AStageEncounterDirector();

	/* IBossEncounterDirector: 파생 클래스는 아래 확장 지점을 사용 */

	virtual void OnBossSpawned(ABossCharacter* Boss) override;
	virtual bool TryBeginEncounterIntro(ABossEncounterManager& Manager, APlayerCharacter* Player) override;
	virtual void OnEncounterReset() override;
	virtual void OnEncounterCleared() override;

	/* IBossPhaseTransitionHandler */

	virtual bool BeginPhaseTransition(const FBossPhaseTransitionContext& Context) override;
	virtual void CancelPhaseTransition(int64 TransitionId) override;

	/* 파생 클래스와 BP가 쓰는 도우미 */

	// 문 열기/닫기: 연출이 문 상태를 강제 (Duration이 0 이하면 즉시, 소리 없음)
	UFUNCTION(BlueprintCallable, Category = "Stage|Doors")
	void SetDoor(int32 DoorIndex, bool bOpen, float Duration);

	// 문이 열려 있는지 (없는 문은 false)
	UFUNCTION(BlueprintPure, Category = "Stage|Doors")
	bool IsDoorOpen(int32 DoorIndex) const;

	// 문을 플레이어에게 넘김: 연출 강제 해제(현재 위치 유지), 잠금 해제, 여닫기 허용
	// - 전투 초기화 때 원래 상태(잠금, 여닫기, 열림)로 되돌림
	UFUNCTION(BlueprintCallable, Category = "Stage|Doors")
	void FreeDoorForPlayer(int32 DoorIndex);

	// 스테이지 전용 컷 재생: 끝나면 OnCutFinished(CutTag) 호출 (재생할 수 없어도 바로 호출)
	UFUNCTION(BlueprintCallable, Category = "Stage|Cinematic")
	void PlayStageCut(ULevelSequence* Sequence, FName CutTag, float TimeoutSeconds = 0.f);

	// 컷 재생 중인지
	UFUNCTION(BlueprintPure, Category = "Stage|Cinematic")
	bool IsCutPlaying() const { return ActiveCut.IsValid(); }

	// 살아 있는 플레이어 (없으면 nullptr)
	UFUNCTION(BlueprintPure, Category = "Stage")
	APlayerCharacter* GetAlivePlayer() const;

	// 현재 보스 (없으면 nullptr)
	UFUNCTION(BlueprintPure, Category = "Stage")
	ABossCharacter* GetCurrentBoss() const;

	// 스테이지(레벨 인스턴스) 변환: 시퀀스 키와 진입 영역의 좌표계
	FTransform GetStageTransform() const;

protected :

	// 보스방에 연출 담당자로 등록
	virtual void BeginPlay() override;

	// 재생 중인 컷과 타이머 정리
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* 페이즈 설정 조회 */

	// 페이즈 연출 설정 조회: 기본은 PhaseStagings, 파생 구조체를 쓰는 스테이지는 override
	virtual const FStagePhaseStaging* FindPhaseStaging(FName PhaseId) const;

	// 컷 요청에 스테이지 전용 바인딩 추가 (예: 대문 자물쇠 컴포넌트)
	virtual void AddCutBindings(FName CutTag, FCinematicRequest& Request) const {}

	/* 확장 지점 */

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnBossReady(ABossCharacter* Boss);
	virtual void OnBossReady_Implementation(ABossCharacter* Boss) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	bool CanBeginIntro(APlayerCharacter* Player) const;
	virtual bool CanBeginIntro_Implementation(APlayerCharacter* Player) const { return true; }

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnIntroStarting(ABossCharacter* Boss);
	virtual void OnIntroStarting_Implementation(ABossCharacter* Boss) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnIntroFinished();
	virtual void OnIntroFinished_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnEncounterStarted();
	virtual void OnEncounterStarted_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnPhaseStagingBegin(FName PhaseId, ABossCharacter* Boss);
	virtual void OnPhaseStagingBegin_Implementation(FName PhaseId, ABossCharacter* Boss) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnPhaseStagingFinished(FName PhaseId, ABossCharacter* Boss);
	virtual void OnPhaseStagingFinished_Implementation(FName PhaseId, ABossCharacter* Boss) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnStagePhaseStarted(FName PhaseId, ABossCharacter* Boss);
	virtual void OnStagePhaseStarted_Implementation(FName PhaseId, ABossCharacter* Boss) {}

	// 공통 마커(Door, Event)가 아닌 마커: 처리했으면 true
	// - Marker.bReachedByPlayback == false면 스킵으로 한꺼번에 온 마커 → 애니메이션 없이 최종 상태만
	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	bool HandleStageMarker(const FCinematicMarker& Marker);
	virtual bool HandleStageMarker_Implementation(const FCinematicMarker& Marker) { return false; }

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnCutFinished(FName CutTag);
	virtual void OnCutFinished_Implementation(FName CutTag) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnStageReset();
	virtual void OnStageReset_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	void OnStageCleared();
	virtual void OnStageCleared_Implementation() {}

private :

	/* 컷 */

	// 컷 재생 공통 처리: 보스/플레이어 바인딩, 레벨 원점, 컷신 설정
	void PlayCutInternal(ULevelSequence* Sequence, FName CutTag, float TimeoutSeconds);

	// 재생 중인 컷 취소 (콜백 없음)
	void CancelActiveCut();

	// 마커 → 공통 처리 또는 파생 클래스
	void HandleCutMarker(FCinematicHandle Handle, const FCinematicMarker& Marker);

	// 컷 종료 → 태그별 다음 단계
	void HandleCutEnded(FCinematicHandle Handle, ECinematicEndReason Reason);

	// 태그별 다음 단계 실행 (컷 재생 실패 시에도 사용)
	void CompleteCut(FName CutTag);

	/* 등장과 페이즈 전환 */

	// 등장 컷 종료: 전투 시작
	void CompleteIntro();

	// 전환 컷 종료: 보스 위치, 아레나 진입 대기 시작
	void FinishPhaseStaging();

	// 아레나 진입 주기 확인 → 보스에 전환 연출 종료 통지
	void UpdateArenaEntryCheck();

	// 플레이어 캡슐이 진입 영역 안에 완전히 들어왔는지
	bool HasPlayerEnteredArena(const FStagePhaseStaging& Staging) const;

	// 진행 중인 전환 상태 초기화
	void ClearActiveTransition();

	/* 공통 마커 */

	void HandleDoorMarker(const FCinematicMarkerArgs& Args, bool bInstant);
	void HandleEventMarker(const FCinematicMarkerArgs& Args);
	void HandleCueMarker(const FCinematicMarkerArgs& Args, bool bReachedByPlayback);

	// 붙여서 재생 중인 컷 연출 정지 (초기화, 처치, 종료)
	void StopCinematicCues();

protected :

	// 공통 컷 태그: 등장, 페이즈 전환 (스테이지 전용 컷은 다른 이름 사용)
	static const FName IntroCutTag;
	static const FName PhaseTransitionCutTag;

	/* 컴포넌트 */

	// 배치용 루트: Director 위치는 동작에 영향 없음 (판정은 모두 레벨 원점 기준)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
	TObjectPtr<USceneComponent> Root;

	/* 보스방 */

	// 연출을 담당할 보스방: BeginPlay에서 이 매니저에 자신을 등록
	UPROPERTY(EditInstanceOnly, Category = "Stage")
	TObjectPtr<ABossEncounterManager> Encounter;

	// 보스 등장 컷을 가진 페이즈 (페이즈 연출 설정의 키)
	UPROPERTY(EditInstanceOnly, Category = "Stage")
	FName IntroPhaseId = TEXT("CombatPhase1");

	// 시퀀스에서 플레이어를 연결할 바인딩 태그 (보스 태그는 보스 BP에서 설정)
	UPROPERTY(EditInstanceOnly, Category = "Stage")
	FName PlayerBindingTag = TEXT("Player");

	// 페이즈별 공통 연출 설정 (파생 구조체를 쓰는 스테이지는 숨기고 자기 맵 사용)
	UPROPERTY(EditInstanceOnly, Category = "Phase Staging")
	TMap<FName, FStagePhaseStaging> PhaseStagings;

	/* 문 */

	// 스테이지 문 목록 (레벨에 배치된 AGimmickDoor)
	// - 마커 Door:<번호>의 번호가 이 배열의 인덱스: 삭제된 문 자리는 비워 두어 번호를 유지
	// - 열림 각도, 기본 시간, 소리(닫힐 때 = Deactivate Sound), 잠금, 플레이어 여닫기는 각 문에서 설정
	// - 보스 처치 후 열리는 시간은 각 문의 Mover 기본 시간 (문마다 다르게 설정 가능)
	UPROPERTY(EditInstanceOnly, Category = "Stage|Doors")
	TArray<TObjectPtr<AGimmickDoor>> Doors;

	// 페이즈 시작 시 문이 닫히는 시간
	UPROPERTY(EditInstanceOnly, Category = "Stage|Doors", meta = (ClampMin = "0.0", Units = "s"))
	float PhaseDoorCloseSeconds = 0.18f;

	// 보스 처치 후 Doors 목록의 문을 모두 플레이어에게 넘김 (잠금 해제 + 여닫기 허용)
	// - 끄면 처치 후 문이 열린 채로만 남고 플레이어 설정(잠금, 여닫기)은 그대로
	UPROPERTY(EditInstanceOnly, Category = "Stage|Doors")
	bool bPlayerControlsDoorsAfterClear = true;

	/* 마커 이벤트 */

	// 마커 Event:<이름>을 받을 액터: IActivatableInterface 구현 액터 (기믹, 또는 인터페이스를 추가한 BP)
	// - Event:<이름> → Activate, Event:<이름>:Off → Deactivate
	// - 스킵하면 종료 시점에 한꺼번에 전달되므로 받는 쪽은 여러 번 와도 안전해야 함 (기믹은 같은 상태면 무시)
	UPROPERTY(EditInstanceOnly, Category = "Stage|Cinematic")
	TMap<FName, TObjectPtr<AActor>> EventReceivers;

	// 마커 Cue:<이름>이 재생할 소리와 이펙트 (키: 이름)
	// - 시퀀스에는 마커만 두고 에셋은 여기서 지정: 시퀀스를 다시 생성해도 연출 설정 유지
	// - 등록하지 않은 이름의 마커는 무시 (자리만 만들어 둔 마커)
	UPROPERTY(EditInstanceOnly, Category = "Stage|Cinematic")
	TMap<FName, FStageCinematicCue> CinematicCues;

private :

	// 현재 보스
	TWeakObjectPtr<ABossCharacter> CurrentBoss;

	// 재생 중인 컷과 태그
	FCinematicHandle ActiveCut;
	FName ActiveCutTag;

	// 진행 중인 페이즈 전환 (0이면 없음)
	int64 ActiveTransitionId = 0;
	FName ActiveTransitionPhaseId;

	// 진행 중인 전환에서 끝나는 페이즈 (그 방의 문을 다음 페이즈 시작 때 넘김)
	FName ActiveTransitionFromPhaseId;

	// 플레이어에게 넘긴 문과 원래 상태 (전투 초기화 때 복원)
	TMap<TWeakObjectPtr<AGimmickDoor>, FStageDoorDefaultState> FreedDoorDefaults;

	// 아레나 진입 확인 타이머
	FTimerHandle ArenaEntryTimer;

	// 붙여서 재생 중인 컷 연출
	TArray<FEnemyFeedbackInstance> ActiveCueInstances;
};
