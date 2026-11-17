// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/AI/Services/BTService_Enemy_UpdateCombatState.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"

#include "Enemy/AI/EnemyBlackboardKeys.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/EnemyCharacter.h"

UBTService_Enemy_UpdateCombatState::UBTService_Enemy_UpdateCombatState()
{
	NodeName = TEXT("Update Combat State");

	bNotifyOnSearch = true;
	bNotifyTick = true;

	Interval = 0.2f;
	RandomDeviation = 0.03f;

	// 에디터에서 맞는 타입의 키만 고를 수 있게 필터 설정
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_Enemy_UpdateCombatState, TargetKey), AActor::StaticClass());
	IsInAttackRangeKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_Enemy_UpdateCombatState, IsInAttackRangeKey));
	HasLineOfSightKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_Enemy_UpdateCombatState, HasLineOfSightKey));
	CanStartAttackKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_Enemy_UpdateCombatState, CanStartAttackKey));

	// 기존 BT 에셋은 새 속성 값이 저장되어 있지 않으므로 이 기본 이름으로 연결됨
	TargetKey.SelectedKeyName = EnemyBlackboardKeys::TargetActor;
	IsInAttackRangeKey.SelectedKeyName = EnemyBlackboardKeys::IsInAttackRange;
	HasLineOfSightKey.SelectedKeyName = EnemyBlackboardKeys::HasLineOfSight;
	CanStartAttackKey.SelectedKeyName = EnemyBlackboardKeys::CanStartAttack;
}

void UBTService_Enemy_UpdateCombatState::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* Blackboard = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*Blackboard);
		IsInAttackRangeKey.ResolveSelectedKey(*Blackboard);
		HasLineOfSightKey.ResolveSelectedKey(*Blackboard);
		CanStartAttackKey.ResolveSelectedKey(*Blackboard);
	}
}

void UBTService_Enemy_UpdateCombatState::OnSearchStart(FBehaviorTreeSearchData& SearchData)
{
	Super::OnSearchStart(SearchData);
	UpdateCombatState(SearchData.OwnerComp);
}

void UBTService_Enemy_UpdateCombatState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	UpdateCombatState(OwnerComp);
}

FString UBTService_Enemy_UpdateCombatState::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\nTarget: %s\nOut: %s, %s, %s"), *Super::GetStaticDescription(),
		*TargetKey.SelectedKeyName.ToString(), *IsInAttackRangeKey.SelectedKeyName.ToString(),
		*HasLineOfSightKey.SelectedKeyName.ToString(), *CanStartAttackKey.SelectedKeyName.ToString());
}

void UBTService_Enemy_UpdateCombatState::UpdateCombatState(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Blackboard))
	{
		return;
	}

	/* 판단 대상 */

	AAIController* AI = OwnerComp.GetAIOwner();
	AEnemyCharacter* Enemy = IsValid(AI) ? Cast<AEnemyCharacter>(AI->GetPawn()) : nullptr;
	UEnemyPatternComponent* PatternComp = IsValid(Enemy) ? Enemy->GetPatternComponent() : nullptr;

	AActor* Target = TargetKey.IsSet()
		? Cast<AActor>(Blackboard->GetValue<UBlackboardKeyType_Object>(TargetKey.GetSelectedKeyID()))
		: nullptr;

	/* 판단 값 계산: 참조가 없어도 false로 기록해 이전 값이 남지 않게 함 */

	bool bInAttackRange = false;
	bool bHasLineOfSight = false;

	if (IsValid(PatternComp) && Enemy->IsAlive() && IsValid(Target) && Target != Enemy)
	{
		bInAttackRange = PatternComp->HasPatternInRange(Target);
		bHasLineOfSight = AI->LineOfSightTo(Target);
	}

	const bool bCanStartAttack = IsValid(PatternComp) && PatternComp->HasExecutableAttack(Target);

	/* 기록: 선택하지 않은(또는 BB에 없는) 키는 건너뜀 */

	if (IsInAttackRangeKey.IsSet())
	{
		Blackboard->SetValue<UBlackboardKeyType_Bool>(IsInAttackRangeKey.GetSelectedKeyID(), bInAttackRange);
	}

	if (HasLineOfSightKey.IsSet())
	{
		Blackboard->SetValue<UBlackboardKeyType_Bool>(HasLineOfSightKey.GetSelectedKeyID(), bHasLineOfSight);
	}

	if (CanStartAttackKey.IsSet())
	{
		Blackboard->SetValue<UBlackboardKeyType_Bool>(CanStartAttackKey.GetSelectedKeyID(), bCanStartAttack);
	}
}
