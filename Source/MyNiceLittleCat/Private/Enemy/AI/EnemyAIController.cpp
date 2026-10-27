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
#include "Enemy/Boss/BossCharacter.h"

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

// 활성 상태의 매 프레임 처리
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

// 빙의한 Enemy의 준비 상태를 확인하여 AI 시작 요청
void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Log, TEXT("[%s] Enemy possessed"), *GetNameSafe(GetPawn()));

	TryStartEnemyAI();
}

// 빙의 해제 시 이동과 판단 로직 및 타깃 정리
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

	UE_LOG(LogTemp, Log, TEXT("[%s] Enemy unpossessed"), *GetNameSafe(GetPawn()));
	Super::OnUnPossess();
}

// 잡몹의 발견 및 시야 상실을 타깃 변경에 반영
void AEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	AEnemyCharacter* Enemy = Cast <AEnemyCharacter>(GetPawn());
	if (!IsValid(Enemy) || !Enemy->IsEnemyInitialized() || !Enemy->IsAlive())
		return;

	// 보스의 타깃은 보스방 관리자가 선정하며 시야 상실로 초기화하지 않음
	if (Cast<ABossCharacter>(Enemy))
	{
		return;
	}
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

// 캐릭터 준비와 전투 상태 확인 후 Blackboard 및 BT 시작
void AEnemyAIController::TryStartEnemyAI()
{
	/* 현재 제어 중인 Pawn 확인 */
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn());
	if (!IsValid(Enemy))
		return;

	/* Enemy 초기화 여부 및 생존 여부 확인 */
	if (!Enemy->CanRunEnemyAI())
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
	if (!IsValid(Tree->BlackboardAsset))
	{
		return;
	}
	// 캐릭터별 인지 범위를 Controller 인스턴스에 적용
	SightConfig->SightRadius = Config->SightRadius;
	SightConfig->LoseSightRadius = FMath::Max(Config->SightRadius, Config->LoseSightRadius);
	SightConfig->PeripheralVisionAngleDegrees = Config->PeripheralVisionHalfAngle;
	EnemyPerception->ConfigureSense(*SightConfig);
	EnemyPerception->RequestStimuliListenerUpdate();
	
	/* Blackboard 준비 */
	UBlackboardComponent* BlackboardComp = nullptr;
	if (!UseBlackboard(Tree->BlackboardAsset, BlackboardComp))
		return;

	/* 초기 값 설정 */
	BlackboardComp->SetValueAsVector(EnemyBlackboardKeys::HomeLocation, Enemy->GetActorLocation());

	BlackboardComp->SetValueAsBool(EnemyBlackboardKeys::CanAct, true);

	// 보스 전환 브랜치에서 사용하는 필수 키가 없으면 시작하지 않음
	if (Cast<ABossCharacter>(Enemy) && BlackboardComp->GetKeyID(TEXT("PhaseTransitionRequested")) == FBlackboard::InvalidKey)
	{
		UE_LOG(LogTemp, Error, TEXT("Boss blackboard requires PhaseTransitionRequested."));
		return;
	}

	/* BT 실행 */
	if (!RunBehaviorTree(Tree))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Failed to run BehaviorTree"), *GetName());
	}
	if (ABossCharacter* Boss = Cast<ABossCharacter>(Enemy))
	{
		Boss->RefreshBossBlackboard();
	}
}

// 유효한 전투 타깃을 Blackboard에 전달
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

// Blackboard 타깃과 전투 Focus 해제
void AEnemyAIController::ClearCombatTarget()
{
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->ClearValue(EnemyBlackboardKeys::TargetActor);
	}

	ClearFocus(EAIFocusPriority::Gameplay);
}
