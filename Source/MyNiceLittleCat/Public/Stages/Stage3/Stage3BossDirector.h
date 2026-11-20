// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.17. (AStage3BossCinematics 대체)
* 수정 일자: 2026.11.20. (저녁 조명을 시간대 시스템으로, BossEffect → Cue 마커로 통일, 엔딩 뒤 검은 화면 복구, 페이즈 눈 발광 하드코딩 제거 → bEyeGlowAtCutStart)
* Stage3 저택 보스전 연출 (AStageEncounterDirector 파생)
* - 오프닝: 도착 컷 → 대문 자물쇠 잠김 → (자물쇠를 열쇠로 풀면) 대문 열림 컷 → 마당 → (베이스) 보스 등장 컷 → 전투
* - 페이즈 전환 연출: 핏자국, 사이렌, 보스 효과, 컷 종료 후 눈 색
* - Stage3 전용 마커: Stair:Break, EyeGlow, Collapse:Player
*
* 베이스가 처리하는 것: 보스방 등록, 컷 재생, Door/Event/Cue 마커, 보스 시작 위치, 아레나 진입 대기, 문 복원/출구 개방
* 다른 곳으로 분리된 것: 시퀀스 재생(UCinematicSubsystem), 핏자국(UCinematicTrailDecalComponent),
*   눈 발광(보스 BP의 UEnemyEyeGlowComponent), 문·잠긴 문 덜컹 소리(AGimmickDoor),
*   열쇠·자물쇠 상호작용(AGimmickKeyLock), 재도전 초기화(ABossEncounterManager)
*/

#pragma once

#include "CoreMinimal.h"
#include "Stages/StageEncounterDirector.h"
#include "GameplayTagContainer.h"

#include "Stage3BossDirector.generated.h"

class AGimmickActorBase;
class AGimmickKeyLock;
class APetCharacter;
class USoundBase;
class UCinematicTrailDecalComponent;
class UEnemyEyeGlowComponent;

// Stage3 페이즈 연출 설정: 공통 설정(컷, 보스 시작점, 진입 영역, 닫을 문) + Stage3 전용 연출
USTRUCT(BlueprintType)
struct FStage3PhaseStaging : public FStagePhaseStaging
{
	GENERATED_BODY()

	// 경로 샘플 외에 추가로 찍을 핏자국 지점 (스테이지 로컬 바닥 좌표)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage3")
	TArray<FVector> BloodTrail;

	// 컷 종료 후 보스 눈 발광 색 (검은색이면 끔)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage3")
	FLinearColor EyeGlow = FLinearColor::Black;

	// 컷 시작 때 바로 눈 발광 색을 바꿈 (끄면 컷 종료나 시퀀스의 EyeGlow 마커에서 바꿈)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage3")
	bool bEyeGlowAtCutStart = false;

	// 컷 시작 시 경고 사이렌(눈 깜빡임) 재생 (켜면 눈 색도 컷 시작 때 바꿈)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Stage3")
	bool bSirenWarning = false;
};

// 베이스의 공통 페이즈 맵(Phase Staging)은 쓰지 않으므로 숨기고 Stage3 전용 Phases 사용
UCLASS(HideCategories = ("Phase Staging"))
class MYNICELITTLECAT_API AStage3BossDirector : public AStageEncounterDirector
{
	GENERATED_BODY()

public :

	// 핏자국 컴포넌트 생성
	AStage3BossDirector();

	/* IBossEncounterDirector */

	// 엔딩 컷이 있으면 처치 후 레벨 이동을 엔딩이 끝날 때까지 미룸
	virtual bool HoldsLevelAfterDefeat() const override;

protected :

	// 계단 막이, 핏자국 경로 미리 계산, 자물쇠 구독, 도착 감시 시작
	virtual void BeginPlay() override;

	/* AStageEncounterDirector */

	virtual const FStagePhaseStaging* FindPhaseStaging(FName PhaseId) const override;
	virtual void AddCutBindings(FName CutTag, FCinematicRequest& Request) const override;

