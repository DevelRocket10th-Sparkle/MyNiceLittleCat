#include "Enemy/Data/EnemyPatternSetData.h"

#include "Abilities/GameplayAbility.h"

#include "Serialization/CustomVersion.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace EnemyPatternSetVersion
{
	// 저장 형식 버전: 이전 버전으로 저장된 에셋을 읽을 수 있도록 GUID 등록은 삭제하지 않음
	const FGuid Guid(0x9D3E658A, 0xAB414EE8, 0xB37D17B0, 0x13604BF2);
	constexpr int32 CommonRecovery = 1;
	constexpr int32 CommonMovement = 2;
	constexpr int32 CommonDamage = 3;
	constexpr int32 LatestVersion = CommonDamage;
	const FCustomVersionRegistration Registration(Guid, LatestVersion, TEXT("EnemyPatternSetVersion"));
}

/* FEnemyPatternDefinition */

bool FEnemyPatternDefinition::Validate(FString& OutError) const
{
	const FString Name = PatternId.ToString();

	if (PatternId.IsNone())
	{
		OutError = TEXT("PatternId is None.");
		return false;
	}

	if (!AbilityClass)
	{
		OutError = FString::Printf(TEXT("Pattern '%s': AbilityClass is not set."), *Name);
		return false;
	}

	// 거리 범위
	if (!FMath::IsFinite(MinRange) || MinRange < 0.f
		|| !FMath::IsFinite(MaxRange) || MaxRange < MinRange
		|| !FMath::IsFinite(MaxHeightDifference) || MaxHeightDifference < 0.f)
	{
		OutError = FString::Printf(TEXT("Pattern '%s': invalid range (Min %.1f, Max %.1f, Height %.1f)."),
			*Name, MinRange, MaxRange, MaxHeightDifference);
		return false;
	}

	// 가중치, 피해, 시간
	if (!FMath::IsFinite(Weight) || Weight < 0.f
		|| !FMath::IsFinite(DamageMultiplier) || DamageMultiplier < 0.f
		|| !FMath::IsFinite(RecoverySeconds) || RecoverySeconds < 0.f
		|| !FMath::IsFinite(MaxExecutionSeconds) || MaxExecutionSeconds <= 0.f)
	{
		OutError = FString::Printf(TEXT("Pattern '%s': invalid weight, damage or time values."), *Name);
		return false;
	}

	// 동시 실행 설정
	if (Threat < 0 || !FMath::IsFinite(CompanionStartDelay) || CompanionStartDelay < 0.f)
	{
		OutError = FString::Printf(TEXT("Pattern '%s': invalid Threat or CompanionStartDelay."), *Name);
		return false;
	}

	return true;
}

/* FEnemyPatternConcurrency */

bool FEnemyPatternConcurrency::Validate(FString& OutError) const
{
	if (MaxConcurrentPatterns < 1 || MaxThreat < 0
		|| !FMath::IsFinite(CompanionChance) || CompanionChance < 0.f || CompanionChance > 1.f)
	{
		OutError = TEXT("Invalid concurrency settings (MaxConcurrentPatterns >= 1, 0 <= CompanionChance <= 1, MaxThreat >= 0).");
		return false;
	}

	return true;
}

/* UEnemyPatternSetDat */

void UEnemyPatternSetData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(EnemyPatternSetVersion::Guid);
}

#if WITH_EDITOR
void UEnemyPatternSetData::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// GA별 설정의 편집 처리는 전용 객체에 위임하여 공통 에셋이 판정 방식을 알 필요가 없게 함
	for (FEnemyPatternDefinition& Pattern : Patterns)
		if (Pattern.ExecutionSettings)
		{
			Pattern.ExecutionSettings->PostEditChangeChainProperty(PropertyChangedEvent);
		}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
EDataValidationResult UEnemyPatternSetData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TSet<FName> PatternIds;

	for (const FEnemyPatternDefinition& Pattern : Patterns)
	{
		FString Error;

		// 유효하지 않은 패턴
		if (!Pattern.Validate(Error))
		{
			Context.AddError(FText::FromString(Error));
			Result = EDataValidationResult::Invalid;
		}

		// 이미 있는 패턴ID (중복)
		if (PatternIds.Contains(Pattern.PatternId))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Duplicate PatternId '%s'."), *Pattern.PatternId.ToString())));
			Result = EDataValidationResult::Invalid;
		}

		// 문제 없음
		PatternIds.Add(Pattern.PatternId);
	}

	return Result;
}
#endif
