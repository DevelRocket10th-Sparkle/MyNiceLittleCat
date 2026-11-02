// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Datas/EnemyPhaseConfigData.h"


// 페이즈 순서, 중복 ID 및 체력 하한의 감소 여부 검사
bool UEnemyPhaseConfigData::ValidateConfiguration(FString& OutError) const
{
	// 같은 페이즈 이름이 반복되는지 확인하기 위한 집합
	TSet<FName> Ids;
	// 전투 페이즈별 체력 하한은 순서대로 감소해야 함
	float PreviousFloor = 1.f;
	if (Phases.IsEmpty() || IsLogOnlyPhase(0) || Phases[0].Kind != EBossPhaseKind::Combat)
	{
		OutError = TEXT("The first phase must be an implemented combat phase.");
		return false;
	}

	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		// 현재 검사할 페이즈 설정
		const FBossPhaseDefinition& Phase = Phases[Index];
		if (Phase.PhaseId.IsNone() || Ids.Contains(Phase.PhaseId))
		{
			OutError = TEXT("Invalid or duplicate phase ID.");
			return false;
		}
		Ids.Add(Phase.PhaseId);	// 유효한 PhaseID 추가

		// 이동 설정 검사
		if (!IsLogOnlyPhase(Index))
		{
			// 잘못된 이동 정책으로 등록된 경우 유효하지 않음 처리
			if (static_cast<uint8>(Phase.MovementMode) > static_cast<uint8>(EBossPhaseMovementMode::Waypoints))
			{
				OutError = FString::Printf(
					TEXT("Phase '%s': Invalid movement mode."),
					*Phase.PhaseId.ToString()
				);
				return false;
			}

			// 접근 허용 범위 유효성 검사
			if (!FMath::IsFinite(Phase.MovementAcceptanceRadius)
				|| Phase.MovementAcceptanceRadius < 0.f)
			{
				OutError = FString::Printf(
					TEXT("Phase '%s': Invalid movement acceptance radius."),
					*Phase.PhaseId.ToString()
				);
				return false;
			}

			// Waypoints 모드에서 사용할 경로 ID가 지정되지 않은 경우
			if (Phase.MovementMode == EBossPhaseMovementMode::Waypoints
				&& Phase.MovementRouteId.IsNone())
			{
				OutError = FString::Printf(
					TEXT("Phase '%s': MovementRouteId is None"),
					*Phase.PhaseId.ToString()
				);
				return false;
			}

			// 등록된 페이즈 회전 모드인지 검사
			if (static_cast<uint8>(Phase.RotationMode) > static_cast<uint8>(EBossPhaseRotationMode::Hold))
			{
				OutError = FString::Printf(
					TEXT("Phase '%s': Invalid rotation mode"),
					*Phase.PhaseId.ToString()
				);
				return false;
			}
		}

		// 체력 하한 검사
		if (!IsLogOnlyPhase(Index) && Phase.Kind == EBossPhaseKind::Combat)
		{
			if (!FMath::IsFinite(Phase.HealthFloorRatio) || Phase.HealthFloorRatio < 0.f
				|| Phase.HealthFloorRatio >= PreviousFloor
				|| (Phase.HealthFloorRatio == 0.f && Index != Phases.Num() - 1))
			{
				OutError = TEXT("Combat health floors must decrease and end at zero.");
				return false;
			}
			PreviousFloor = Phase.HealthFloorRatio;
		}
	}

	if (!IsLogOnlyPhase(Phases.Num() - 1)
		&& (Phases.Last().Kind != EBossPhaseKind::Combat || PreviousFloor != 0.f))
	{
		OutError = TEXT("Last combat phase must allow death.");
		return false;
	}
	OutError.Reset();
	return true;
}

// 다른 담당자의 페이즈와 기존 배리어 항목은 실제 기믹을 실행하지 않음
bool UEnemyPhaseConfigData::IsLogOnlyPhase(int32 Index) const
{
	return Phases.IsValidIndex(Index) && (Phases[Index].bLogOnly
		|| Phases[Index].Kind == EBossPhaseKind::LogOnly || (bLogOnlyAfterFirstPhase && Index > 0));
}
