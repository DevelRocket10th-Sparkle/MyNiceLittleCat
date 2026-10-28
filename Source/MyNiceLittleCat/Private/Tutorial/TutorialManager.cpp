#include "Tutorial/TutorialManager.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/ActivatableInterface.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Pet/PetCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pet/SpeechBubbleComponent.h"

// 튜토리얼에서 찾는 레벨 액터의 태그 이름 (레벨 액터의 Actor Tags에 똑같이 적어야 함)
namespace TutorialTag
{
	const FName TriggerDoor01(TEXT("Tutorial.Trigger.Door01")); // 이동 구간 문 앞 트리거
	const FName Gate01(TEXT("Tutorial.Gate.01"));               // 이동 → 대시 사이 문
	const FName DecalWASD(TEXT("Tutorial.Decal.WASD")); // 이동 구간 바닥 WASD 안내

	const FName PetPointStart(TEXT("Tutorial.PetPoint.Start"));             // 펫 첫 등장 위치
	const FName PetPointDoor01(TEXT("Tutorial.PetPoint.Door01"));           // 이동 구간 문 앞
	const FName PetPointAfterDoor01(TEXT("Tutorial.PetPoint.AfterDoor01")); // 이동 구간 문 너머
}

ATutorialManager::ATutorialManager()
{
 	PrimaryActorTick.bCanEverTick = false;
}

void ATutorialManager::BeginPlay()
{
	Super::BeginPlay();
	
	// 펫은 GameMode 가 플레이어를 스폰한 "뒤에" 만들기 때문에 지금은 아직 없을 수 있음
	// -> 0.1초마다 확인하다가 준비되면 시작 (게임모드, 펫 코드는 건드리지 않음)
	GetWorldTimerManager().SetTimer(StartTimerHandle, this, &ATutorialManager::TryStartTutorial, 0.1f, true);
}

void ATutorialManager::TryStartTutorial()
{
	AMainGameMode* GameMode = GetWorld()->GetAuthGameMode<AMainGameMode>();
	APetCharacter* FoundPet = (nullptr != GameMode) ? GameMode->GetPet() : nullptr;

	// 펫이 아직 게임 모드에서 안 만들어졌거나, 초기화 전이라면 다음 확인할 때 다시 확인..
	if (nullptr == FoundPet || false == FoundPet->IsPetInitialized())
		return;

	GetWorldTimerManager().ClearTimer(StartTimerHandle); // 준비 완료 -> 펫 더 확인 안 함
	Pet = FoundPet;
	PetDefaultSpeed = FoundPet->GetCharacterMovement()->MaxWalkSpeed; // 기존 속도 기억

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
	TWeakObjectPtr<AActor> Gate01 = FindActorByTag(TutorialTag::Gate01);
	TWeakObjectPtr<AActor> DecalWASD = FindActorByTag(TutorialTag::DecalWASD);

	TWeakObjectPtr<AActor> PetStart = FindActorByTag(TutorialTag::PetPointStart);
	TWeakObjectPtr<AActor> PetDoor01 = FindActorByTag(TutorialTag::PetPointDoor01);
	TWeakObjectPtr<AActor> PetAfterDoor01 = FindActorByTag(TutorialTag::PetPointAfterDoor01);

	// 1) 펫이 플레이어 근처에 나타나 인사
	OutActions.Add(Do([this, PetStart] { PlacePet(PetStart.Get()); }));
	OutActions.Add(Do([this] { PetSay(INVTEXT("Hi!")); }));
	OutActions.Add(WaitSeconds(1.5f));

	// 2) 이동 키 안내
	OutActions.Add(Do([this, DecalWASD]{SetActivated(DecalWASD.Get(), true); }));
	OutActions.Add(WaitSeconds(1.f));

	// 3) 펫이 문쪽으로 천천히 걸어가며 따라오라고 함
	OutActions.Add(Do([this, PetDoor01] {PetMoveTo(PetDoor01.Get(), SlowPetSpeed); }));
	OutActions.Add(Do([this] { PetSay(INVTEXT("Follow Me!")); }));

	// 4) 플레이어가 문 앞에 오면 문을 열고, 문이 어느 정도 열린 뒤 펫이 먼저 지나감
	OutActions.Add(WaitPlayerEnter(TriggerDoor01)); // (플레이어가 문 앞까지 올 때까지 멈춤)
	OutActions.Add(Do([this, Gate01] {SetActivated(Gate01.Get(), true); })); // 문 열기
	OutActions.Add(WaitSeconds(0.6f)); // 문짝이 다 열리기 전에 펫이 부딪히지 않게
	OutActions.Add(Do([this, PetAfterDoor01] {PetMoveTo(PetAfterDoor01.Get()); }));
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

void ATutorialManager::SetActivated(AActor* _Target, bool _bActivate)
{
	if (nullptr == _Target || false == _Target->Implements<UActivatableInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] %s 는 Activatable 이 아님"), *GetNameSafe(_Target));
		return;
	}

	// 상호작용 활/비활
	if (true == _bActivate)
		IActivatableInterface::Execute_Activate(_Target, this);
	else
		IActivatableInterface::Execute_Deactivate(_Target, this);
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

void ATutorialManager::PlacePet(AActor* _Point)
{
	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr || nullptr == _Point) return;

	// 순간이동
	PetPtr->TeleportTo(_Point->GetActorLocation(), _Point->GetActorRotation());
}

void ATutorialManager::PetMoveTo(AActor* _Point, float _Speed)
{
	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr || nullptr == _Point)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetMoveTo: 펫 또는 지점이 없음."));
		return;
	}

	// 속도 지정이 없으면(0) 원래 속도로 되돌려요 -> 느린 속도가 남아 Follow가 꼬이는 걸 방지
	PetPtr->GetCharacterMovement()->MaxWalkSpeed = (_Speed > 0.f) ? _Speed : PetDefaultSpeed;

	// 이동은 펫의 StateTree(MoveTo)가 처리함. 매니저는 목적지만 알려줘요
	if (false == PetPtr->RequestMoveTo(_Point->GetActorLocation()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetMoveTo: 요청 거절됨 (펫 초기화 전?)"));
	}
}

void ATutorialManager::PetSay(const FText& _Message)
{
	//! 임시..
	UE_LOG(LogTemp, Log, TEXT("[Tutorial] 펫: %s"), *_Message.ToString());

	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr) return;

	if (USpeechBubbleComponent* Bubble = PetPtr->GetSpeechBubble())
		Bubble->Say(_Message);
}
