#include "Tutorial/ReflectTutorialManager.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Pet/PetCharacter.h"
#include "Pet/SpeechBubbleComponent.h"
#include "Pet/GAS/AbilityObject/PetReflector.h"
#include "Gimmicks/GimmickActorBase.h"
#include "Items/InventorySubsystem.h"
#include "Items/ItemAbilityTags.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "ReflectTutorial"

AReflectTutorialManager::AReflectTutorialManager()
{
	PrimaryActorTick.bCanEverTick = false; // 타이머로만 확인

	// 시작 트리거: 겹침만 감지 (길을 막지 않음). 크기는 레벨에서 통로 폭에 맞춰 조절
	StartTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("StartTrigger"));
	SetRootComponent(StartTrigger);
	StartTrigger->SetCollisionProfileName(TEXT("Trigger"));
	StartTrigger->InitBoxExtent(FVector(400.f, 100.f, 200.f));
	StartTrigger->ShapeColor = FColor::Cyan; // 에디터에서 다른 트리거와 구분되게
}

void AReflectTutorialManager::BeginPlay()
{
	Super::BeginPlay();

	// 구슬 획득 연출이 끝나는 순간을 받음
	if (UGameInstance* GI = GetGameInstance())
		if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
			Inventory->OnAbilityAcquireFinished.AddUniqueDynamic(this, &AReflectTutorialManager::HandleAbilityAcquireFinished);

	// 시작 트리거 감시 (끄면 바로 시작 가능 상태)
	if (false == bStartOnTrigger)
	{
		bPlayerEntered = true;
	}
	else
	{
		StartTrigger->OnComponentBeginOverlap.AddDynamic(this, &AReflectTutorialManager::HandleStartTriggerOverlap);

		// 플레이어가 트리거 안에서 시작하면(체크포인트 재시작 등) 겹침 시작 알림을 놓치므로 한 번 직접 확인
		StartTrigger->UpdateOverlaps();
		TArray<AActor*> AlreadyOverlapping;
		StartTrigger->GetOverlappingActors(AlreadyOverlapping, APawn::StaticClass());
		for (AActor* Actor : AlreadyOverlapping)
			HandleStartTriggerOverlap(StartTrigger, Actor, nullptr, INDEX_NONE, false, FHitResult());
	}

	// 펫은 GameMode가 나중에 스폰/초기화하므로 준비될 때까지 반복 확인 (트리거에 들어오기 전엔 대기)
	GetWorldTimerManager().SetTimer(StartTimerHandle, this, &AReflectTutorialManager::TryStart, 0.2f, true);
}

void AReflectTutorialManager::HandleStartTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (true == bPlayerEntered) return;

	// 플레이어만 시작시킴 (펫이 먼저 들어와도 무시)
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (nullptr == Pawn || false == Pawn->IsPlayerControlled()) return;

	bPlayerEntered = true;
	StartTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 한 번만 쓰므로 이후 겹침 계산 생략
}

void AReflectTutorialManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	if (UGameInstance* GI = GetGameInstance())
		if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
			Inventory->OnAbilityAcquireFinished.RemoveDynamic(this, &AReflectTutorialManager::HandleAbilityAcquireFinished);

	// 반사판이 튜토리얼보다 오래 남아도 사라진 객체를 부르지 않게 바인딩 해제
	for (const TWeakObjectPtr<AActor>& Entry : TrackedReflectors)
		if (AActor* Reflector = Entry.Get())
			Reflector->OnEndPlay.RemoveDynamic(this, &AReflectTutorialManager::HandleReflectorEndPlay);
	TrackedReflectors.Reset();

	// 문이 매니저보다 오래 남아도 사라진 객체를 부르지 않게 바인딩만 해제 (레벨 종료 시 문은 열지 않음)
	if (IsValid(EntranceDoor))
		EntranceDoor->OnActivationChanged.RemoveDynamic(this, &AReflectTutorialManager::HandleEntranceDoorChanged);

	Super::EndPlay(EndPlayReason);
}

