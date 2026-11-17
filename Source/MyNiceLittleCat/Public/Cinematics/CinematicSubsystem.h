// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.16.
* 레벨 시퀀스 재생을 한 곳에서 관리하는 월드 서브시스템
* - 재생, 바인딩, 컷신 처리(입력/HUD/카메라 가림), 스킵, 제한 시간, 마커 전달, 정리를 담당
* - 요청자는 "무엇을 재생할지"만 넘기고, 마커/종료 콜백으로 자기 연출만 처리
* - 재생 중일 때만 Tick (대기 중 비용 없음)
*/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Cinematics/CinematicTypes.h"
#include "CinematicSubsystem.generated.h"

class ALevelSequenceActor;
class ULevelSequencePlayer;
class UMovieSceneSequence;
class UCameraOcclusionComponent;
class UCinematicSubsystem;

// 시퀀스 플레이어의 OnFinished/OnStop은 매개변수 없는 다이나믹 델리게이트라 어느 재생이 끝났는지 알 수 없음
// → 재생마다 작은 리스너 객체를 만들어 자기 핸들을 기억시킴
UCLASS(Transient)
class MYNICELITTLECAT_API UCinematicPlaybackListener : public UObject
{
	GENERATED_BODY()

public :

	// 리스너가 담당할 재생 핸들과 서브시스템 설정
	void Initialize(UCinematicSubsystem* InSubsystem, FCinematicHandle InHandle);

	// 마지막 프레임까지 재생 완료
	UFUNCTION()
	void HandleFinished();

	// 플레이어 정지 (정상 완료 직전에도 호출됨)
	UFUNCTION()
	void HandleStopped();

private :

	// 종료를 알릴 서브시스템
	TWeakObjectPtr<UCinematicSubsystem> Subsystem;

	// 이 리스너가 담당하는 재생
	FCinematicHandle Handle;
};

// 재생 중인 시퀀스 하나의 실행 상태
USTRUCT()
struct FCinematicPlayback
{
	GENERATED_BODY()

	// 재생 식별 핸들
	FCinematicHandle Handle;

	// 시퀀스 플레이어
	UPROPERTY()
	TObjectPtr<ULevelSequencePlayer> Player;

	// 재생용으로 생성한 임시 시퀀스 액터: 종료 후 제거
	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	// 플레이어 종료 이벤트 수신용 객체
	UPROPERTY()
	TObjectPtr<UCinematicPlaybackListener> Listener;

	// 시간순으로 정렬한 마커: 재생 시작 시 한 번만 읽음
	TArray<FCinematicMarker> Markers;

	// 다음에 전달할 마커 인덱스: 매 프레임 마커를 다시 파싱하지 않고 앞에서부터 한 번씩만 전달
	int32 NextMarkerIndex = 0;

	// 재생 범위 시작 시각 (초): 플레이어 현재 시각을 "재생 시작 기준 시각"으로 바꿀 때 사용
	double PlaybackStartSeconds = 0.0;

	// 제한 시간 계산용 시작 월드 시각
	double StartWorldSeconds = 0.0;

	// 제한 시간: 0 이하면 제한 없음
	float TimeoutSeconds = 0.f;

	// 컷신 처리(카메라 가림 정지)를 적용한 재생인지
	bool bCutscene = false;

	// 스킵 가능 여부
	bool bSkippable = false;

	// 플레이어 이벤트가 알린 종료 이유: 실제 종료 처리는 서브시스템 Tick에서 수행
	// → Play() 안에서 즉시 끝나도 요청자가 핸들을 받기 전에 콜백이 불리지 않음
	TOptional<ECinematicEndReason> PendingEndReason;

	// 요청자 콜백
	FOnCinematicMarker OnMarker;
	FOnCinematicEnded OnEnded;
};

