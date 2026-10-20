#include "Enemy/Boss/BossEncounterManager.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/AI/EnemyAIController.h"
#include "Characters/PlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABossEncounterManager::ABossEncounterManager()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

// 컴포넌트 초기화 후 실행 준비
void ABossEncounterManager::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) ResetEncounter();
}

// 레벨 기믹 참조를 먼저 전달한 뒤 보스 생성과 초기화 확인
bool ABossEncounterManager::SpawnBoss()
{
	if (!BossClass) return false;
	// 보스의 초기 위치는 재시도 때마다 같은 설정 사용
	const FTransform SpawnTransform = IsValid(SpawnPoint) ? SpawnPoint->GetActorTransform() : GetActorTransform();
	ABossCharacter* Boss = GetWorld()->SpawnActorDeferred<ABossCharacter>(BossClass, SpawnTransform,
		this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Boss) return false;
	CurrentBoss = Boss;
	Boss->OnDefeated.AddUniqueDynamic(this, &ABossEncounterManager::HandleBossDefeated);
	Boss->OnEncounterFailed.AddUniqueDynamic(this, &ABossEncounterManager::HandleBossFailed);
	Boss->FinishSpawning(SpawnTransform);
	if (!Boss->GetController()) Boss->SpawnDefaultController();
	if (!Boss->IsEnemyInitialized() || !Cast<AEnemyAIController>(Boss->GetController()))
	{
		RemoveCurrentBoss();
		return false;
	}
	return true;
}

// 이벤트 해제 및 전투 정리 후 현재 보스 제거
void ABossEncounterManager::RemoveCurrentBoss()
{
	if (!IsValid(CurrentBoss)) { CurrentBoss = nullptr; return; }
	CurrentBoss->OnDefeated.RemoveDynamic(this, &ABossEncounterManager::HandleBossDefeated);
	CurrentBoss->OnEncounterFailed.RemoveDynamic(this, &ABossEncounterManager::HandleBossFailed);
	CurrentBoss->StopEncounter();
	CurrentBoss->Destroy();
	CurrentBoss = nullptr;
}

// 보스와 기믹 정리 후 새 보스를 생성하여 체력 및 페이즈 초기화
void ABossEncounterManager::ResetEncounter()
{
	if (!HasAuthority()) return;
	++EncounterGeneration;
	EncounterState = EBossEncounterState::Resetting;
	GetWorld()->GetTimerManager().ClearTimer(ParticipantCheckHandle);
	GetWorld()->GetTimerManager().ClearTimer(ResetTimerHandle);
	Participants.Reset();
	RemoveCurrentBoss();
	if (SpawnBoss()) EncounterState = EBossEncounterState::Ready;
	else UE_LOG(LogTemp, Error, TEXT("Boss room initialization failed: %s"), *GetName());
}

bool ABossEncounterManager::IsParticipantAlive(AActor* Participant) const
{
	// 현재 Player는 인터페이스 미구현 상태이므로 사망 Getter 사용
	const APlayerCharacter* Player = Cast<APlayerCharacter>(Participant);
	return IsValid(Player) && !Player->GetbIsDead();
}

AActor* ABossEncounterManager::FindAliveParticipant() const
{
	for (const TWeakObjectPtr<AActor>& Participant : Participants)
		if (IsParticipantAlive(Participant.Get())) return Participant.Get();
	return nullptr;
}

// 준비된 보스방의 첫 페이즈와 타깃 선정 시작
bool ABossEncounterManager::StartEncounter(const TArray<AActor*>& InParticipants)
{
	if (!HasAuthority() || EncounterState != EBossEncounterState::Ready || !IsValid(CurrentBoss)) return false;
	Participants.Reset();
	for (AActor* Participant : InParticipants)
		if (IsParticipantAlive(Participant)) Participants.AddUnique(TWeakObjectPtr<AActor>(Participant));
	// 최초 타깃은 참가자 목록 중 첫 번째 생존자
	AActor* Target = FindAliveParticipant();
	if (!Target) return false;
	AEnemyAIController* AI = Cast<AEnemyAIController>(CurrentBoss->GetController());
	if (!AI || !CurrentBoss->StartEncounter()) { ResetEncounter(); return false; }
	if (!AI->GetBrainComponent() || !AI->GetBrainComponent()->IsRunning())
	{
		ResetEncounter();
		return false;
	}
	EncounterState = EBossEncounterState::Running;
	AI->SetCombatTarget(Target);
	// 타이머가 만들어진 전투 번호를 저장하여 이전 전투의 호출 무시
	const int64 StartedGeneration = EncounterGeneration;
	FTimerDelegate CheckDelegate;
	CheckDelegate.BindWeakLambda(this, [this, StartedGeneration]()
	{
		if (StartedGeneration == EncounterGeneration) CheckParticipants();
	});
	GetWorld()->GetTimerManager().SetTimer(ParticipantCheckHandle, CheckDelegate, 0.5f, true);
	return true;
}

// 현재 타깃을 유지하되 사망 시 교체하고 전멸 시 초기화
void ABossEncounterManager::CheckParticipants()
{
	if (EncounterState != EBossEncounterState::Running) return;
	if (!IsValid(CurrentBoss)) { ResetEncounter(); return; }
	// 기존 타깃이 살아 있으면 유지하여 매번 타깃을 교체하지 않음
	AEnemyAIController* AI = Cast<AEnemyAIController>(CurrentBoss->GetController());
	UBlackboardComponent* Blackboard = AI ? AI->GetBlackboardComponent() : nullptr;
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetActor"))) : nullptr;
	if (IsParticipantAlive(Target)) return;
	Target = FindAliveParticipant();
	if (!Target) { ResetEncounter(); return; }
	if (AI) AI->SetCombatTarget(Target);
}

void ABossEncounterManager::HandleBossDefeated(ABossCharacter* Boss)
{
	if (Boss != CurrentBoss || EncounterState != EBossEncounterState::Running) return;
	++EncounterGeneration;
	GetWorld()->GetTimerManager().ClearTimer(ParticipantCheckHandle);
	EncounterState = EBossEncounterState::Cleared;
}

void ABossEncounterManager::HandleBossFailed(ABossCharacter* Boss)
{
	if (Boss != CurrentBoss) return;
	EncounterState = EBossEncounterState::Resetting;
	GetWorld()->GetTimerManager().ClearTimer(ParticipantCheckHandle);
	// 실패 통지를 실행 중인 보스는 다음 Tick 이후 제거
	const int64 ExpectedGeneration = ++EncounterGeneration;
	ResetTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this, ExpectedGeneration]()
		{
			if (ExpectedGeneration == EncounterGeneration && EncounterState == EBossEncounterState::Resetting) ResetEncounter();
		}));
}

// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
void ABossEncounterManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	++EncounterGeneration;
	GetWorld()->GetTimerManager().ClearTimer(ParticipantCheckHandle);
	GetWorld()->GetTimerManager().ClearTimer(ResetTimerHandle);
	RemoveCurrentBoss();
	Super::EndPlay(EndPlayReason);
}
