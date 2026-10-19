// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/AI/Services/BTService_Enemy_UpdateHomeState.h"

#include "Enemy/EnemyCharacter.h"
#include "Enemy/Datas/EnemyConfigData.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_Enemy_UpdateHomeState::UBTService_Enemy_UpdateHomeState()
{
	NodeName = TEXT("Update Home State");

	bNotifyOnSearch = true;
	bNotifyTick = true;

	Interval = 0.3f;
	RandomDeviation = 0.05f;

	BlackboardKey.AddVectorFilter(
		this,
		GET_MEMBER_NAME_CHECKED(
			UBTService_Enemy_UpdateHomeState,
			BlackboardKey
		)
	);
}

void UBTService_Enemy_UpdateHomeState::OnSearchStart(FBehaviorTreeSearchData& SearchData)
{
	Super::OnSearchStart(SearchData);
	
	UpdateHomeState(SearchData.OwnerComp);
}

void UBTService_Enemy_UpdateHomeState::TickNode(UBehaviorTreeComponent & OwnerComp, uint8 * NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UpdateHomeState(OwnerComp);
}

void UBTService_Enemy_UpdateHomeState::UpdateHomeState(UBehaviorTreeComponent & OwnerComp)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Blackboard))
		return;

	AAIController* AI = OwnerComp.GetAIOwner();

	// 현재 Enemy 캐릭터 찾기
	AEnemyCharacter* Enemy =
		IsValid(AI)
		? Cast<AEnemyCharacter>(AI->GetPawn())
		: nullptr;

	// 현재 Enemy 설정값 읽기: HomeArrivalRadius 필요
	const UEnemyConfigData* Config =
		IsValid(Enemy)
		? Enemy->GetEnemyConfig()
		: nullptr;

	const FName HomeLocationKey = BlackboardKey.SelectedKeyName;

	// 유효 정보가 없으면 BT에서 복귀 분기 선택하지 않도록 설정
	if (!IsValid(Enemy) || !IsValid(Config) || !Blackboard->IsVectorValueSet(HomeLocationKey))
	{
		Blackboard->SetValueAsBool(TEXT("IsAwayFromHome"), false);

		return;
	}

	// 거리 알아내기
	const float DistanceSquared = FVector::DistSquared2D(Enemy->GetActorLocation(), Blackboard->GetValueAsVector(HomeLocationKey));

	// Home이랑 떨어졌는지 기록
	const bool bIsAwayFromHome = DistanceSquared > FMath::Square(Config->HomeArrivalRadius);

	Blackboard->SetValueAsBool(TEXT("IsAwayFromHome"), bIsAwayFromHome);
}
