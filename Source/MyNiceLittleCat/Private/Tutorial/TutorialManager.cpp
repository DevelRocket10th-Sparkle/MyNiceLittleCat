#include "Tutorial/TutorialManager.h"
#include "Kismet/GameplayStatics.h"
//#include "GameFramework/Pawn.h"

// 튜토리얼에서 찾는 레벨 액터의 태그 이름 (레벨 액터의 Actor Tags에 똑같이 적어야 함)
namespace TutorialTag
{
	const FName TriggerDoor01(TEXT("Tutorial.Trigger.Door01")); // 이동 구간 문 앞 트리거
}

ATutorialManager::ATutorialManager()
{
 	PrimaryActorTick.bCanEverTick = false;
}

void ATutorialManager::BeginPlay()
{
	Super::BeginPlay();
	ChangeStage(StartStage);
}

void ATutorialManager::ChangeStage(ETutorialStage NewStage)
{
	if (NewStage == CurrentStage || true == bChangingStage) return;

	bChangingStage = true;

	const ETutorialStage PreviousStage = CurrentStage;

	if (ETutorialStage::None != PreviousStage)
		StageEnd(PreviousStage);

	CurrentStage = NewStage;
	StageStart(CurrentStage);

	bChangingStage = false;
}

void ATutorialManager::StageStart(ETutorialStage Stage)
{
	UE_LOG(LogTemp, Log, TEXT("[Tutorial] StageStart: %s"), *UEnum::GetValueAsString(Stage));

	Actions.Reset();
	ActionIndex = INDEX_NONE;

	// 현재 구간의 동작 채워넣기
	BuildActions(Stage, Actions);
	RunNextAction();
}

void ATutorialManager::StageEnd(ETutorialStage Stage)
{
	UE_LOG(LogTemp, Log, TEXT("[Tutorial] StageEnd: %s"), *UEnum::GetValueAsString(Stage));

	// 초기화 및 정리
	GetWorldTimerManager().ClearTimer(WaitTimerHandle);
	StopWaitPlayerEnter();
	Actions.Reset();
	ActionIndex = INDEX_NONE;
}

void ATutorialManager::BuildActions(ETutorialStage Stage, TArray<FTutorialAction>& OutActions)
{
	switch (Stage)
	{
	case ETutorialStage::Movement: BuildMovementActions(OutActions); break;
	default: break;
	}
}

void ATutorialManager::BuildMovementActions(TArray<FTutorialAction>& OutActions)
{
	// 이 구간에서 쓸 레벨 액터 찾기 (구간이 시작될 때 한 번)
	AActor* TriggerDoor01 = FindActorByTag(TutorialTag::TriggerDoor01);

	//! 구조 확인용 로그. 6단계에서 실제 동작으로 교체
	OutActions.Add(Do([] { UE_LOG(LogTemp, Log, TEXT("[Tutorial] 펫: Hi!")); }));
	OutActions.Add(WaitSeconds(1.5f));
	OutActions.Add(Do([] { UE_LOG(LogTemp, Log, TEXT("[Tutorial] WASD 데칼 켜기")); }));
	OutActions.Add(WaitSeconds(1.f));
	OutActions.Add(Do([] { UE_LOG(LogTemp, Log, TEXT("[Tutorial] 펫: Follow!")); }));
	OutActions.Add(WaitPlayerEnter(TriggerDoor01)); // 플레이어가 문 앞까지 올 때까지 멈춤
	OutActions.Add(Do([] { UE_LOG(LogTemp, Log, TEXT("[Tutorial] 문 열기")); }));
}

