// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/AI/EnemyAIController.h"
#include "Enemy/EnemyLog.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BrainComponent.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

#include "Enemy/AI/EnemyBlackboardKeys.h"
#include "Enemy/Data/EnemyConfigData.h"
#include "Enemy/EnemyCharacter.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	// AI 로직 자동 실행 방지: Enemy 초기화 완료를 확인한 뒤 TryStartEnemyAI에서 직접 시작
	bStartAILogicOnPossess = false;

	/* 시야 인지 */

	EnemyPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("EnemyPerception"));
	SetPerceptionComponent(*EnemyPerception);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	// 기본값: 실제 값은 TryStartEnemyAI에서 EnemyConfigData로 덮어씀
	SightConfig->SightRadius = 1000.f;
	SightConfig->LoseSightRadius = 1200.f;
	SightConfig->PeripheralVisionAngleDegrees = 60.f;

	// 현재 팀 구분이 없기 때문에 전부 감지하고, 이벤트에서 플레이어만 선택
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	EnemyPerception->ConfigureSense(*SightConfig);
	EnemyPerception->SetDominantSense(UAISense_Sight::StaticClass());

	EnemyPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);
}

/* AI 시작 */

// 캐릭터 준비 확인 → 인지 범위 적용 → Blackboard 준비/검증 → BT 시작
void AEnemyAIController::TryStartEnemyAI()
{
	// 현재 제어 중인 Enemy
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn());
	if (!IsValid(Enemy) || !Enemy->CanRunEnemyAI())
	{
		return;
	}

	// 이미 실행 중이면 중복 시작 방지
	if (const UBrainComponent* Brain = GetBrainComponent(); Brain && Brain->IsRunning())
	{
		return;
	}

	// Enemy 설정과 BT 확인
	const UEnemyConfigData* Config = Enemy->GetEnemyConfig();
	UBehaviorTree* Tree = IsValid(Config) ? Config->BehaviorTree.Get() : nullptr;
	if (!IsValid(Tree) || !IsValid(Tree->BlackboardAsset))
	{
		UE_LOG(LogEnemy, Error, TEXT("[%s] BehaviorTree or Blackboard asset is missing"), *GetNameSafe(Enemy));
		return;
	}

	/* 캐릭터별 인지 범위 적용 */

	SightConfig->SightRadius = Config->SightRadius;
	SightConfig->LoseSightRadius = FMath::Max(Config->SightRadius, Config->LoseSightRadius);
	SightConfig->PeripheralVisionAngleDegrees = Config->PeripheralVisionHalfAngle;
	EnemyPerception->ConfigureSense(*SightConfig);
	EnemyPerception->RequestStimuliListenerUpdate();

	/* Blackboard 준비 및 검증 */

	UBlackboardComponent* BlackboardComp = nullptr;
	if (!UseBlackboard(Tree->BlackboardAsset, BlackboardComp) || !BlackboardComp)
	{
		return;
	}

	// 필수 키가 없으면 BT를 시작하지 않음: "실행 중 조용히 실패"하지 않고 시작할 때 크게 실패
	if (!CacheBlackboardKeys(*BlackboardComp))
	{
		UE_LOG(LogEnemy, Error, TEXT("[%s] Blackboard '%s' is missing required keys. AI not started."),
			*GetNameSafe(Enemy), *GetNameSafe(Tree->BlackboardAsset));
		return;
	}

	/* 초기 값 */

	if (HomeLocationKey != FBlackboard::InvalidKey)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Vector>(HomeLocationKey, Enemy->GetActorLocation());
	}
	SetBlackboardBool(CanActKey, true);

	/* BT 실행 */

	if (!RunBehaviorTree(Tree))
	{
		UE_LOG(LogEnemy, Warning, TEXT("[%s] Failed to run BehaviorTree"), *GetName());
		return;
	}

	OnBehaviorTreeStarted();
}

bool AEnemyAIController::CacheBlackboardKeys(const UBlackboardComponent& BlackboardComponent)
{
	TargetActorKey = FindKey(BlackboardComponent, EnemyBlackboardKeys::TargetActor, UBlackboardKeyType_Object::StaticClass(), true);
	HomeLocationKey = FindKey(BlackboardComponent, EnemyBlackboardKeys::HomeLocation, UBlackboardKeyType_Vector::StaticClass(), false);
	CanActKey = FindKey(BlackboardComponent, EnemyBlackboardKeys::CanAct, UBlackboardKeyType_Bool::StaticClass(), false);

	return TargetActorKey != FBlackboard::InvalidKey;
}

/* 타깃 */

// 유효한 전투 타깃을 Blackboard와 Focus에 반영
void AEnemyAIController::SetCombatTarget(AActor* NewTarget)
{
	if (!IsValid(NewTarget))
	{
		ClearCombatTarget();
		return;
	}

	// 자기 자신을 타깃으로 지정하지 않음
	if (NewTarget == GetPawn())
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!IsValid(BlackboardComp))
	{
		UE_LOG(LogEnemy, Warning, TEXT("[%s] SetCombatTarget: Blackboard is missing"), *GetName());
		return;
	}

	// BT 시작 전(튜토리얼처럼 빙의 직후 지정)에도 동작
	const FBlackboard::FKey Key = ResolveTargetActorKey(*BlackboardComp);
	if (Key == FBlackboard::InvalidKey)
	{
		return;
	}

	BlackboardComp->SetValue<UBlackboardKeyType_Object>(Key, NewTarget);

	// 컨트롤러가 타깃 방향을 계산하고, 캐릭터는 설정된 회전 속도로 천천히 따라감
	SetFocus(NewTarget, EAIFocusPriority::Gameplay);
}

