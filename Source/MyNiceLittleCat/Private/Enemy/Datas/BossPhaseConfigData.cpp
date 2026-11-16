// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Datas/BossPhaseConfigData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

/* FBossPhaseDefinition */

const FBossPatternMovementOverride* FBossPhaseDefinition::FindMovementOverride(FName PatternId) const
{
	if (PatternId.IsNone())
	{
		return nullptr;
	}

	return PatternMovementOverrides.FindByPredicate(
		[PatternId](const FBossPatternMovementOverride& Entry)
		{
			return Entry.PatternId == PatternId;
		});
}

EBossPhaseMovementMode FBossPhaseDefinition::ResolveMovementMode(FName PatternId) const
{
	const FBossPatternMovementOverride* Override = FindMovementOverride(PatternId);
	return Override && Override->bOverrideMovementMode ? Override->MovementMode : MovementMode;
}

float FBossPhaseDefinition::ResolveActiveMoveSpeedMultiplier(FName PatternId) const
{
	const FBossPatternMovementOverride* Override = FindMovementOverride(PatternId);
	return Override ? Override->ActiveMoveSpeedMultiplier : 1.f;
}

bool FBossPhaseDefinition::UsesWaypoints() const
{
	return MovementMode == EBossPhaseMovementMode::Waypoints
		|| PatternMovementOverrides.ContainsByPredicate(
			[](const FBossPatternMovementOverride& Entry)
			{
				return Entry.bOverrideMovementMode && Entry.MovementMode == EBossPhaseMovementMode::Waypoints;
			});
}

/* UBossPhaseConfigData */

// 페이즈 순서, 중복 ID, 체력 하한 감소, 이동 설정 검사
bool UBossPhaseConfigData::ValidateConfiguration(FString& OutError) const
{
	OutError.Reset();

	// 첫 페이즈는 체력으로 진행하는 전투여야 함
	if (Phases.IsEmpty() || Phases[0].Kind != EBossPhaseKind::Combat)
	{
		OutError = TEXT("The first phase must be a combat phase.");
		return false;
	}

	// 같은 페이즈 이름이 반복되는지 확인하기 위한 집합
	TSet<FName> PhaseIds;

	// 전투 페이즈의 체력 하한은 순서대로 감소해야 함
	float PreviousFloor = 1.f;

	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		const FBossPhaseDefinition& Phase = Phases[Index];
		const FString PhaseName = Phase.PhaseId.ToString();

		/* ID */

		if (Phase.PhaseId.IsNone() || PhaseIds.Contains(Phase.PhaseId))
		{
			OutError = FString::Printf(TEXT("Phase [%d]: invalid or duplicate phase ID '%s'."), Index, *PhaseName);
			return false;
		}
		PhaseIds.Add(Phase.PhaseId);

		/* 이동 */

		if (!FMath::IsFinite(Phase.MovementAcceptanceRadius) || Phase.MovementAcceptanceRadius < 0.f)
		{
			OutError = FString::Printf(TEXT("Phase '%s': invalid movement acceptance radius."), *PhaseName);
			return false;
		}

		if (Phase.UsesWaypoints() && Phase.MovementRouteId.IsNone())
		{
			OutError = FString::Printf(TEXT("Phase '%s': MovementRouteId is None."), *PhaseName);
			return false;
		}

		// 패턴별 이동 예외: 중복 없이 이 페이즈에서 허용한 패턴만
		TSet<FName> OverrideIds;
		for (const FBossPatternMovementOverride& Override : Phase.PatternMovementOverrides)
		{
			if (Override.PatternId.IsNone()
				|| OverrideIds.Contains(Override.PatternId)
				|| !Phase.AllowedPatternIds.Contains(Override.PatternId))
			{
				OutError = FString::Printf(TEXT("Phase '%s': invalid movement override '%s'."),
					*PhaseName, *Override.PatternId.ToString());
				return false;
			}
			OverrideIds.Add(Override.PatternId);
		}

		/* 전환 */

		if (Phase.bUseTransitionSequence && (!FMath::IsFinite(Phase.TransitionTimeout) || Phase.TransitionTimeout <= 0.f))
		{
			OutError = FString::Printf(TEXT("Phase '%s': TransitionTimeout must be positive."), *PhaseName);
			return false;
		}

		/* 체력 하한 */

		if (Phase.Kind != EBossPhaseKind::Combat)
		{
			continue;
		}

		const bool bLastPhase = Index == Phases.Num() - 1;
		if (!FMath::IsFinite(Phase.HealthFloorRatio) || Phase.HealthFloorRatio < 0.f
			|| Phase.HealthFloorRatio >= PreviousFloor
			|| (Phase.HealthFloorRatio == 0.f && !bLastPhase))
		{
			OutError = FString::Printf(TEXT("Phase '%s': combat health floors must decrease and end at zero."), *PhaseName);
			return false;
		}
		PreviousFloor = Phase.HealthFloorRatio;
	}

	// 마지막 페이즈는 사망을 허용하는 전투여야 함
	if (Phases.Last().Kind != EBossPhaseKind::Combat || PreviousFloor != 0.f)
	{
		OutError = TEXT("The last phase must be a combat phase that allows death (HealthFloorRatio = 0).");
		return false;
	}

	return true;
}

#if WITH_EDITOR
EDataValidationResult UBossPhaseConfigData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 런타임 검사와 같은 규칙을 에디터 저장/검사 시점에도 보여줌 → PIE를 돌리기 전에 오류 발견
	FString Error;
	if (!ValidateConfiguration(Error))
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