	virtual void OnBossReady_Implementation(ABossCharacter* Boss) override;
	virtual bool CanBeginIntro_Implementation(APlayerCharacter* Player) const override;
	virtual void OnIntroStarting_Implementation(ABossCharacter* Boss) override;
	virtual void OnIntroFinished_Implementation() override;
	virtual void OnEncounterStarted_Implementation() override;
	virtual void OnPhaseStagingBegin_Implementation(FName PhaseId, ABossCharacter* Boss) override;
	virtual void OnPhaseStagingFinished_Implementation(FName PhaseId, ABossCharacter* Boss) override;
	virtual void OnStagePhaseStarted_Implementation(FName PhaseId, ABossCharacter* Boss) override;
	virtual bool HandleStageMarker_Implementation(const FCinematicMarker& Marker) override;
	virtual void OnCutFinished_Implementation(FName CutTag) override;
	virtual void OnStageReset_Implementation() override;
	virtual void OnStageCleared_Implementation() override;

private :

	// 오프닝 진행 단계
	enum class EOpeningStep : uint8
	{
		WaitingForArrival,	// 플레이어가 대문 근처에 오기 전
		GateLocked,			// 도착 컷 이후, 자물쇠가 풀리기 전
		Unlocking,			// 대문 열림 컷 재생 중
		Courtyard,			// 마당: 전투 시작 영역 진입 대기
		BossIntro,			// 보스 등장 컷 재생 중
		Combat				// 전투 중
	};

	/* 오프닝 */

	// 플레이어가 대문 근처에 왔는지 주기 확인 → 도착 컷
	void UpdateArrivalCheck();

	// 도착 단계 종료: 진행 기록 저장 후 대문 상태에 따라 다음 단계
	void CompleteArrival();

	// 자물쇠가 풀림(켜짐): 대문 열림 컷 (열쇠 확인, 덜컹 소리, 안내 문구는 자물쇠가 처리)
	UFUNCTION()
	void HandleGateLockActivated(AGimmickActorBase* Gimmick, bool bActivated, AActor* ChangeInstigator);

	// 대문 열림 컷 시작
	void StartUnlockCut();

	// 자물쇠가 풀려 있는지 (재도전 시에도 자물쇠에 그대로 남음, 자물쇠가 없으면 풀린 것으로 봄)
	bool IsGateUnlocked() const;

	// 전투 중 플레이어가 입구 문을 지나면 닫음
	void UpdateEntranceCheck();

	// 등장 컷 전에는 보스를 숨김
	void HideBossBeforeIntro();

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

	/* 레벨 상태 */

	// 계단 막이 설정
	void SetStairBlocked(bool bBlocked);

	// 보스의 눈 발광 컴포넌트
	UEnemyEyeGlowComponent* GetBossEyeGlow(const ABossCharacter* Boss) const;

	// 페이즈 연출 설정의 눈 발광 색 적용 (bStopWarningPulse: 사이렌 깜빡임도 멈춤)
	void ApplyPhaseEyeGlow(const ABossCharacter* Boss, const FStage3PhaseStaging& Staging, bool bStopWarningPulse) const;

protected :

	/* 컴포넌트 */

	// 보스 경로를 따라 핏자국 배치 (재질, 간격, 크기는 이 컴포넌트에서 설정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage3")
	TObjectPtr<UCinematicTrailDecalComponent> BloodTrailDecals;

	/* 페이즈 */

	// Stage3 페이즈별 연출 설정 (키: PhaseId)
	UPROPERTY(EditInstanceOnly, Category = "Stage3")
	TMap<FName, FStage3PhaseStaging> Phases;

	/* 오프닝 */

	// 도착 컷과 대문 열림 컷
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Opening")
	TObjectPtr<ULevelSequence> ArrivalSequence;

	UPROPERTY(EditInstanceOnly, Category = "Stage3|Opening")
	TObjectPtr<ULevelSequence> UnlockSequence;

	// 도착 컷을 본 뒤 저장할 진행 기록 이름: 저장되어 있으면 도착 컷 생략
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Opening")
	FName ArrivalSeenProgressFlag = TEXT("Stage3.MansionArrivalSeen");

	// 대문(또는 대문 액터) 기준 도착 판정 반경
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Opening", meta = (ClampMin = "0.0", Units = "cm"))
	float ArrivalTriggerRadius = 1200.f;

