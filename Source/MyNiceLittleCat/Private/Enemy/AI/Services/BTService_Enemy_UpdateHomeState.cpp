// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/AI/Services/BTService_Enemy_UpdateHomeState.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

#include "Enemy/AI/EnemyBlackboardKeys.h"
#include "Enemy/Data/EnemyConfigData.h"
#include "Enemy/EnemyCharacter.h"

UBTService_Enemy_UpdateHomeState::UBTService_Enemy_UpdateHomeState()
{
	NodeName = TEXT("Update Home State");

	bNotifyOnSearch = true;
	bNotifyTick = true;

	Interval = 0.3f;
	RandomDeviation = 0.05f;

	// 입력: 복귀 위치 (Vector)
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_Enemy_UpdateHomeState, BlackboardKey));

	// 출력: 기존 BT 에셋을 위해 기본 키 이름 지정
	IsAwayFromHomeKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_Enemy_UpdateHomeState, IsAwayFromHomeKey));
	IsAwayFromHomeKey.SelectedKeyName = EnemyBlackboardKeys::IsAwayFromHome;
}

void UBTService_Enemy_UpdateHomeState::InitializeFromAsset(UBehaviorTree& Asset)
{
	// 부모가 BlackboardKey를 해석
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* Blackboard = GetBlackboardAsset())
	{
		IsAwayFromHomeKey.ResolveSelectedKey(*Blackboard);
	}
}

void UBTService_Enemy_UpdateHomeState::OnSearchStart(FBehaviorTreeSearchData& SearchData)
{
	Super::OnSearchStart(SearchData);
	UpdateHomeState(SearchData.OwnerComp);
}

void UBTService_Enemy_UpdateHomeState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	UpdateHomeState(OwnerComp);
}

void UBTService_Enemy_UpdateHomeState::UpdateHomeState(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Blackboard) || !IsAwayFromHomeKey.IsSet())
	{
		return;
	}

	const AAIController* AI = OwnerComp.GetAIOwner();
	const AEnemyCharacter* Enemy = IsValid(AI) ? Cast<AEnemyCharacter>(AI->GetPawn()) : nullptr;
	const UEnemyConfigData* Config = IsValid(Enemy) ? Enemy->GetEnemyConfig() : nullptr;

	const FBlackboard::FKey HomeKey = BlackboardKey.GetSelectedKeyID();

	// 정보가 없으면 BT가 복귀 분기를 선택하지 않도록 false
	bool bIsAwayFromHome = false;

	if (IsValid(Config) && Blackboard->IsVectorValueSet(HomeKey))
	{
		const FVector HomeLocation = Blackboard->GetValue<UBlackboardKeyType_Vector>(HomeKey);
		const double DistanceSquared = FVector::DistSquared2D(Enemy->GetActorLocation(), HomeLocation);
		bIsAwayFromHome = DistanceSquared > FMath::Square(Config->HomeArrivalRadius);
	}

	Blackboard->SetValue<UBlackboardKeyType_Bool>(IsAwayFromHomeKey.GetSelectedKeyID(), bIsAwayFromHome);
}