void AReflectTutorialManager::SetEntranceDoorLocked(bool _bLock)
{
	if (false == IsValid(EntranceDoor) || bEntranceDoorLocked == _bLock) return;
	bEntranceDoorLocked = _bLock;

	if (true == _bLock)
	{
		// 닫은 뒤에도 골목 수신기 신호로 다시 열리는지 감시
		EntranceDoor->OnActivationChanged.AddUniqueDynamic(this, &AReflectTutorialManager::HandleEntranceDoorChanged);
		EntranceDoor->SetActivated(false, this);
	}
	else
	{
		EntranceDoor->OnActivationChanged.RemoveDynamic(this, &AReflectTutorialManager::HandleEntranceDoorChanged);
		EntranceDoor->SetActivated(true, this);
	}
}

void AReflectTutorialManager::HandleEntranceDoorChanged(AGimmickActorBase* _gimmick, bool _bActivated, AActor* _changeInstigator)
{
	// 튜토리얼 중에는 어떤 신호로 열리든 다시 닫음
	if (true == bEntranceDoorLocked && true == _bActivated && IsValid(_gimmick))
		_gimmick->SetActivated(false, this);
}

void AReflectTutorialManager::TryStart()
{
	if (false == bPlayerEntered) return; // S2_Alley_Door를 지나 트리거에 들어올 때까지 대기

	APetCharacter* FoundPet = FindPet();
	if (false == IsValid(FoundPet) || false == FoundPet->IsPetInitialized()) return;
	if (nullptr == UGameplayStatics::GetPlayerPawn(this, 0)) return;

	Pet = FoundPet;
	GetWorldTimerManager().ClearTimer(StartTimerHandle);

	// 이미 반사 능력을 가졌거나 수신기가 이미 켜진 상태(체크포인트 재시작 등)면 튜토리얼 없이 끝
	// 의도: 이 스테이지에서 배울 능력을 이미 알고 있으면 다시 가르치지 않음
	if (true == FoundPet->IsModeUnlocked(EPetMode::Reflect) || true == IsReceiverOn())
	{
		CurrentStep = EReflectTutorialStep::Completed;
		return;
	}

	SetEntranceDoorLocked(true); // 튜토리얼 시작 → 입구 문 닫기
	EnterStep(EReflectTutorialStep::Acquire);
	GetWorldTimerManager().SetTimer(CheckTimerHandle, this, &AReflectTutorialManager::CheckStep, CheckInterval, true);
}

void AReflectTutorialManager::EnterStep(EReflectTutorialStep _step)
{
	CurrentStep = _step;
	bStepDone = false;

	APetCharacter* P = Pet.Get();
	if (nullptr == P) return;

	switch (_step)
	{
	case EReflectTutorialStep::Acquire:
		PetSay(LOCTEXT("Acquire", "저기 반짝이는 구슬 보여? 가서 먹어봐!"));
		break;

	case EReflectTutorialStep::Preview:
		PreviewTime = 0.f;
		P->RequestMode(EPetMode::Reflect); // 획득 단계를 건너뛰고 들어온 경우 대비
		PetSay(LOCTEXT("Preview", "마우스 쪽 바닥에 반투명한 반사판 보이지? 거기에 세울 수 있어!"));
		break;

	case EReflectTutorialStep::Place:
		TrackPetReflectors(); // 이전에 세운 반사판은 "새로 세움"으로 치지 않도록 미리 등록
		PetSay(LOCTEXT("Place", "우클릭을 누른 채로 끌어봐! 끈 방향으로 세워줄게!"));
		break;

	case EReflectTutorialStep::Remove:
		bRemovedByPlayer = false;
		PetSay(LOCTEXT("Remove", "치우고 싶으면 반사판에 마우스를 올리고 Q!"));
		break;

	case EReflectTutorialStep::Solve:
		PetSay(LOCTEXT("Solve", "레이저를 꺾어서 수신기로 보내봐!"));
		break;

	case EReflectTutorialStep::Completed:
		GetWorldTimerManager().ClearTimer(CheckTimerHandle);
		SetEntranceDoorLocked(false); // 튜토리얼 끝 → 입구 문 다시 열기
		//PetSay(LOCTEXT("Done", "문이 열렸다! 이제 같이 가자!"), 3.f);
		break;

	default:
		break;
	}
}

