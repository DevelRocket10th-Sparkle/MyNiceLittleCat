#include "Tutorial/TutorialManager.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/ActivatableInterface.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Pet/PetCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pet/SpeechBubbleComponent.h"
#include "AIController.h"                 // 비행 전 길찾기 이동 멈추기
#include "Components/CapsuleComponent.h"  // 착지 높이 계산

// 튜토리얼에서 찾는 레벨 액터의 태그 이름 (레벨 액터의 Actor Tags에 똑같이 적어야 함)
namespace TutorialTag
{
	//! 이동 구간
	const FName CameraDoor01(TEXT("Tutorial.Camera.Door01")); // 이동 구간 시작 연출 카메라 (문 쪽을 보여줌)

	const FName TriggerDoor01(TEXT("Tutorial.Trigger.Door01")); // 이동 구간 문 앞 트리거
	const FName Gate01(TEXT("Tutorial.Gate.01"));               // 이동 → 대시 사이 문
	const FName DecalWASD(TEXT("Tutorial.Decal.WASD")); // 이동 구간 바닥 WASD 안내

	const FName PetPointStart(TEXT("Tutorial.PetPoint.Start"));             // 펫 첫 등장 위치
	const FName PetPointDoor01(TEXT("Tutorial.PetPoint.Door01"));           // 이동 구간 문 앞
	const FName PetPointAfterDoor01(TEXT("Tutorial.PetPoint.AfterDoor01")); // 이동 구간 문 너머

	//! 대시 구간
	const FName TriggerDashEnter(TEXT("Tutorial.Trigger.DashEnter"));          // 대시 방 입구 (구간 실제 시작)
	const FName CameraDashCorridor(TEXT("Tutorial.Camera.DashCorridor"));      // 카메라 C
	const FName PetPointDashEnd(TEXT("Tutorial.PetPoint.DashEnd"));            // 펫 착지 지점 (통로 끝)
	const FName PetPointAfterDoor02(TEXT("Tutorial.PetPoint.AfterDoor02"));    // 다음 문 너머
	const FName DecalShift(TEXT("Tutorial.Decal.Shift"));                      // Shift 데칼
	const FName TriggerDoor02(TEXT("Tutorial.Trigger.Door02"));                // 통로 끝 문 앞
	const FName Gate02(TEXT("Tutorial.Gate.02"));                              // 대시 -> 락온 사이 문
}

ATutorialManager::ATutorialManager()
{
 	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ATutorialManager::BeginPlay()
{
	Super::BeginPlay();
	
	// 리스폰되면 연출을 강제로 끝내기 위해 구독 (게임모드는 리스폰돼도 그대로라 한 번만)
	if (AMainGameMode* GameMode = GetWorld()->GetAuthGameMode<AMainGameMode>())
		GameMode->OnPlayerRespawned.AddUniqueDynamic(this, &ATutorialManager::HandlePlayerRespawned);

	// 펫은 GameMode 가 플레이어를 스폰한 "뒤에" 만들기 때문에 지금은 아직 없을 수 있음
	// -> 0.1초마다 확인하다가 준비되면 시작 (게임모드, 펫 코드는 건드리지 않음)
	GetWorldTimerManager().SetTimer(StartTimerHandle, this, &ATutorialManager::TryStartTutorial, 0.1f, true);
	TryStartTutorial(); // 먼저 걸어봄
}

void ATutorialManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr)
	{
		StopPetFly();
		bPetTurning = false;
		SetActorTickEnabled(false);
		return;
	}

	//! ---- 비행 ----
	if (true == bPetFlying)
	{
		FlyElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(FlyElapsed / FlyDuration, 0.f, 1.f);
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f); // 천천히 출발해서 천천히 착지

		// 직선 위치 + 포물선 높이 (가운데에서 가장 높고 양 끝에서 0)
		FVector Location = FMath::Lerp(FlyStart, FlyEnd, Eased);
		Location.Z += PetFlyArcHeight * 4.f * Eased * (1.f - Eased);
		PetPtr->SetActorLocation(Location);

		if (Alpha >= 1.f)
			StopPetFly(); // 착지	
	}

	//! ---- 플레이어 쪽으로 돌기 ----
	if (true == bPetTurning)
	{
		const FRotator Target(0.f, PetTurnTargetYaw, 0.f);
		const FRotator NewRot = FMath::RInterpConstantTo(PetPtr->GetActorRotation(), Target, DeltaSeconds, PetTurnSpeed);
		PetPtr->SetActorRotation(NewRot);

		// 거의 다 돌았으면 딱 맞추고 끝 (-180/180 경계도 NormalizeAxis 로 처리)
		if (FMath::Abs(FRotator::NormalizeAxis(NewRot.Yaw - PetTurnTargetYaw)) < 1.f)
		{
			PetPtr->SetActorRotation(Target);
			bPetTurning = false;
		}
	}

	// 할 일이 다 끝나면 Tick 끄기 -> 평소엔 비용 없음
	if (false == bPetFlying && false == bPetTurning)
		SetActorTickEnabled(false);
}

void ATutorialManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 매니저가 사라질 때 잠금이 남아 있으면 조작 불가 버그 → 무조건 정리
	EndCinematic(0.f);
	StopPetFly();
	bPetTurning = false;
	StopWaitPetArrive();
	Super::EndPlay(EndPlayReason);
}

void ATutorialManager::TryStartTutorial()
{
	// 1) 플레이어가 생기자마자 시작 연출 (카메라 + 잠금)
	// (펫을 기다리는 동안 원래 카메라가 보이지 읺게)
	if (false == bIntroApplied)
	{
		if (false == ApplyIntroCinematic()) return; // 플레이어가 아직 없는 상태 -> 다음 확인 때 다시
		bIntroApplied = true;
	}

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
	StopWaitPetArrive(); // 펫 도착 대기 중이었다면 정리
	EndCinematic(0.f);   // 구간이 바뀌면 연출은 무조건 끝 (잠금이 다음 구간으로 새지 않게)
	StopPetFly(); // 비행 도중 구간이 바뀌어도 펫이 공중에 남지 않게
	bPetTurning = false;
	Actions.Reset();
	ActionIndex = INDEX_NONE;
}

void ATutorialManager::BuildActions(ETutorialStage Stage, TArray<FTutorialAction>& OutActions)
{
	switch (Stage)
	{
	case ETutorialStage::Movement: BuildMovementActions(OutActions); break;
	case ETutorialStage::Dash: BuildDashActions(OutActions); break;
	default: break;
	}
}

void ATutorialManager::BuildMovementActions(TArray<FTutorialAction>& OutActions)
{
	// 이 구간에서 쓸 레벨 액터 찾기 (구간이 시작될 때 한 번)
	TWeakObjectPtr<AActor> CameraDoor01 = FindActorByTag(TutorialTag::CameraDoor01);

	AActor* TriggerDoor01 = FindActorByTag(TutorialTag::TriggerDoor01);
	TWeakObjectPtr<AActor> Gate01 = FindActorByTag(TutorialTag::Gate01);
	TWeakObjectPtr<AActor> DecalWASD = FindActorByTag(TutorialTag::DecalWASD);

	TWeakObjectPtr<AActor> PetStart = FindActorByTag(TutorialTag::PetPointStart);
	TWeakObjectPtr<AActor> PetDoor01 = FindActorByTag(TutorialTag::PetPointDoor01);
	TWeakObjectPtr<AActor> PetAfterDoor01 = FindActorByTag(TutorialTag::PetPointAfterDoor01);

	// 0) 시작 연출: 문 쪽 앵글 + 조작 잠금
	OutActions.Add(Do([this, CameraDoor01] { BeginCinematic(CameraDoor01.Get(), 0.f); }));

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
	OutActions.Add(WaitPetArrive(PetDoor01.Get())); // 펫이 문 앞에 도착할 때까지 (최대 8초)
	OutActions.Add(Do([this] { PetFacePlayer(); })); // 문 앞에 도착하면 플레이어 쪽으로 돌아봄
	OutActions.Add(WaitSeconds(0.2f));                // 돌아보는 모습이 보이고 나서 카메라 복귀

	// 3-1) 연출 끝 : 플레이어 카메라로 복귀 + 조작 되돌리기
	OutActions.Add(Do([this] {EndCinematic(CameraReturnBlendTime); }));
	
	// 4) 플레이어가 문 앞에 오면 문을 열고, 문이 어느 정도 열린 뒤 펫이 먼저 지나감
	OutActions.Add(WaitPlayerEnter(TriggerDoor01)); // (플레이어가 문 앞까지 올 때까지 멈춤)
	OutActions.Add(Do([this, Gate01] {SetActivated(Gate01.Get(), true); })); // 문 열기
	OutActions.Add(WaitSeconds(0.6f)); // 문짝이 다 열리기 전에 펫이 부딪히지 않게
	OutActions.Add(Do([this, PetAfterDoor01] {PetMoveTo(PetAfterDoor01.Get()); }));
}

