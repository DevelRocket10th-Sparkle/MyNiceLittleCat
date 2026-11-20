// Fill out your copyright notice in the Description page of Project Settings.


#include "Stages/StageDoorControlComponent.h"

#include "Cinematics/CinematicTypes.h"
#include "Gimmicks/Stage/GimmickDoor.h"

namespace
{
	// 문 마커에 시간이 없을 때 기본 회전 시간
	constexpr float DefaultDoorOpenSeconds = 0.85f;
	constexpr float DefaultDoorCloseSeconds = 0.18f;
}

UStageDoorControlComponent::UStageDoorControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AGimmickDoor* UStageDoorControlComponent::GetDoor(int32 DoorIndex) const
{
	return Doors.IsValidIndex(DoorIndex) ? Doors[DoorIndex].Get() : nullptr;
}

void UStageDoorControlComponent::SetDoor(int32 DoorIndex, bool bOpen, float Duration)
{
	AGimmickDoor* Door = GetDoor(DoorIndex);
	if (!Door)
	{
		return;
	}

	// 연출이 문 상태를 강제: 전투 초기화 전까지 신호·플레이어 조작보다 우선
	// - 시간 0 이하(스킵, 초기화)는 즉시 이동: 문 효과음 없음
	const bool bInstant = Duration <= 0.f;
	Door->ForceDoorState(bOpen, Duration, bInstant);
}

bool UStageDoorControlComponent::IsDoorOpen(int32 DoorIndex) const
{
	const AGimmickDoor* Door = GetDoor(DoorIndex);
	return Door && Door->IsDoorOpen();
}

void UStageDoorControlComponent::FreeDoorForPlayer(int32 DoorIndex)
{
	AGimmickDoor* Door = GetDoor(DoorIndex);
	if (!Door)
	{
		return;
	}

	// 처음 넘길 때의 설정만 기록 (두 번 넘겨도 원래 상태 유지)
	if (!FreedDoorDefaults.Contains(Door))
	{
		FStageDoorDefaultState& Saved = FreedDoorDefaults.Add(Door);
		Saved.bLocked = Door->IsLocked();
		Saved.bPlayerCanToggle = Door->CanPlayerToggle();
		Saved.bActivated = Door->IsActivated();
	}

	// 신호 상태를 지금 위치에 맞춘 뒤 강제를 풀어 문이 움직이지 않게 함
	Door->SetActivated(Door->IsDoorOpen(), GetOwner());
	Door->ReleaseForcedDoorState();
	Door->SetLocked(false, GetOwner());
	Door->SetPlayerCanToggle(true);
}

// Door:<번호>:Open|Close[:초] → 연출 강제 이동
// Door:<번호>:Lock|Unlock     → 플레이어 상호작용 잠금 (스킵돼도 최종 잠금 상태가 남음)
void UStageDoorControlComponent::HandleDoorMarker(const FCinematicMarkerArgs& Args, bool bInstant)
{
	int32 DoorIndex = INDEX_NONE;
	if (!Args.TryGetInt(0, DoorIndex))
	{
		return;
	}

	AGimmickDoor* Door = GetDoor(DoorIndex);
	if (!Door)
	{
		return;
	}

	// Lock|Unlock: 플레이어 상호작용만 막거나 풂 (문 위치는 그대로)
	if (Args.ArgEquals(1, TEXT("Lock")) || Args.ArgEquals(1, TEXT("Unlock")))
	{
		Door->SetLocked(Args.ArgEquals(1, TEXT("Lock")), GetOwner());
		return;
	}

	// Open|Close: 연출 강제 이동
	const bool bOpen = Args.ArgEquals(1, TEXT("Open"));
	const float Duration = Args.GetFloat(2, bOpen ? DefaultDoorOpenSeconds : DefaultDoorCloseSeconds);
	SetDoor(DoorIndex, bOpen, bInstant ? 0.f : Duration);
}

void UStageDoorControlComponent::RestoreForReset()
{
	// 플레이어에게 넘겼던 문을 원래 설정으로 복원 (열림 상태도 되돌린 뒤 아래에서 강제를 풀어 그 위치로 이동)
	for (const TPair<TWeakObjectPtr<AGimmickDoor>, FStageDoorDefaultState>& Pair : FreedDoorDefaults)
	{
		if (AGimmickDoor* Door = Pair.Key.Get())
		{
			Door->SetLocked(Pair.Value.bLocked, GetOwner());
			Door->SetPlayerCanToggle(Pair.Value.bPlayerCanToggle);
			Door->SetActivated(Pair.Value.bActivated, GetOwner());
		}
	}
	FreedDoorDefaults.Reset();

	// 연출이 강제한 문 상태를 풀어 문 자체 상태(신호 기준, 기본 닫힘)로 즉시 복귀
	for (AGimmickDoor* Door : Doors)
	{
		if (Door)
		{
			Door->ReleaseForcedDoorState(true);
		}
	}
}

void UStageDoorControlComponent::ReleaseForClear()
{
	// 처치 후 모든 문을 열고 연출 통제에서 돌려줌
	// - 문 자체 상태(신호)를 열림으로 바꾼 뒤 강제를 풀면, 열린 채로 이후에는 신호·플레이어 조작을 따름
	//   (예: 옥상 문은 다시 플레이어가 여닫을 수 있음)
	// - 강제한 채 두면 클리어 후에도 플레이어가 문을 조작할 수 없음
	for (AGimmickDoor* Door : Doors)
	{
		if (Door)
		{
			Door->SetActivated(true, GetOwner());
			Door->ReleaseForcedDoorState();

			// 처치 후에는 잠긴 문(대문 등)도 플레이어가 여닫을 수 있게 넘김
			if (bPlayerControlsDoorsAfterClear)
			{
				Door->SetLocked(false, GetOwner());
				Door->SetPlayerCanToggle(true);
			}
		}
	}

	// 처치 후에는 초기화가 없으므로 복원 기록 정리
	FreedDoorDefaults.Reset();
}