void AReflectTutorialManager::CheckStep()
{
	if (true == bStepDone) return;

	APetCharacter* P = Pet.Get();
	if (nullptr == P) return;

	// 수신기가 켜지면 어느 단계든 마무리 (순서를 건너뛰어 성공해도 튜토리얼이 남지 않게)
	if (CurrentStep >= EReflectTutorialStep::Preview && true == IsReceiverOn())
	{
		CurrentStep = EReflectTutorialStep::Solve;
		CompleteStep(LOCTEXT("SolveOK", "성공! 빛은 반사판에 맞으면 꺾여!"));
		return;
	}

	// 획득 이후 단계는 반사 모드여야 진행 가능
	if (CurrentStep >= EReflectTutorialStep::Preview && false == IsReflectMode())
	{
		SayHint(LOCTEXT("ModeHint", "왼쪽 Ctrl을 눌러 다이얼에서 반사 모드를 골라줘!"));
		return;
	}

	switch (CurrentStep)
	{
	case EReflectTutorialStep::Acquire:
		if (true == bAcquireFinished)
		{
			P->RequestMode(EPetMode::Reflect); // 얻자마자 반사 모드로 → 프리뷰가 바로 보이게
			CompleteStep(LOCTEXT("AcquireOK", "반사 능력을 얻었어!"));
			return;
		}
		SayHint(LOCTEXT("AcquireHint", "앞에 있는 반짝이는 구슬을 먹어봐!"));
		break;

	case EReflectTutorialStep::Preview:
		// 설명을 읽고 마우스를 움직여 볼 시간을 준 뒤 다음으로
		PreviewTime += CheckInterval;
		if (PreviewTime >= PreviewLookTime)
		{
			CompleteStep(LOCTEXT("PreviewOK", "못 세우는 곳에선 빨갛게 변해!"));
			return;
		}
		break;

	case EReflectTutorialStep::Place:
		if (TrackPetReflectors() > 0)
		{
			CompleteStep(LOCTEXT("PlaceOK", "잘했어! 화살표 쪽이 반사판 앞면이야!"));
			return;
		}
		SayHint(LOCTEXT("PlaceHint", "우클릭을 꾹 누른 채로 끌었다가 놓아봐!"));
		break;

	case EReflectTutorialStep::Remove:
		TrackPetReflectors(); // 새로 세운 반사판도 철거 감시
		if (true == bRemovedByPlayer)
		{
			CompleteStep(LOCTEXT("RemoveOK", "깔끔해! 개수 제한이 있으니 안 쓰는 건 치워줘!"));
			return;
		}
		// 반사판은 수명이 짧아서, 사라졌으면 다시 세우라고 알려줌
		SayHint(TrackedReflectors.IsEmpty()
			? LOCTEXT("RemovePlaceHint", "반사판을 세운 다음 마우스를 올리고 Q!")
			: LOCTEXT("RemoveHint", "마우스를 올리면 Q 표시가 떠! 그때 Q!"));
		break;

	case EReflectTutorialStep::Solve:
		SayHint(LOCTEXT("SolveHint", "레이저 길에 반사판을 비스듬히 세워봐!"));
		break;

	default:
		break;
	}
}