void ATutorialManager::RunNextAction()
{
	while (true)
	{
		// 즉시 끝나는 동작은 반복문으로 연달아 실행하고,
		// 기다려야 하는 동작을 만나면 return 으로 멈춰요 (기다리는 게 끝났다면? 다시 이 함수 호출함)
		++ActionIndex;
		if (false == Actions.IsValidIndex(ActionIndex))
		{
			OnActionsFinished();
			return;
		}

		const FTutorialAction& Action = Actions[ActionIndex];
		if (Action.Run) // 이 동작이 할 일 실행 (대기 전용 동작이면 Run이 비어있음)
			Action.Run();

		switch (Action.Wait)
		{
		case ETutorialWait::None: continue; // 기다릴 거 없음 -> 바로 다음 동작

		case ETutorialWait::Seconds: // N초 뒤에 이 함수를 다시 부름 -> 다음 동작으로 진행
			GetWorldTimerManager().SetTimer(WaitTimerHandle, this, &ATutorialManager::RunNextAction, Action.Seconds, false);
			return;

		case ETutorialWait::PlayerEnter: // 트리거에 들어올 때까지 멈춤 -> 신호가 오면 다음 동작으로 진행
			if (true == BeginWaitPlayerEnter(Action.Trigger.Get())) return;
			continue; // 트리거가 없거나 플레이어가 이미 안에 있으면 바로 다음 동작
		}
	}
}

void ATutorialManager::OnActionsFinished()
{
	if (ETutorialStage::Completed == CurrentStage) return;
	const ETutorialStage NextStage = static_cast<ETutorialStage>(static_cast<uint8>(CurrentStage) + 1);

	// ChangeStage 도중 (bChangingStage)에 여기로 올 수 있으니, 한 틱 미뤄서 구간 전환
	GetWorldTimerManager().SetTimerForNextTick([this, NextStage] {ChangeStage(NextStage); });
}

FTutorialAction ATutorialManager::Do(TFunction<void()> _Run)
{
	FTutorialAction Action;
	Action.Run = MoveTemp(_Run);
	return Action;
}

FTutorialAction ATutorialManager::WaitSeconds(float _Seconds)
{
	FTutorialAction Action;
	Action.Wait = ETutorialWait::Seconds;
	Action.Seconds = _Seconds;
	return Action;
}

FTutorialAction ATutorialManager::WaitPlayerEnter(AActor* _Trigger)
{
	FTutorialAction Action;
	Action.Wait = ETutorialWait::PlayerEnter;
	Action.Trigger = _Trigger;
	return Action;
}

AActor* ATutorialManager::FindActorByTag(FName _Tag) const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, _Tag, Found);

	// 태그는 액터 하나에만 붙어있어야 함.
	//? 아니라면 세팅 실수인 거시야
	if (1 != Found.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 태그 %s 를 가진 액터가 %d개"), 
			*_Tag.ToString(), Found.Num());
	}

	return (true == Found.IsEmpty()) ? nullptr : Found[0];
}

bool ATutorialManager::BeginWaitPlayerEnter(AActor* _Trigger)
{
	//! 트리거를 못 찾았다면.. 영원히 멈추지 않아요.. 디버깅용
	if (nullptr == _Trigger)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 기다릴 트리거가 없어 건너뜀"));
		return false;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	// 플레이어가 이미 트리거 안에 있으면 바로 통과
	if (nullptr != PlayerPawn && true == _Trigger->IsOverlappingActor(PlayerPawn))
		return false;

	// 트리거를 기다릴 동안만 바인딩
	WaitingTrigger = _Trigger;
	_Trigger->OnActorBeginOverlap.AddUniqueDynamic(this, &ATutorialManager::OnWaitTriggerBeginOverlap);
	return true;
}

void ATutorialManager::StopWaitPlayerEnter()
{
	// 트리거 델리게이트 바인딩 해제
	if (AActor* Trigger = WaitingTrigger.Get())
			Trigger->OnActorBeginOverlap.RemoveDynamic(this, &ATutorialManager::OnWaitTriggerBeginOverlap);

		WaitingTrigger.Reset();
}

void ATutorialManager::OnWaitTriggerBeginOverlap(AActor* _OverlappedActor, AActor* _OtherActor)
{
	if (WaitingTrigger.Get() != _OverlappedActor || false == IsPlayerPawn(_OtherActor))
		return;

	StopWaitPlayerEnter(); // 한번만 반응하도록 먼저 바인딩 해제
	RunNextAction(); // 멈춰있던 동작 목록 이어서 진행
}

bool ATutorialManager::IsPlayerPawn(const AActor* _Actor) const
{
	const APawn* Pawn = Cast<APawn>(_Actor);
	return (nullptr != Pawn && true == Pawn->IsPlayerControlled());
}