void ATutorialManager::BuildDashActions(TArray<FTutorialAction>& OutActions)
{
	AActor* TriggerDashEnter = FindActorByTag(TutorialTag::TriggerDashEnter);
	AActor* TriggerDoor02 = FindActorByTag(TutorialTag::TriggerDoor02);

	TWeakObjectPtr<AActor> CamCorridor = FindActorByTag(TutorialTag::CameraDashCorridor);
	TWeakObjectPtr<AActor> PetDashEnd = FindActorByTag(TutorialTag::PetPointDashEnd);
	TWeakObjectPtr<AActor> PetAfterDoor02 = FindActorByTag(TutorialTag::PetPointAfterDoor02);
	TWeakObjectPtr<AActor> DecalShift = FindActorByTag(TutorialTag::DecalShift);
	TWeakObjectPtr<AActor> Gate02 = FindActorByTag(TutorialTag::Gate02);

	// 0) 구간은 이미 시작됐지만, 실제 연출은 플레이어가 대시 방에 들어와야 시작
	OutActions.Add(WaitPlayerEnter(TriggerDashEnter));

	// 1) (잠금) 통로 전체 샷으로 전환. 카메라가 다 도착한 뒤에 펫이 출발해야 비행 전체가 보임
	OutActions.Add(Do([this, CamCorridor] { BeginCinematic(CamCorridor.Get(), DashCameraBlendTime); }));
	OutActions.Add(WaitSeconds(DashCameraBlendTime));

	// 2) 카메라는 가만히, 펫이 레이저 위를 날아감 -> 플레이어 시선이 펫을 따라 레이저와 끝 지점을 봄
	OutActions.Add(Do([this, PetDashEnd] { PetFlyTo(PetDashEnd.Get(), DashFlyTime); }));
	OutActions.Add(WaitSeconds(DashFlyTime));

	// 3) 착지 직후 부르기 + Shift 데칼 ("날아가서 -> 저기서 부른다" 가 한 흐름으로 읽히게)
	OutActions.Add(Do([this] { PetFacePlayer(); })); // 착지하면 플레이어 쪽으로 돌아봄
	OutActions.Add(WaitSeconds(0.3f));                // 돌고 나서 "Dash!" (등 돌린 채 말하지 않게)
	OutActions.Add(Do([this] { PetSay(INVTEXT("Follow Me!")); })); 
	OutActions.Add(Do([this, DecalShift] { SetActivated(DecalShift.Get(), true); }));
	OutActions.Add(WaitSeconds(DashShowTime));

	// 4) (잠금 해제) 플레이어 카메라로 복귀
	OutActions.Add(Do([this] { EndCinematic(CameraReturnBlendTime); }));

	// 5) 플레이어가 통로 끝에 도착하면 문 열고 펫 통과
	OutActions.Add(WaitPlayerEnter(TriggerDoor02));
	OutActions.Add(Do([this, Gate02] { SetActivated(Gate02.Get(), true); }));
	OutActions.Add(WaitSeconds(0.6f)); // 문짝에 펫이 부딪히지 않게
	OutActions.Add(Do([this, PetAfterDoor02] { PetMoveTo(PetAfterDoor02.Get()); }));
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
			if (true == BeginWaitPlayerEnter(Action.Target.Get())) return;
			continue; // 트리거가 없거나 플레이어가 이미 안에 있으면 바로 다음 동작

		case ETutorialWait::PetArrive: // 펫이 지점에 도착할 떄 까지 멈춤 -> 도착하면 다음 동작
			if (true == BeginWaitPetArrive(Action.Target.Get(), Action.Seconds)) return;
			continue; // 펫이나 지점이 없거나 이미 도착해있으면 바로 다음 동작
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
	Action.Target = _Trigger;
	return Action;
}