void AReflectTutorialManager::CompleteStep(const FText& _praise)
{
	bStepDone = true;
	PetSay(_praise, StepDelay + 0.5f);

	// 칭찬을 잠깐 보여준 뒤 다음 단계로
	const EReflectTutorialStep Next = static_cast<EReflectTutorialStep>(static_cast<uint8>(CurrentStep) + 1);
	const FTimerDelegate NextDelegate = FTimerDelegate::CreateUObject(this, &AReflectTutorialManager::EnterStep, Next);
	GetWorldTimerManager().SetTimer(NextStepTimerHandle, NextDelegate, FMath::Max(0.01f, StepDelay), false);
}

void AReflectTutorialManager::PetSay(const FText& _message, float _duration)
{
	if (APetCharacter* P = Pet.Get())
		if (USpeechBubbleComponent* Bubble = P->GetSpeechBubble())
			Bubble->Say(_message, _duration > 0.f ? _duration : InstructionDuration);

	LastSayTime = GetWorld()->GetTimeSeconds();
}

void AReflectTutorialManager::SayHint(const FText& _hint)
{
	// 방금 말했으면 참기 (말풍선 도배 방지)
	if (GetWorld()->GetTimeSeconds() - LastSayTime < HintInterval) return;
	PetSay(_hint);
}

APetCharacter* AReflectTutorialManager::FindPet() const
{
	const AMainGameMode* GM = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	return GM ? GM->GetPet() : nullptr;
}

bool AReflectTutorialManager::IsReflectMode() const
{
	const APetCharacter* P = Pet.Get();
	return nullptr != P && EPetMode::Reflect == P->GetCurrentMode();
}

bool AReflectTutorialManager::IsReceiverOn() const
{
	return IsValid(Receiver) && true == Receiver->IsActivated();
}

int32 AReflectTutorialManager::TrackPetReflectors()
{
	// 사라진 반사판 정리
	for (auto It = TrackedReflectors.CreateIterator(); It; ++It)
		if (false == It->IsValid())
			It.RemoveCurrent();

	int32 NewCount = 0;
	APetCharacter* P = Pet.Get();
	if (nullptr == P) return 0;

	// 펫이 설치한 반사판(Owner = 펫)만 감시 대상
	for (TActorIterator<APetReflector> It(GetWorld()); It; ++It)
	{
		APetReflector* Reflector = *It;
		if (false == IsValid(Reflector) || Reflector->GetOwner() != P || true == Reflector->IsActorBeingDestroyed()) continue;
		if (true == TrackedReflectors.Contains(Reflector)) continue;

		Reflector->OnEndPlay.AddUniqueDynamic(this, &AReflectTutorialManager::HandleReflectorEndPlay);
		TrackedReflectors.Add(Reflector);
		++NewCount;
	}

	return NewCount;
}

void AReflectTutorialManager::HandleReflectorEndPlay(AActor* _actor, EEndPlayReason::Type _reason)
{
	TrackedReflectors.Remove(_actor);

	// 수명 타이머가 끝나서 사라질 땐 남은 수명이 0(타이머 실행 중), Q로 철거하면 수명이 남아 있음
	// 의도: 철거 입력을 따로 받지 않고 결과만으로 판단 (반사 능력 코드 수정 없이)
	//       레벨 종료 등 Destroy가 아닌 이유로 사라지는 건 철거로 치지 않음
	if (EReflectTutorialStep::Remove == CurrentStep && nullptr != _actor
		&& EEndPlayReason::Destroyed == _reason
		&& _actor->GetLifeSpan() > RemoveLifeSpanThreshold)
	{
		bRemovedByPlayer = true; // 다음 CheckStep에서 4단계 완료
	}
}

void AReflectTutorialManager::HandleAbilityAcquireFinished(FGameplayTag _abilityTag, UItemData* _item)
{
	if (true == _abilityTag.MatchesTagExact(ItemAbilityTags::Ability_Pet_Reflect))
		bAcquireFinished = true; // 다음 CheckStep에서 1단계 완료
}

#undef LOCTEXT_NAMESPACE