UCLASS()
class MYNICELITTLECAT_API UCinematicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public :

	/* 재생 요청 */

	// 시퀀스 재생 시작
	// - 시작에 실패하면 무효 핸들 반환 (콜백 없음) → 호출자는 연출 없이 바로 다음 단계로 진행
	// - 콜백은 Play() 안에서 호출되지 않음 (즉시 끝나는 시퀀스도 다음 Tick에 종료 콜백)
	FCinematicHandle Play(const FCinematicRequest& Request,
		FOnCinematicMarker OnMarker = FOnCinematicMarker(),
		FOnCinematicEnded OnEnded = FOnCinematicEnded());

	// 요청자가 재생 취소 (전투 초기화 등)
	// - 남은 마커와 종료 콜백을 호출하지 않음: 호출자가 이미 자기 상태를 정리하는 중이므로 재진입 방지
	void Cancel(FCinematicHandle Handle);

	// 가장 최근에 시작한 스킵 가능한 컷신을 스킵: 스킵했으면 true
	// - 마지막 프레임을 평가한 뒤 정지하므로 KeepState 트랙(플레이어 최종 위치 등)이 유지됨
	// - 지나치지 못한 마커는 bReachedByPlayback = false로 전달
	bool TrySkipActiveCutscene();

	/* 상태 조회 */

	// 해당 재생이 아직 진행 중인지 확인
	bool IsPlaying(FCinematicHandle Handle) const;

	// 컷신이 하나라도 재생 중인지 확인
	bool IsCutscenePlaying() const { return ActiveCutsceneCount > 0; }

	/* 유틸리티 */

	// 시퀀스의 마커를 재생 범위 시작 기준 시각순으로 읽음
	static TArray<FCinematicMarker> ReadMarkers(const UMovieSceneSequence* Sequence);

	// 액터가 속한 스트리밍 레벨(레벨 인스턴스 포함)의 변환: 일반 레벨이면 Identity
	static FTransform GetLevelTransform(const AActor& Actor);

	/* 리스너 전용 */

	// 플레이어 종료 이벤트 기록: 실제 처리는 Tick에서 수행
	void NotifyPlayerEnded(FCinematicHandle Handle, bool bFinishedNaturally);

protected :

	/* UWorldSubsystem */

	// 게임 월드(PIE 포함)에서만 생성: 에디터 월드에는 만들지 않음
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 월드 종료 시 재생 중인 시퀀스와 컷신 처리를 콜백 없이 정리
	virtual void Deinitialize() override;

	/* FTickableGameObject */

	// 마커 전달, 제한 시간 검사, 종료 처리
	virtual void Tick(float DeltaTime) override;

	// 재생 중일 때만 Tick
	virtual bool IsTickable() const override { return !Playbacks.IsEmpty(); }

	virtual TStatId GetStatId() const override;

private :

	// 핸들로 재생 상태 검색: 배열이 바뀔 수 있으므로 포인터를 오래 들고 있지 않음
	FCinematicPlayback* FindPlayback(FCinematicHandle Handle);
	const FCinematicPlayback* FindPlayback(FCinematicHandle Handle) const;

	// 현재 재생 시각까지 지난 마커를 순서대로 전달
	void DispatchReachedMarkers(FCinematicHandle Handle);

	// 재생 종료: 목록에서 먼저 제거 → 정리 → 남은 마커 전달 → 종료 콜백
	void EndPlayback(FCinematicHandle Handle, ECinematicEndReason Reason);

	// 리스너 해제, 플레이어 정지, 임시 액터 제거, 컷신 처리 해제
	void ReleasePlayback(FCinematicPlayback& Playback);

	// 태그 바인딩 적용 (액터는 엔진 태그 바인딩, 컴포넌트는 바인딩 덮어쓰기)
	static void ApplyBindings(ALevelSequenceActor& SequenceActor, ULevelSequence& Sequence, const FCinematicRequest& Request);

	// 컷신 처리 시작/해제: 컷신이 겹쳐도 처음 시작과 마지막 종료에만 적용 (참조 횟수)
	void BeginCutsceneEffects();
	void EndCutsceneEffects();

private :

	// 재생 중인 시퀀스 목록 (보통 0~1개)
	UPROPERTY(Transient)
	TArray<FCinematicPlayback> Playbacks;

	// 새 핸들 발급용: 이전 번호를 재사용하지 않음
	int64 NextHandleId = 0;

	// 재생 중인 컷신 수
	int32 ActiveCutsceneCount = 0;

	// 컷신 동안 일시 정지한 플레이어 카메라 가림 컴포넌트
	TWeakObjectPtr<UCameraOcclusionComponent> SuspendedOcclusion;
};
