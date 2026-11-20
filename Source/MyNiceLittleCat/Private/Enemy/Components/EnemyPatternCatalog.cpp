// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Components/EnemyPatternCatalog.h"
#include "Enemy/EnemyLog.h"

#include "AbilitySystemComponent.h"
#include "Enemy/GAS/EnemyCombatTags.h"
#include "Enemy/GAS/Abilities/EnemyPatternAbility.h"

/* 부여와 해제 */

bool FEnemyPatternCatalog::GrantAbilities(UAbilitySystemComponent& InASC, UObject* SourceObject, const UObject* LogContext)
{
	if (!PatternSet)
	{
		return false;
	}

	/* 1) 부여 전에 전체 설정 검사: 하나라도 잘못되면 아무것도 부여하지 않음 */

	FString ConfigError;
	if (!PatternSet->ValidateConfiguration(ConfigError))
	{
		UE_LOG(LogEnemy, Error, TEXT("[EnemyPattern] %s: invalid pattern settings. %s"), *GetNameSafe(PatternSet), *ConfigError);
		return false;
	}

	/* 2) 패턴마다 GA Spec 부여 */

	// 같은 GA 클래스라도 패턴마다 따로 부여: InstancedPerActor는 Spec마다 인스턴스가 하나라,
	// Spec을 공유하면 같은 클래스의 두 패턴이 동시에 실행될 수 없고 종료 이벤트도 구분할 수 없음
	for (const FEnemyPatternDefinition& Pattern : PatternSet->Patterns)
	{
		// SourceObject = 패턴 컴포넌트: GA 디버깅 시 부여 출처 확인용
		const FGameplayAbilitySpecHandle Handle = InASC.GiveAbility(FGameplayAbilitySpec(Pattern.AbilityClass, 1, INDEX_NONE, SourceObject));
		if (!Handle.IsValid())
		{
			ClearAbilities(&InASC, true);
			return false;
		}

		PatternHandles.Add(Pattern.PatternId, Handle);
		PatternResources.Add(Pattern.PatternId, ResolveResources(Pattern));

		// 보조 가능 패턴: GA 클래스 자체가 몸을 쓰면 체크해도 보조로 쓰지 않음 (근접, 돌진, 레이저 빔)
		if (Pattern.bCanRunAsCompanion)
		{
			FGameplayTagContainer AsCompanion = ResolveResources(Pattern, true);
			if (AsCompanion.HasTag(EnemyCombatTags::PatternResourceBody))
			{
				UE_LOG(LogEnemy, Warning, TEXT("[EnemyPattern] %s: '%s' uses the body by its ability class and cannot run as a companion"),
					*GetNameSafe(LogContext), *Pattern.PatternId.ToString());
			}
			else
			{
				CompanionResources.Add(Pattern.PatternId, MoveTemp(AsCompanion));
			}
		}
	}

	// 기본 선택: 모든 패턴 허용, 가중치는 패턴 기본값
	ResetSelection();
	return true;
}

void FEnemyPatternCatalog::ClearAbilities(UAbilitySystemComponent* InASC, bool bHasAuthority)
{
	if (InASC && bHasAuthority)
	{
		// 패턴마다 Spec이 따로라 중복 없이 하나씩 제거
		for (const TPair<FName, FGameplayAbilitySpecHandle>& Pair : PatternHandles)
		{
			InASC->ClearAbility(Pair.Value);
		}
	}

	PatternHandles.Reset();
	PatternResources.Reset();
	CompanionResources.Reset();
	AvailablePatterns.Reset();
	WeightOverrides.Reset();
}

/* 선택 */

void FEnemyPatternCatalog::ResetSelection()
{
	AvailablePatterns.Reset();
	WeightOverrides.Reset();

	for (const FEnemyPatternDefinition& Pattern : GetPatterns())
	{
		AvailablePatterns.Add(Pattern.PatternId);
	}
}

void FEnemyPatternCatalog::SetSelection(const TArray<FEnemyPatternSelectionEntry>& Entries)
{
	AvailablePatterns.Reset();
	WeightOverrides.Reset();

	for (const FEnemyPatternSelectionEntry& Entry : Entries)
	{
		if (!Find(Entry.PatternId))
		{
			continue;
		}

		AvailablePatterns.Add(Entry.PatternId);

		// 재정의가 없으면 기록하지 않음 → 패턴 기본 Weight 사용
		if (Entry.bOverrideWeight)
		{
			WeightOverrides.Add(Entry.PatternId, Entry.Weight);
		}
	}
}

float FEnemyPatternCatalog::GetSelectionWeight(const FEnemyPatternDefinition& Pattern) const
{
	const float* Override = WeightOverrides.Find(Pattern.PatternId);
	return Override ? *Override : Pattern.Weight;
}

/* 조회 */

const FEnemyPatternDefinition* FEnemyPatternCatalog::Find(FName PatternId) const
{
	if (!PatternSet)
	{
		return nullptr;
	}

	return PatternSet->Patterns.FindByPredicate(
		[PatternId](const FEnemyPatternDefinition& Pattern)
		{
			return Pattern.PatternId == PatternId;
		});
}

TConstArrayView<FEnemyPatternDefinition> FEnemyPatternCatalog::GetPatterns() const
{
	return PatternSet ? TConstArrayView<FEnemyPatternDefinition>(PatternSet->Patterns) : TConstArrayView<FEnemyPatternDefinition>();
}

const FGameplayTagContainer* FEnemyPatternCatalog::GetResources(FName PatternId, bool bAsCompanion) const
{
	return bAsCompanion ? CompanionResources.Find(PatternId) : PatternResources.Find(PatternId);
}

bool FEnemyPatternCatalog::UsesBody(FName PatternId) const
{
	const FGameplayTagContainer* Resources = PatternResources.Find(PatternId);
	return Resources && Resources->HasTag(EnemyCombatTags::PatternResourceBody);
}

FGameplayTagContainer FEnemyPatternCatalog::ResolveResources(const FEnemyPatternDefinition& Pattern, bool bAsCompanion)
{
	FGameplayTagContainer Resources = Pattern.Resources;

	// GA 클래스 기본 자원: 근접, 돌진처럼 클래스 자체가 몸을 쓰는 경우 페이즈 설정자가 따로 적지 않아도 됨
	if (const UEnemyPatternAbility* AbilityDefaults = Cast<UEnemyPatternAbility>(Pattern.AbilityClass->GetDefaultObject()))
	{
		Resources.AppendTags(AbilityDefaults->GetDefaultPatternResources());
	}

	// 패턴 설정이 몸을 제어하면 자동으로 몸 점유 (회전 잠금, 시작 정렬, 정지/GA 이동)
	// - 보조 실행은 이 설정들을 끄고 실행하므로(AddSlot) 붙이지 않음
	const bool bControlsBody = !bAsCompanion
		&& (Pattern.bLockRotation || Pattern.bFaceTargetOnStart
			|| Pattern.Movement.Policy != EEnemyPatternMovementPolicy::FollowLocomotion);

	if (bControlsBody)
	{
		Resources.AddTag(EnemyCombatTags::PatternResourceBody);
	}

	return Resources;
}