	// 대문 자물쇠 기믹: 풀리면 대문 열림 컷, 도착 판정 기준점, 컷의 자물쇠 장치
	// - 열쇠 아이템, 덜컹 소리, 안내 문구는 자물쇠(AGimmickKeyLock)에서 설정
	// - 자물쇠의 LinkedActors는 비워 둠: 대문은 컷의 Door 마커가 열어야 하므로 신호로 바로 열지 않음
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Opening")
	TObjectPtr<AGimmickKeyLock> GateLock;

	// 대문 열림 컷에서 자물쇠 장치로 연결할 컴포넌트 (GateLock 액터의 컴포넌트를 선택)
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Opening", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.StaticMeshComponent"))
	FComponentReference GateLockMechanism;

	// 대문 열림 컷에서 자물쇠 장치 바인딩에 붙인 태그
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Opening")
	FName GateLockBindingTag = TEXT("GateLockMechanism");

	/* 문 */

	// 대문 번호: 열림 컷 뒤 열어 두고, 등장 컷 뒤 닫는 문
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Doors")
	int32 GateDoorIndex = 0;

	// 전투 시작 후 플레이어가 지나가면 닫히는 입구 문 번호와 통과 판정 거리 (문 기준 스테이지 로컬 X)
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Doors")
	int32 EntranceDoorIndex = 1;

	UPROPERTY(EditInstanceOnly, Category = "Stage3|Doors", meta = (Units = "cm"))
	float EntranceCloseDistance = 260.f;

	/* 기타 레벨 연출 */

	// 마커 Stair:Break에서 사라지는 계단 막이
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Staging")
	TObjectPtr<AActor> StairBlocker;

	// 마커 Stair:Break에서 바꿀 시간대 (비우면 바꾸지 않음)
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Staging", meta = (Categories = "TimeOfDay"))
	FGameplayTag StairBreakTimeOfDay;

	// 사이렌 경고 길이. 깜빡임 색은 해당 페이즈의 EyeGlow 사용.
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Siren", meta = (ClampMin = "0.0", Units = "s"))
	float SirenSeconds = 3.f;

	/* 엔딩 */

	// 보스 처치 후 재생할 엔딩 컷 (비우면 기존처럼 사망 연출 뒤 바로 다음 레벨)
	// - 시퀀스: 보스 대역(스폰), Player/Pet 태그 바인딩, 마커 Cue:BossDust, Collapse:Player
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Ending")
	TObjectPtr<ULevelSequence> EndingSequence;

	// 엔딩 컷에서 펫을 연결할 바인딩 태그
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Ending")
	FName PetBindingTag = TEXT("Pet");

	// 엔딩 동안 이어질 곡 (이벤트 BGM: 존 볼륨보다 우선이라 컷이 마을로 넘어가도 곡 유지, 비우면 기존 곡)
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Ending")
	TObjectPtr<USoundBase> EndingBGM;

	// 엔딩 컷이 끝난 뒤(페이드 아웃 상태) 다음 레벨로 넘어가기 전 대기 시간
	UPROPERTY(EditInstanceOnly, Category = "Stage3|Ending", meta = (ClampMin = "0.0", Units = "s"))
	float EndingBlackHoldSeconds = 1.f;

private :

	// Stage3 전용 컷 태그
	static const FName ArrivalCutTag;
	static const FName UnlockCutTag;
	static const FName EndingCutTag;

	// 엔딩 컷에 연결할 펫 (엔딩 시작 시 저장, AddCutBindings가 const라 미리 찾아 둠)
	TWeakObjectPtr<APetCharacter> EndingPet;

	// 엔딩 진행 여부 (중복 시작 방지)
	bool bEndingStarted = false;

	// 엔딩 컷 종료 후 레벨 이동 대기
	FTimerHandle EndingTimer;

	// 오프닝 단계
	EOpeningStep OpeningStep = EOpeningStep::WaitingForArrival;

	// 입구 문이 이미 닫혔는지
	bool bEntranceClosed = false;

	// 주기 확인 타이머
	FTimerHandle ArrivalCheckTimer;
	FTimerHandle EntranceCheckTimer;
};