FTutorialAction ATutorialManager::WaitPetArrive(AActor* _Point, float _Timeout)
{
	FTutorialAction Action;
	Action.Wait = ETutorialWait::PetArrive;
	Action.Target = _Point;
	Action.Seconds = _Timeout;
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
		Bubble->Say(_Message, 4.f);
}

void ATutorialManager::PetFlyTo(AActor* _Point, float _Duration)
{
	APetCharacter* PetPtr = Pet.Get();
	if(nullptr == PetPtr || nullptr == _Point)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetFlyTo: 펫 또는 지점이 없음"));
		return;
	}

	// 1) StateTree 를 Idle 로 비켜 세움 -> 날아가는 동안 StateTree 가 펫을 끌고 가지 않음
	PetPtr->RequestBehavior(EPetBehavior::Idle);
	if (AAIController* AIC = Cast<AAIController>(PetPtr->GetController()))
		AIC->StopMovement(); // 진행 중이던 길찾기 이동도 멈춤

	// 2) 중력 끄기 (캐릭터라 공중에 두면 매 프레임 떨어지려 함)
	UCharacterMovementComponent* Move = PetPtr->GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_Flying);

	// 3) Fly 애니메이션. RequestBehavior 가 Normal 로 되돌리므로 반드시 그 "뒤에"
	PetPtr->SetAnimationMovementStyle(EPetMovementStyle::Fly);

	// 비행 경로: 지점은 바닥 위치라, 캡슐 절반 높이만큼 올려야 바닥에 묻히지 않음
	FlyStart = PetPtr->GetActorLocation();
	FlyEnd = _Point->GetActorLocation();
	FlyEnd.Z += PetPtr->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FlyDuration = FMath::Max(_Duration, 0.1f);
	FlyElapsed = 0.f;

	// 도착 방향을 바라보게
	const FVector Dir = FlyEnd - FlyStart;
	PetPtr->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));

	bPetFlying = true;
	SetActorTickEnabled(true); // 비행 중에만 Tick
}

void ATutorialManager::StopPetFly()
{
	if (false == bPetFlying) return;
	bPetFlying = false;
	//SetActorTickEnabled(false);

	if (APetCharacter* PetPtr = Pet.Get())
	{
		// 원래대로: 걷기 모드 + Normal 애니메이션. 행동은 Idle 그대로 (도착 지점에서 기다림)
		PetPtr->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		PetPtr->SetAnimationMovementStyle(EPetMovementStyle::Normal);
	}
}

void ATutorialManager::PetFacePlayer()
{
	APetCharacter* PetPtr = Pet.Get();
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (nullptr == PetPtr || nullptr == PlayerPawn) return;

	// 도착 반경 안에 들어왔어도 마지막 몇 cm 를 걷는 중일 수 있음
	// -> 걸으면 "이동 방향 바라보기"가 회전을 덮어쓰니, 먼저 확실히 세워둠
	PetPtr->RequestBehavior(EPetBehavior::Idle);
	if (AAIController* AIC = Cast<AAIController>(PetPtr->GetController()))
		AIC->StopMovement();

	// 높이 차이는 무시하고 수평 방향만 (위아래로 고개 꺾이지 않게)
	FVector ToPlayer = PlayerPawn->GetActorLocation() - PetPtr->GetActorLocation();
	ToPlayer.Z = 0.f;
	if (true == ToPlayer.IsNearlyZero()) return;

	PetTurnTargetYaw = ToPlayer.Rotation().Yaw;
	bPetTurning = true;
	SetActorTickEnabled(true);
}

bool ATutorialManager::BeginWaitPetArrive(AActor* _Point, float _TimeOut)
{
	APetCharacter* PetPtr = Pet.Get();
	if(nullptr == PetPtr || nullptr == _Point)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 펫 또는 도착 지점이 없어 건너뜀"));
		return false;
	}

	// 이미 도착해있으면 바로 통과
	if (FVector::Dist2D(PetPtr->GetActorLocation(), _Point->GetActorLocation()) <= PetArriveRadius)
		return false;

	WaitingPetPoint = _Point;
	PetArriveTimeLeft = _TimeOut;

	// 0.1초 마다 거리를 확인
	GetWorldTimerManager().SetTimer(PetArriveTimerHandle, this, &ATutorialManager::CheckPetArrive, 0.1f, true);
	return true;
}

