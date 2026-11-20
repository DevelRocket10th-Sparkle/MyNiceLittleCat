// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.17.
* 수정 일자: 2026.11.20. (문은 UStageDoorControlComponent, Event·Cue 마커는 UCinematicMarkerEffectsComponent로 분리,
*            처치 후 엔딩 컷·첫 페이즈 시작 훅·등장 전 보스 숨기기를 Stage3에서 베이스로)
* 보스방 레벨 연출 담당의 공통 베이스 (레벨 배치 액터, 보스방 하나당 하나)
* - 보스방 매니저에 연출 담당자로 등록, 등장 컷 → 전투 시작, 페이즈 전환 컷 → 아레나 진입 대기 → 다음 페이즈
* - 공통 마커: Door:<번호>:…(DoorControl), Event:<이름>[:Off]·Cue:<이름>(MarkerEffects), Collapse:Player
* - 처치 후 엔딩 컷(EndingSequence): 다음 틱에 시작, 엔딩 BGM, 컷 뒤 검은 화면 유지 후 다음 레벨
* - 연출 소리·이펙트를 둘 곳 (2026.11.20. 규칙)
*   시퀀스 안에서만 나는 일회성 소리·화면 연출 → 시퀀스 트랙 (Sequencer에서 타이밍을 보며 맞춤)
*   보스·플레이어·레벨 액터에 붙거나 리셋·스킵 때 멈춰야 하는 효과 → Cue: 마커 + MarkerEffects의 CinematicCues
*   패턴·적의 효과 → 패턴 실행 설정·적 BP의 FFeedbackCue
* - 문은 연출 중에만 강제하고, 초기화·처치 때 강제를 풀어 문 자체 규칙(신호, 플레이어, 잠금)에 돌려줌
* - 페이즈가 끝난 방의 문(DoorsFreedOnPhaseEnd)과 처치 후의 문은 플레이어에게 넘김 (잠금 해제 + 여닫기), 초기화 때 원래대로
* - 스테이지마다 다른 부분은 파생 클래스(C++ 또는 BP)가 아래 확장 지점에서 처리
*
* 확장 지점 (BlueprintNativeEvent: C++ override 또는 BP 이벤트로 구현)
*   OnBossReady            보스 생성 직후
*   CanBeginIntro          등장 컷을 시작할 수 있는 단계인지
*   OnIntroStarting        등장 컷 직전
*   OnIntroFinished        등장 컷 종료 후, 전투 시작 직전 (문 닫기 등)
*   OnEncounterStarted     전투 시작 성공 후
*   OnPhaseStagingBegin    페이즈 전환 컷 시작 시 (효과, 데칼)
*   OnPhaseStagingFinished 페이즈 전환 컷 종료 시 (보스 외형)
*   OnStagePhaseStarted    첫 페이즈(전투 시작)와 다음 페이즈가 실제로 시작된 뒤
*   HandleStageMarker      공통 마커가 아닌 마커
*   OnCutFinished          스테이지가 직접 재생한 컷(태그) 종료
*   OnStageReset           전투 초기화 (문 복원 이후)
*   OnStageCleared         보스 처치 (출구 개방 이후)
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Cinematics/CinematicTypes.h"
#include "Cinematics/CinematicMarkerEffectsComponent.h"
#include "Enemy/Interfaces/BossEncounterDirector.h"
#include "Enemy/Interfaces/BossPhaseTransitionHandler.h"

#include "StageEncounterDirector.generated.h"

class ABossCharacter;
class ABossEncounterManager;
class AGimmickDoor;
class APetCharacter;
class APlayerCharacter;
class ULevelSequence;
class USceneComponent;
class USoundBase;
class UStageDoorControlComponent;

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

	// 엔딩 컷이 있으면 처치 후 레벨 이동을 엔딩이 끝날 때까지 미룸
	virtual bool HoldsLevelAfterDefeat() const override;

	/* IBossPhaseTransitionHandler */

	virtual bool BeginPhaseTransition(const FBossPhaseTransitionContext& Context) override;
	virtual void CancelPhaseTransition(int64 TransitionId) override;
	virtual bool IsWaitingForPlayer(int64 TransitionId) const override;

	/* 파생 클래스와 BP가 쓰는 도우미 */

	// 문 열기/닫기: 연출이 문 상태를 강제 (Duration이 0 이하면 즉시, 소리 없음) → DoorControl
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

	// 문 제어와 마커 효과 컴포넌트
	UStageDoorControlComponent* GetDoorControl() const { return DoorControl; }
	UCinematicMarkerEffectsComponent* GetMarkerEffects() const { return MarkerEffects; }

