// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/AI/Services/BTService_Enemy_UpdateCombatState.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "Enemy/EnemyCharacter.h"
#include "Enemy/Datas/EnemyConfigData.h"

namespace EnemyCombatKeys
{
	const FName TargetActor(TEXT("TargetActor"));
	const FName IsInAttackRange(TEXT("IsInAttackRange"));
	const FName HasLineOfSight(TEXT("HasLineOfSight"));
}

UBTService_Enemy_UpdateCombatState::UBTService_Enemy_UpdateCombatState()
{
	NodeName = TEXT("Update Combat State");

	bNotifyOnSearch = true;
	bNotifyTick = true;

	Interval = 0.2f;
	RandomDeviation = 0.03f;
}

void UBTService_Enemy_UpdateCombatState::OnSearchStart(FBehaviorTreeSearchData& SearchData)
{
	Super::OnSearchStart(SearchData);
	
	UpdateCombatState(SearchData.OwnerComp);
}

void UBTService_Enemy_UpdateCombatState::TickNode(UBehaviorTreeComponent & OwnerComp, uint8 * NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UpdateCombatState(OwnerComp);
}

void UBTService_Enemy_UpdateCombatState::UpdateCombatState(UBehaviorTreeComponent& OwnerComp)
{
	UE_LOG(LogTemp, Warning, TEXT("[CombatService] Called"));

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Blackboard))
		return;
	
	AAIController* AI = OwnerComp.GetAIOwner();

	// 현재 Enemy 캐릭터 찾기
	AEnemyCharacter* Enemy =
		IsValid(AI)
		? Cast<AEnemyCharacter>(AI->GetPawn())
		: nullptr;

	// Target 찾기
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(EnemyCombatKeys::TargetActor));

	// 현재 Enemy 설정값 읽기: AttackRange 필요
	const UEnemyConfigData* Config =
		IsValid(Enemy)
		? Enemy->GetEnemyConfig()
		: nullptr;

	UE_LOG(LogTemp, Warning,
		TEXT("[CombatService] Enemy=%s Alive=%d Target=%s Config=%s"),
		*GetNameSafe(Enemy),
		IsValid(Enemy) && Enemy->IsAlive(),
		*GetNameSafe(Target),
		*GetNameSafe(Config)
	);

	bool bInAttackRange = false;
	bool bHasLineOfSight = false;

	if (IsValid(Enemy) && Enemy->IsAlive()
		&& IsValid(Target) && Target != Enemy
		&& IsValid(Config))
	{
		// Actor 중심 사이의 수평 거리 기준
		const float DistanceSqaured = FVector::DistSquared2D(Enemy->GetActorLocation(), Target->GetActorLocation());

		bInAttackRange = DistanceSqaured <= FMath::Square(Config->AttackRange);

		bHasLineOfSight = AI->LineOfSightTo(Target);
		
		UE_LOG(LogTemp, Log, TEXT("[%s] Target Distance Squared: %.1f"), *Enemy->GetName(), DistanceSqaured);
	}

	// Blackboard 값 설정: 참조한 값들이 nullptr이든 말든 Blackboard 값 업데이트는 해줘야 함
	Blackboard->SetValueAsBool(EnemyCombatKeys::IsInAttackRange, bInAttackRange);
	Blackboard->SetValueAsBool(EnemyCombatKeys::HasLineOfSight, bHasLineOfSight);
}
