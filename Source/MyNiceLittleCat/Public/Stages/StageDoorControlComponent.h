// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20. (AStageEncounterDirector에서 분리)
* 보스방 연출의 문 제어 (연출 담당자의 기본 컴포넌트)
* - 마커 Door:<번호>:Open|Close[:초], Door:<번호>:Lock|Unlock
* - 문은 연출 중에만 강제하고, 초기화·처치 때 강제를 풀어 문 자체 규칙(신호, 플레이어, 잠금)에 돌려줌
* - 플레이어에게 넘긴 문(잠금 해제 + 여닫기)은 전투 초기화 때 원래 상태로 복원
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StageDoorControlComponent.generated.h"

class AGimmickDoor;
struct FCinematicMarkerArgs;

// 플레이어에게 넘긴 문의 원래 상태 (전투 초기화 때 복원)
struct FStageDoorDefaultState
{
	bool bLocked = false;
	bool bPlayerCanToggle = false;
	bool bActivated = false;
};

UCLASS(ClassGroup = (Stage), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UStageDoorControlComponent : public UActorComponent
{
	GENERATED_BODY()

public :

	UStageDoorControlComponent();

	// 번호의 문 (없거나 비운 자리면 nullptr)
	AGimmickDoor* GetDoor(int32 DoorIndex) const;

	// 문 열기/닫기: 연출이 문 상태를 강제 (Duration이 0 이하면 즉시, 소리 없음)
	void SetDoor(int32 DoorIndex, bool bOpen, float Duration);

	// 페이즈 시작 때 닫기 (PhaseDoorCloseSeconds)
	void CloseForPhase(int32 DoorIndex) { SetDoor(DoorIndex, false, PhaseDoorCloseSeconds); }

	// 문이 열려 있는지 (없는 문은 false)
	bool IsDoorOpen(int32 DoorIndex) const;

	// 문을 플레이어에게 넘김: 연출 강제 해제(현재 위치 유지), 잠금 해제, 여닫기 허용
	void FreeDoorForPlayer(int32 DoorIndex);

	// Door 마커 처리 (bInstant: 스킵으로 한꺼번에 온 마커는 즉시 최종 상태)
	void HandleDoorMarker(const FCinematicMarkerArgs& Args, bool bInstant);

	// 전투 초기화: 넘긴 문을 원래 설정으로 되돌리고 모든 강제를 풀어 문 자체 상태로 즉시 복귀
	void RestoreForReset();

	// 보스 처치: 모든 문을 열고 연출 통제에서 돌려줌 (설정에 따라 플레이어에게 넘김)
	void ReleaseForClear();

public :

	// 스테이지 문 목록 (레벨에 배치된 AGimmickDoor)
	// - 마커 Door:<번호>의 번호가 이 배열의 인덱스: 삭제된 문 자리는 비워 두어 번호를 유지
	// - 열림 각도, 기본 시간, 소리(닫힐 때 = Deactivate Sound), 잠금, 플레이어 여닫기는 각 문에서 설정
	// - 보스 처치 후 열리는 시간은 각 문의 Mover 기본 시간 (문마다 다르게 설정 가능)
	UPROPERTY(EditAnywhere, Category = "Stage|Doors")
	TArray<TObjectPtr<AGimmickDoor>> Doors;

	// 페이즈 시작 시 문이 닫히는 시간
	UPROPERTY(EditAnywhere, Category = "Stage|Doors", meta = (ClampMin = "0.0", Units = "s"))
	float PhaseDoorCloseSeconds = 0.18f;

	// 보스 처치 후 Doors 목록의 문을 모두 플레이어에게 넘김 (잠금 해제 + 여닫기 허용)
	// - 끄면 처치 후 문이 열린 채로만 남고 플레이어 설정(잠금, 여닫기)은 그대로
	UPROPERTY(EditAnywhere, Category = "Stage|Doors")
	bool bPlayerControlsDoorsAfterClear = true;

private :

	// 플레이어에게 넘긴 문과 원래 상태 (전투 초기화 때 복원)
	TMap<TWeakObjectPtr<AGimmickDoor>, FStageDoorDefaultState> FreedDoorDefaults;
};