protected :

	// 보스방에 연출 담당자로 등록
	virtual void BeginPlay() override;

	// 재생 중인 컷과 타이머 정리
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 컴포넌트로 옮긴 예전 설정(문, 이벤트 대상, 큐)을 옮김
	virtual void PostLoad() override;

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

	// 공통 마커(Door, Event, Cue, Collapse)가 아닌 마커: 처리했으면 true
	// - Marker.bReachedByPlayback == false면 스킵으로 한꺼번에 온 마커 → 애니메이션 없이 최종 상태만
	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Events")
	bool HandleStageMarker(const FCinematicMarker& Marker);
	virtual bool HandleStageMarker_Implementation(const FCinematicMarker& Marker) { return false; }

	// 스테이지 전용 컷 종료 (엔딩 컷은 베이스가 처리)
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

	// 등장 컷 전에는 보스를 숨김 (bHideBossUntilIntro)
	void HideBossUntilIntro();

	/* 엔딩 */

	// 처치 다음 틱: 실제 보스 숨김, 펫 AI 정지, 엔딩 BGM, 엔딩 컷 재생
	void StartEnding();

	// 엔딩 컷 종료 후 검은 화면 유지 시간이 지나면 다음 레벨로
	// - 다음 레벨이 없으면 화면 페이드와 펫 AI를 되돌림 (검은 화면에 멈추지 않게)
	void FinishEnding();

	// 보스방에 엔딩 뒤 열 다음 레벨이 지정되어 있는지
	bool HasLevelAfterEnding() const;

	// 플레이어의 파트너 펫 (없으면 nullptr)
	APetCharacter* FindPartnerPet() const;

protected :

	// 공통 컷 태그: 등장, 페이즈 전환, 엔딩 (스테이지 전용 컷은 다른 이름 사용)
	static const FName IntroCutTag;
	static const FName PhaseTransitionCutTag;
	static const FName EndingCutTag;

	/* 컴포넌트 */

	// 배치용 루트: Director 위치는 동작에 영향 없음 (판정은 모두 레벨 원점 기준)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
	TObjectPtr<USceneComponent> Root;

	// 스테이지 문 목록과 Door 마커
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
	TObjectPtr<UStageDoorControlComponent> DoorControl;

	// Event·Cue 마커 (이벤트 대상, 연출 소리·이펙트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
	TObjectPtr<UCinematicMarkerEffectsComponent> MarkerEffects;

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

	// 등장 컷 전에는 보스를 숨김 (보스가 미리 생성되는 보스방에서 등장 컷 전 노출 방지)
	UPROPERTY(EditAnywhere, Category = "Stage")
	bool bHideBossUntilIntro = false;

	// 페이즈별 공통 연출 설정 (파생 구조체를 쓰는 스테이지는 숨기고 자기 맵 사용)
	UPROPERTY(EditInstanceOnly, Category = "Phase Staging")
	TMap<FName, FStagePhaseStaging> PhaseStagings;

	/* 엔딩 */

	// 보스 처치 후 재생할 엔딩 컷 (비우면 사망 연출 뒤 바로 다음 레벨)
	// - 시퀀스: 보스 대역(스폰), Player/Pet 태그 바인딩, 마커 Cue:*, Collapse:Player
	UPROPERTY(EditInstanceOnly, Category = "Stage|Ending")
	TObjectPtr<ULevelSequence> EndingSequence;

	// 엔딩 컷에서 펫을 연결할 바인딩 태그
	UPROPERTY(EditInstanceOnly, Category = "Stage|Ending")
	FName PetBindingTag = TEXT("Pet");

	// 엔딩 동안 이어질 곡 (이벤트 BGM: 존 볼륨보다 우선이라 컷이 다른 BGM 존으로 넘어가도 곡 유지, 비우면 기존 곡)
	UPROPERTY(EditInstanceOnly, Category = "Stage|Ending")
	TObjectPtr<USoundBase> EndingBGM;

	// 엔딩 컷이 끝난 뒤(페이드 아웃 상태) 다음 레벨로 넘어가기 전 대기 시간
	UPROPERTY(EditInstanceOnly, Category = "Stage|Ending", meta = (ClampMin = "0.0", Units = "s"))
	float EndingBlackHoldSeconds = 1.f;

	/* 옮기기 전 설정 (2026.11.20. 컴포넌트로 이동: 예전 레벨을 열면 PostLoad에서 옮김, 레벨을 다시 저장한 뒤 삭제) */

	UPROPERTY()
	TArray<TObjectPtr<AGimmickDoor>> Doors;

	UPROPERTY()
	float PhaseDoorCloseSeconds = 0.18f;

	UPROPERTY()
	bool bPlayerControlsDoorsAfterClear = true;

	UPROPERTY()
	TMap<FName, TObjectPtr<AActor>> EventReceivers;

	UPROPERTY()
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

	// 아레나 진입 확인 타이머
	FTimerHandle ArenaEntryTimer;

	// 등장 컷을 시작했는지 (초기화 때 다시 false)
	bool bIntroStarted = false;

	// 엔딩: 엔딩 컷에 연결하고 끝난 뒤 되돌릴 펫, 중복 시작 방지, 레벨 이동 대기
	TWeakObjectPtr<APetCharacter> EndingPet;
	bool bEndingStarted = false;
	FTimerHandle EndingTimer;
};
