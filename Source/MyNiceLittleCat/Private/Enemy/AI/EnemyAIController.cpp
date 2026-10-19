// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/AI/EnemyAIController.h"

#include "DrawDebugHelpers.h"

#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AIPerceptionTypes.h"
#include "GameFramework/Pawn.h"

#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "Enemy/EnemyCharacter.h"
#include "Enemy/Datas/EnemyConfigData.h"

namespace EnemyBlackboardKeys
{
	const FName TargetActor(TEXT("TargetActor"));
	const FName HomeLocation(TEXT("HomeLocation"));
	const FName CanAct(TEXT("CanAct"));
}

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	// AI 로직 자동 실행 방지
	// -> Enemy의 초기화 완료를 확인한 뒤 직접 시작
	bStartAILogicOnPossess = false;

	EnemyPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("EnemyPerception"));
	SetPerceptionComponent(*EnemyPerception);
	
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	// 감지 시작 거리
	SightConfig->SightRadius = 1000.f;

	// 이미 발견한 대상을 시야에서 놓치는 거리
	SightConfig->LoseSightRadius = 1200.f;

	// 정면 기준 좌우 각각 60도
	SightConfig->PeripheralVisionAngleDegrees = 60.f;

	// 현재 팀 구분이 없기 때문에 전부 감지하고, 이벤트에서 플레이어만 선택
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	EnemyPerception->ConfigureSense(*SightConfig);
	EnemyPerception->SetDominantSense(UAISense_Sight::StaticClass());

	EnemyPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);
}

void AEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn());
	if (!IsValid(Enemy))
		return;

	const UPathFollowingComponent* PathFollowing = GetPathFollowingComponent();

	const bool bWantsToMove = Enemy->IsAlive() && IsValid(PathFollowing) && PathFollowing->GetStatus() == EPathFollowingStatus::Moving;

	Enemy->SetWantsToMove(bWantsToMove);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Log, TEXT("[%s] Enemy possessed"), *GetPawn()->GetName());

	TryStartEnemyAI();
}

void AEnemyAIController::OnUnPossess()
{
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Enemy unpossessed"));
	}

	StopMovement();			// 움직임 멈추기
	ClearCombatTarget();	// 전투 종료

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn()))
	{
		Enemy->SetWantsToMove(false);
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] Enemy unpossessed"), *GetPawn()->GetName());
	Super::OnUnPossess();
}

void AEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	AEnemyCharacter* Enemy = Cast <AEnemyCharacter>(GetPawn());
	if (!IsValid(Enemy) || !Enemy->IsEnemyInitialized() || !Enemy->IsAlive())
		return;

	APawn* DetectedPawn = Cast<APawn>(Actor);
	// 플레이어가 조종하는 Pawn만 공격 대상으로 선택
	if (!IsValid(DetectedPawn) || !DetectedPawn->IsPlayerControlled())
		return;

	if (Stimulus.WasSuccessfullySensed())
	{
		SetCombatTarget(DetectedPawn);
		
		// Debug
		DrawDebugLine(
			GetWorld(),
			Enemy->GetPawnViewLocation(),	// Enemy 시점
			DetectedPawn->GetActorLocation(),
			FColor::Red,
			false,	// 영구 표시 여부
			2.f,	// 표시 시간: 2초
			0,
			3.f		// 선 두께
		);
	}
	else
	{
		UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
		if (!IsValid(BlackboardComp))
			return;

		// 타겟 잃었으면 집 가기
		if (IsValid(BlackboardComp)
			&& BlackboardComp->GetValueAsObject(TEXT("TargetActor")) == DetectedPawn)
		{
			ClearCombatTarget();
		}
	}
}

void AEnemyAIController::TryStartEnemyAI()
{
	/* 현재 제어 중인 Pawn 확인 */
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn());
	if (!IsValid(Enemy))
		return;

	/* Enemy 초기화 여부 및 생존 여부 확인 */
	if (!Enemy->IsEnemyInitialized() || !Enemy->IsAlive())
		return;

	/* 이미 실행 중이면 중복 시작 방지 */
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		if (Brain->IsRunning())
			return;
	}

	/* Enemy 설정과 BT 확인 */
	const UEnemyConfigData* Config = Enemy->GetEnemyConfig();

	if (!IsValid(Config) || !IsValid(Config->BehaviorTree))
		return;

	UBehaviorTree* Tree = Config->BehaviorTree;
	
	/* Blackboard 준비 */
	UBlackboardComponent* BlackboardComp = nullptr;
	if (!UseBlackboard(Tree->BlackboardAsset, BlackboardComp))
		return;

	/* 초기 값 설정 */
	BlackboardComp->SetValueAsVector(EnemyBlackboardKeys::HomeLocation, Enemy->GetActorLocation());

	BlackboardComp->SetValueAsBool(EnemyBlackboardKeys::CanAct, true);

	/* BT 실행 */
	if (!RunBehaviorTree(Tree))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Failed to run BehaviorTree"), *GetName());
	}
}

void AEnemyAIController::SetCombatTarget(AActor * NewTarget)
{
	// 타깃 유효성 검사
	if (!IsValid(NewTarget))
	{
		ClearCombatTarget();
		return;
	}

	// 자기 자신을 타깃으로 지정하지 않음
	if (NewTarget == GetPawn())
		return;

	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!IsValid(BlackboardComp))
		return;
	if (!IsValid(BlackboardComp))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SetCombatTarget] Blackboard is missing"));

		return;
	}
	
	BlackboardComp->SetValueAsObject(EnemyBlackboardKeys::TargetActor, NewTarget);
	UE_LOG(LogTemp, Warning,
		TEXT("[SetCombatTarget] Stored Target=%s"),
		*GetNameSafe(BlackboardComp->GetValueAsObject(
			EnemyBlackboardKeys::TargetActor)));
}

void AEnemyAIController::ClearCombatTarget()
{
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->ClearValue(EnemyBlackboardKeys::TargetActor);
	}

	ClearFocus(EAIFocusPriority::Gameplay);
}