// Blackboard 타깃과 전투 Focus 해제
void AEnemyAIController::ClearCombatTarget()
{
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		const FBlackboard::FKey Key = ResolveTargetActorKey(*BlackboardComp);
		if (Key != FBlackboard::InvalidKey)
		{
			BlackboardComp->ClearValue(Key);
		}
	}

	ClearFocus(EAIFocusPriority::Gameplay);
}

AActor* AEnemyAIController::GetCombatTarget() const
{
	const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return nullptr;
	}

	const FBlackboard::FKey Key = ResolveTargetActorKey(*BlackboardComp);
	return Key != FBlackboard::InvalidKey
		? Cast<AActor>(BlackboardComp->GetValue<UBlackboardKeyType_Object>(Key))
		: nullptr;
}

// 잡몹의 발견 및 시야 상실을 타깃 변경에 반영
void AEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 보스처럼 외부에서 타깃을 정하는 컨트롤러는 인지 결과를 무시
	if (!ShouldTrackPerceivedTargets())
	{
		return;
	}

	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn());
	if (!IsValid(Enemy) || !Enemy->IsEnemyInitialized() || !Enemy->IsAlive())
	{
		return;
	}

	// 플레이어가 조종하는 Pawn만 공격 대상으로 선택
	APawn* DetectedPawn = Cast<APawn>(Actor);
	if (!IsValid(DetectedPawn) || !DetectedPawn->IsPlayerControlled())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		SetCombatTarget(DetectedPawn);
		return;
	}

	// 현재 타깃을 놓친 경우에만 해제 → BT가 복귀 분기 선택
	if (GetCombatTarget() == DetectedPawn)
	{
		ClearCombatTarget();
	}
}

/* 프레임 갱신 및 빙의 */

void AEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn());
	if (!IsValid(Enemy))
	{
		return;
	}

	// 경로 이동 중인지 AnimInstance에 전달 (이동 애니메이션 재생 여부)
	const UPathFollowingComponent* PathFollowingComp = GetPathFollowingComponent();
	const bool bWantsToMove = Enemy->IsAlive()
		&& IsValid(PathFollowingComp)
		&& PathFollowingComp->GetStatus() == EPathFollowingStatus::Moving;

	Enemy->SetWantsToMove(bWantsToMove);
}

// 빙의한 Enemy의 준비 상태를 확인하여 AI 시작 요청
void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogEnemy, Log, TEXT("[%s] Enemy possessed"), *GetNameSafe(GetPawn()));

	TryStartEnemyAI();
}

// 빙의 해제 시 이동과 판단 로직 및 타깃 정리
void AEnemyAIController::OnUnPossess()
{
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Enemy unpossessed"));
	}

	StopMovement();
	ClearCombatTarget();
	ClearFocus(EnemyFocusPriority::AttackFacing);

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn()))
	{
		Enemy->SetWantsToMove(false);
	}

	UE_LOG(LogEnemy, Log, TEXT("[%s] Enemy unpossessed"), *GetNameSafe(GetPawn()));
	Super::OnUnPossess();
}

/* Blackboard 도우미 */

FBlackboard::FKey AEnemyAIController::FindKey(const UBlackboardComponent& Blackboard, FName KeyName,
	TSubclassOf<UBlackboardKeyType> ExpectedType, bool bRequired)
{
	const FBlackboard::FKey KeyId = Blackboard.GetKeyID(KeyName);
	const UBlackboardData* Asset = Blackboard.GetBlackboardAsset();

	// 키 없음
	if (KeyId == FBlackboard::InvalidKey || !Asset)
	{
		if (bRequired)
		{
			UE_LOG(LogEnemy, Error, TEXT("[Blackboard] '%s' requires key '%s'"), *GetNameSafe(Asset), *KeyName.ToString());
		}
		return FBlackboard::InvalidKey;
	}

	// 타입 불일치: 다른 타입 값을 읽고 쓰면 조용히 실패하므로 여기서 차단
	if (Asset->GetKeyType(KeyId) != ExpectedType)
	{
		UE_LOG(LogEnemy, Error, TEXT("[Blackboard] '%s' key '%s' must be %s"),
			*GetNameSafe(Asset), *KeyName.ToString(), *GetNameSafe(ExpectedType.Get()));
		return FBlackboard::InvalidKey;
	}

	return KeyId;
}

FBlackboard::FKey AEnemyAIController::ResolveTargetActorKey(const UBlackboardComponent& BlackboardComp) const
{
	return TargetActorKey != FBlackboard::InvalidKey
		? TargetActorKey
		: BlackboardComp.GetKeyID(EnemyBlackboardKeys::TargetActor);
}

void AEnemyAIController::SetBlackboardBool(FBlackboard::FKey KeyId, bool bValue)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (BlackboardComp && KeyId != FBlackboard::InvalidKey)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Bool>(KeyId, bValue);
	}
}