void ATutorialManager::CheckPetArrive()
{
	APetCharacter* PetPtr = Pet.Get();
	AActor* Point = WaitingPetPoint.Get();

	// 높이(Z)는 무시: 지점 액터와 펫 캡슐 중심 높이가 달라서
	const bool bArrived = (nullptr == PetPtr || nullptr == Point)
		|| FVector::Dist2D(PetPtr->GetActorLocation(), Point->GetActorLocation()) <= PetArriveRadius;

	PetArriveTimeLeft -= 0.1f;
	if (false == bArrived && PetArriveTimeLeft > 0.f)
		return; // 아직 가는 중 -> 다음 확인

	// 시간 초과 : 펫이 길에 막혀도 조작이 영원히 잠기지 않게 진행
	if(false == bArrived)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 펫 도착 대기 시간 초과 -> 그냥 진행"));
	}

	StopWaitPetArrive();
	RunNextAction();
}

void ATutorialManager::StopWaitPetArrive()
{
	GetWorldTimerManager().ClearTimer(PetArriveTimerHandle);
	WaitingPetPoint.Reset();
}

bool ATutorialManager::ApplyIntroCinematic()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (nullptr == PC || nullptr == PC->GetPawn())
		return false;

	// 이동 구간 시작할 떄만
	if (ETutorialStage::Movement == StartStage)
		BeginCinematic(FindActorByTag(TutorialTag::CameraDoor01), 0.f);

	return true;
}

void ATutorialManager::BeginCinematic(AActor* _Camera, float _BlendTime)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (nullptr == PC) return;

	// 이미 연출 중이면 잠금은 그대로, 카메라만 바꿈
	if (false == bInCinematic)
	{
		SetPlayerInputLocked(PC, true);
		bInCinematic = true;
	}

	// 카메라 못 찾아도 잠금은 유지
	if(nullptr == _Camera)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 연출 카메라가 없어 플레이어 카메라 유지"));
		return;
	}

	// 이미 그 카메라를 보고 있으면 다시 전환핮 ㅣ 않음
	if (PC->GetViewTarget() != _Camera)
		PC->SetViewTargetWithBlend(_Camera, _BlendTime, VTBlend_Cubic); // Cubic 천천히 출발해서 천천히 멈춤 
}

void ATutorialManager::EndCinematic(float _BlendTime)
{
	if (false == bInCinematic) return; // 연출중이 아니라면 할일 읎음
	bInCinematic = false;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (nullptr == PC)
	{
		LockedPawn.Reset();
		return;
	}

	// 플레이어 카메라로 복귀
	if (APawn* PlayerPawn = PC->GetPawn())
		PC->SetViewTargetWithBlend(PlayerPawn, _BlendTime, VTBlend_Cubic);

	SetPlayerInputLocked(PC, false);
}

void ATutorialManager::SetPlayerInputLocked(APlayerController* _PC, bool _bLock)
{
	// 캐릭터 입력 = 이동/대시/공격/락온, 
	// 컨트롤러 입력 = 펫 다이얼/펫 모드/펫 특수 행동
	// 둘 다 꺼야 모든 플레이어 입력이 막힘!!

	if (true == _bLock)
	{
		LockedPawn = _PC->GetPawn();
		if (APawn* PlayerPawn = LockedPawn.Get())
			PlayerPawn->DisableInput(_PC);
		_PC->DisableInput(_PC);
	}
	else
	{
		// 잠갔던 그 캐릭터만 켬 (리스폰으로 이미 사라졌으면 건너뜀. 새 캐릭터는 원래 켜져 있음)
		if (APawn* PlayerPawn = LockedPawn.Get())
			PlayerPawn->EnableInput(_PC);
		LockedPawn.Reset();
		_PC->EnableInput(_PC);
	}
}

void ATutorialManager::HandlePlayerRespawned(AController* _Controller, APawn* _NewPawn)
{
	// 컨트롤러는 리스폰돼도 그대로라, 꺼둔 입력이 남으면 새 캐릭터도 조작 불가 -> 강제 해제
	EndCinematic(0.f);
}
