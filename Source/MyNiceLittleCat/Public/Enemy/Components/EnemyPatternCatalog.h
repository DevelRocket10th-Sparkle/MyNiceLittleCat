// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20. (UEnemyPatternComponent에서 분리)
* 패턴 목록과 GA Spec, 자원, 선택 설정 (패턴 컴포넌트가 하나 소유)
* - 부여: 설정 검사 → 패턴마다 GA Spec 부여 → 유효 자원과 보조 실행 자원 계산
* - 선택: 지금 실행을 허용한 패턴과 가중치 재정의 (보스 페이즈 진입 때 교체)
* - 실행 상태(슬롯, 요청 번호)는 모름: 실행은 컴포넌트 담당
*/

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Enemy/Data/EnemyPatternSetData.h"
#include "EnemyPatternCatalog.generated.h"

class UAbilitySystemComponent;

USTRUCT()
struct MYNICELITTLECAT_API FEnemyPatternCatalog
{
	GENERATED_BODY()

public :

	/* 부여와 해제 */

	// 사용할 패턴 설정 지정 (부여 전에 먼저 지정, 해제해도 유지)
	void SetPatternSet(UEnemyPatternSetData* InPatternSet) { PatternSet = InPatternSet; }

	// 설정 검사 후 패턴마다 GA Spec 부여, 유효 자원 계산, 선택을 기본값(전부 허용)으로
	// - 하나라도 실패하면 이미 부여한 것도 해제하고 false
	bool GrantAbilities(UAbilitySystemComponent& InASC, UObject* SourceObject, const UObject* LogContext);

	// 부여한 GA 제거(권한이 있을 때만)와 자원·선택 정보 초기화
	void ClearAbilities(UAbilitySystemComponent* InASC, bool bHasAuthority);

	/* 선택 */

	// 모든 패턴 허용, 가중치는 패턴 기본값
	void ResetSelection();

	// 사용할 패턴과 가중치 재정의 교체 (모르는 패턴 ID는 무시)
	void SetSelection(const TArray<FEnemyPatternSelectionEntry>& Entries);

	// 현재 선택에서 실행을 허용한 패턴인지
	bool IsAvailable(FName PatternId) const { return AvailablePatterns.Contains(PatternId); }

	// 현재 선택의 유효 가중치 (재정의가 없으면 패턴 기본값)
	float GetSelectionWeight(const FEnemyPatternDefinition& Pattern) const;

	/* 조회 */

	// ID가 일치하는 설정의 주소
	const FEnemyPatternDefinition* Find(FName PatternId) const;

	// 패턴 목록 (설정이 없으면 빈 목록)
	TConstArrayView<FEnemyPatternDefinition> GetPatterns() const;

	bool HasPatterns() const { return PatternSet && !PatternSet->Patterns.IsEmpty(); }

	// 패턴 ID와 이 패턴 전용 GA Spec
	const TMap<FName, FGameplayAbilitySpecHandle>& GetHandles() const { return PatternHandles; }
	FGameplayAbilitySpecHandle GetHandle(FName PatternId) const { return PatternHandles.FindRef(PatternId); }

	// 패턴의 유효 자원: 주 실행용 또는 보조 실행용 (보조로 실행할 수 없으면 nullptr)
	const FGameplayTagContainer* GetResources(FName PatternId, bool bAsCompanion) const;

	// 보조로 실행할 수 있는 패턴인지 (보조 가능 체크 + GA가 몸을 쓰지 않음)
	bool CanRunAsCompanion(FName PatternId) const { return CompanionResources.Contains(PatternId); }

	// 주 실행 때 몸(Resource.Body)을 쓰는지
	bool UsesBody(FName PatternId) const;

	// 패턴의 유효 자원 계산: GA 기본 자원 + 패턴 추가 자원 + 몸 사용 자동 판정
	// - bAsCompanion이면 보조 실행용: 패턴 설정에 따른 몸 사용은 붙이지 않음 (보조는 몸 설정을 쓰지 않음)
	static FGameplayTagContainer ResolveResources(const FEnemyPatternDefinition& Pattern, bool bAsCompanion = false);

private :

	// 캐릭터별 공유 패턴 설정: 실행 중 원본을 변경하지 않음
	UPROPERTY(Transient)
	TObjectPtr<UEnemyPatternSetData> PatternSet;

	// 패턴 ID와 이 패턴 전용 GA Spec (같은 GA 클래스여도 패턴마다 따로 부여)
	TMap<FName, FGameplayAbilitySpecHandle> PatternHandles;

	// 패턴 ID와 유효 자원·위험 태그 (부여 때 계산)
	TMap<FName, FGameplayTagContainer> PatternResources;

	// 보조로 실행할 수 있는 패턴 ID와 보조 실행 때의 자원 (bCanRunAsCompanion이고 GA가 몸을 쓰지 않는 패턴만)
	TMap<FName, FGameplayTagContainer> CompanionResources;

	// 현재 상황에서 실행을 허용한 패턴 ID와 가중치 재정의
	TSet<FName> AvailablePatterns;
	TMap<FName, float> WeightOverrides;
};
