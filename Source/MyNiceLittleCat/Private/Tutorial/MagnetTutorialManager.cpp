#include "Tutorial/MagnetTutorialManager.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Pet/PetCharacter.h"
#include "Pet/SpeechBubbleComponent.h"
#include "Gimmicks/Stage/GimmickPressurePlate.h"
#include "Items/InventorySubsystem.h"
#include "Items/ItemAbilityTags.h"
#include "Interfaces/ActivatableInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MagnetTutorial"

AMagnetTutorialManager::AMagnetTutorialManager()
{
	PrimaryActorTick.bCanEverTick = false; // 타이머로만 확인
}

void AMagnetTutorialManager::BeginPlay()
{
	Super::BeginPlay();

	// 구슬(또는 상자) 획득 연출이 끝나는 순간을 받음
	if (UGameInstance* GI = GetGameInstance())
		if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
			Inventory->OnAbilityAcquireFinished.AddUniqueDynamic(this, &AMagnetTutorialManager::HandleAbilityAcquireFinished);

	// 시작 트리거 감시 (끄면 바로 시작 가능 상태)
	if (false == bStartOnTrigger)
	{
		bPlayerEntered = true;
	}
	else
	{
		TArray<AActor*> FoundTriggers;
		UGameplayStatics::GetAllActorsWithTag(this, StartTriggerTag, FoundTriggers);

		if (true == FoundTriggers.IsEmpty())
			UE_LOG(LogTemp, Warning, TEXT("MagnetTutorial: '%s' 태그가 붙은 시작 트리거가 없어요."), *StartTriggerTag.ToString());

		for (AActor* Trigger : FoundTriggers)
		{
			Trigger->OnActorBeginOverlap.AddUniqueDynamic(this, &AMagnetTutorialManager::HandleStartTriggerOverlap);
			StartTriggers.Add(Trigger);

			// 플레이어가 트리거 안에서 시작하면(체크포인트 재시작 등) 겹침 시작 알림을 놓치므로 한 번 직접 확인
			TArray<AActor*> AlreadyOverlapping;
			Trigger->GetOverlappingActors(AlreadyOverlapping, APawn::StaticClass());
			for (AActor* Actor : AlreadyOverlapping)
				HandleStartTriggerOverlap(Trigger, Actor);
		}
	}

	// 펫은 GameMode가 나중에 스폰/초기화하므로 준비될 때까지 반복 확인 (트리거에 들어오기 전엔 대기)
	GetWorldTimerManager().SetTimer(StartTimerHandle, this, &AMagnetTutorialManager::TryStart, 0.2f, true);
}

void AMagnetTutorialManager::HandleStartTriggerOverlap(AActor* _overlappedActor, AActor* _otherActor)
{
	if (true == bPlayerEntered) return;

	// 플레이어만 시작시킴 (펫이 먼저 들어와도 무시)
	const APawn* Pawn = Cast<APawn>(_otherActor);
	if (nullptr == Pawn || false == Pawn->IsPlayerControlled()) return;

	bPlayerEntered = true;
	UnbindStartTriggers(); // 한 번만 쓰므로 바로 해제
}

void AMagnetTutorialManager::UnbindStartTriggers()
{
	for (const TWeakObjectPtr<AActor>& Entry : StartTriggers)
		if (AActor* Trigger = Entry.Get())
			Trigger->OnActorBeginOverlap.RemoveDynamic(this, &AMagnetTutorialManager::HandleStartTriggerOverlap);

	StartTriggers.Reset();
}

void AMagnetTutorialManager::SetFenceGatesClosed(bool _bClosed)
{
	TArray<AActor*> Gates;
	UGameplayStatics::GetAllActorsWithTag(this, FenceGateTag, Gates);

	if (true == Gates.IsEmpty())
		UE_LOG(LogTemp, Warning, TEXT("MagnetTutorial: '%s' 태그가 붙은 울타리 입구가 없어요."), *FenceGateTag.ToString());

	// 입구 쪽 클래스를 몰라도 되게 IActivatableInterface 로 호출 (Activate = 막기, Deactivate = 열기)
	for (AActor* Gate : Gates)
	{
		if (false == IsValid(Gate) || false == Gate->Implements<UActivatableInterface>()) continue;

		if (true == _bClosed)
			IActivatableInterface::Execute_Activate(Gate, this);
		else
			IActivatableInterface::Execute_Deactivate(Gate, this);
	}
}

void AMagnetTutorialManager::BringPetInsideFence()
{
	APetCharacter* P = Pet.Get();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (nullptr == P || nullptr == Player) return;

	TArray<AActor*> Gates;
	UGameplayStatics::GetAllActorsWithTag(this, FenceGateTag, Gates);
	if (true == Gates.IsEmpty()) return;

	// 가림막의 오른쪽(로컬 Y) = 울타리 줄의 앞뒤 방향 (가림막이 울타리 줄을 따라 놓여 있으므로)
	const AActor* Gate = Gates[0];
	const FVector Normal = Gate->GetActorRightVector();
	const FVector GateLocation = Gate->GetActorLocation();

	const float PlayerSide = FVector::DotProduct(Player->GetActorLocation() - GateLocation, Normal);
	const float PetSide = FVector::DotProduct(P->GetActorLocation() - GateLocation, Normal);

	// 같은 쪽이고 울타리에서 충분히 떨어져 있으면 그대로 둠
	const bool bSameSide = (PlayerSide * PetSide) > 0.f;
	if (true == bSameSide && FMath::Abs(PetSide) > PetFenceMargin) return;

	// 울타리 밖이면 날아서 플레이어 옆으로 넘어오게 함 (펫의 날아서 복귀 행동 재사용)
	// 의도: 순간이동보다 자연스럽고, 나는 동안 충돌이 꺼져 있어 울타리 위로 넘어옴
	P->RequestBehavior(EPetBehavior::RecoverFollow);
}

void AMagnetTutorialManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	// 매니저가 먼저 사라져도 트리거가 사라진 객체를 부르지 않게 바인딩 해제
	UnbindStartTriggers();

	if (UGameInstance* GI = GetGameInstance())
		if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
			Inventory->OnAbilityAcquireFinished.RemoveDynamic(this, &AMagnetTutorialManager::HandleAbilityAcquireFinished);

	// 튜토리얼 도중 끝나도 펫 극 고정이 남지 않게
	if (APetCharacter* P = Pet.Get())
		P->UnlockMagnetPolarity();

	Super::EndPlay(EndPlayReason);
}

void AMagnetTutorialManager::TryStart()
{
	if (false == bPlayerEntered) return; // TriggerBoxMagnetMode에 들어올 때까지 대기

	APetCharacter* FoundPet = FindPet();
	if (false == IsValid(FoundPet) || false == FoundPet->IsPetInitialized()) return;
	if (nullptr == UGameplayStatics::GetPlayerPawn(this, 0)) return;

	Pet = FoundPet;
	GetWorldTimerManager().ClearTimer(StartTimerHandle);

	// 이미 자성을 가졌거나 문이 이미 열린 상태(체크포인트 재시작 등)면 튜토리얼 없이 끝
	// 의도: 이 스테이지에서 배울 능력을 이미 알고 있으면 다시 가르치지 않음
	//       (+ 세이브 이후 통과할 수 없는 2단계에 들어가 극이 N으로 잠기던 문제도 같이 막힘)
	if (true == FoundPet->IsModeUnlocked(EPetMode::Magnet) || (IsValid(Plate) && true == Plate->IsActivated()))
	{
		CurrentStep = EMagnetTutorialStep::Completed;
		FoundPet->UnlockMagnetPolarity();
		SetFenceGatesClosed(false); // 자성을 이미 가졌으면 입구는 절대 막지 않음 (혹시 닫혀 있어도 엶)
		return;
	}

	// 자성 없이 울타리 안(시작 트리거)에 들어옴 → 튜토리얼이 끝날 때까지 입구를 막아 구역 밖으로 못 나가게
	// 먼저 막고 펫을 날려 보냄
	// 의도: 날아오기는 펫 쪽 방향부터 착지점을 찾으므로, 입구를 먼저 막아 "플레이어가 보이는 자리" 검사에서 울타리 바깥이 걸러지게
	SetFenceGatesClosed(true);
	BringPetInsideFence();     // 펫이 밖에 남아 있으면 날아서 안으로 넘어오게

	// 3단계(Q 안내) 전까지 N극 고정을 쭉 유지 (자성 모드가 아닐 때 걸어도 유지됨)
	// 의도: 구슬을 먹은 직후나 2단계 도중에 Q로 S가 됐다가 강제로 N으로 되돌아가는 부자연스러운 순간을 없앰
	FoundPet->LockMagnetPolarity(EPetMagnetPolarity::N);
	EnterStep(EMagnetTutorialStep::Acquire);
	GetWorldTimerManager().SetTimer(CheckTimerHandle, this, &AMagnetTutorialManager::CheckStep, CheckInterval, true);
}

void AMagnetTutorialManager::EnterStep(EMagnetTutorialStep _step)
{
	CurrentStep = _step;
	bStepDone = false;

	APetCharacter* P = Pet.Get();
	if (nullptr == P) return;

	switch (_step)
	{
	case EMagnetTutorialStep::Acquire:
		PetSay(LOCTEXT("Acquire", "저기 반짝이는 구슬 보여? 가서 먹어봐!"));
		break;

	case EMagnetTutorialStep::HoldField:
		HoldFieldTime = 0.f;
		P->RequestMode(EPetMode::Magnet); // 획득 단계를 건너뛰고 들어온 경우 대비
		// 극은 TryStart 에서 건 N 고정이 그대로 유지됨 (Q로 미리 바꿔 단계를 건너뛰지 못하게)
		PetSay(LOCTEXT("Hold", "나 이제 자석이야! 상자 가까이 가서 마우스 우클릭을 꾹 누르고 있어봐!"));
		break;

	case EMagnetTutorialStep::SwapPolarity:
		P->UnlockMagnetPolarity();                    // 이제 Q 허용
		PetSay(LOCTEXT("Swap", "우클릭을 누른 채로 Q를 눌러봐! 내 극이 S로 바뀔 거야!"));
		break;

	case EMagnetTutorialStep::PlaceOnPlate:
		bWasCarrying = true;
		PetSay(LOCTEXT("Place", "발판 위까지 옮긴 다음, 우클릭을 떼서 내려놔!"));
		break;

	case EMagnetTutorialStep::Completed:
		GetWorldTimerManager().ClearTimer(CheckTimerHandle);
		// 의도: 2단계에서 발판이 먼저 켜져 3단계(잠금 해제)를 건너뛰어도 N 고정이 남지 않게
		P->UnlockMagnetPolarity();
		SetFenceGatesClosed(false); // 자성 튜토리얼을 다 배웠으니 입구를 열어 다시 자유롭게 드나들게
		//PetSay(LOCTEXT("Done", "문이 열렸다! 이제 같이 가자!"), 3.f);
		break;

	default:
		break;
	}
}

void AMagnetTutorialManager::CheckStep()
{
	if (true == bStepDone) return;

	APetCharacter* P = Pet.Get();
	if (nullptr == P) return;

	// 발판이 켜지면 어느 단계든 마무리 (순서를 건너뛰어 성공해도 튜토리얼이 남지 않게)
	if (CurrentStep >= EMagnetTutorialStep::HoldField && IsValid(Plate) && true == Plate->IsActivated())
	{
		CurrentStep = EMagnetTutorialStep::PlaceOnPlate;
		CompleteStep(LOCTEXT("PlaceOK", "잘했어! 같은 극은 밀어내고, 다른 극은 끌어당겨!"));
		return;
	}

	switch (CurrentStep)
	{
	case EMagnetTutorialStep::Acquire:
		if (true == bAcquireFinished)
		{
			P->RequestMode(EPetMode::Magnet); // 얻자마자 자성 모드로 → 모드 배지 갱신
			CompleteStep(LOCTEXT("AcquireOK", "자성 능력을 얻었어!"));
			return;
		}
		SayHint(LOCTEXT("AcquireHint", "앞에 있는 반짝이는 구슬을 먹어봐!"));
		break;

	case EMagnetTutorialStep::HoldField:
	{
		// 상자에서 멀리 떨어져 있으면 진행/힌트를 쉼 (예전과 같은 흐름)
		// 극 고정은 여기서 풀지 않음
		// 의도: 울타리가 튜토리얼이 끝날 때까지 구역을 막고 있어 밖의 다른 자성 기믹을 풀 일이 없으므로,
		//       거리에 따라 고정을 풀었다 다시 걸면서 S → N 으로 강제로 되돌아가던 문제만 없앰
		if (GetPlayerDistanceTo(MagnetBox) > PolarityLockDistance)
		{
			HoldFieldTime = 0.f;
			return; // 구역 밖에서는 힌트 말풍선도 쉼
		}

		if (EPetMode::Magnet != P->GetCurrentMode())
		{
			SayHint(LOCTEXT("ModeHint", "왼쪽 Ctrl을 눌러 다이얼에서 자성 모드를 골라줘!"));
			return;
		}

		// 박스 근처에서 우클릭을 잠깐 유지 → 같은 N극이라 안 붙는 걸 보여줌
		if (true == IsMagnetHeld() && GetPlayerDistanceTo(MagnetBox) <= NearBoxDistance)
		{
			HoldFieldTime += CheckInterval;
			if (HoldFieldTime >= HoldFieldRequiredTime)
			{
				CompleteStep(LOCTEXT("HoldOK", "어라? 안 붙네… 상자도 나도 N극이라 그래!"));
				return;
			}
		}
		else
		{
			HoldFieldTime = 0.f;
		}

		SayHint(IsMagnetHeld() ? LOCTEXT("HoldNearHint", "상자 가까이 가서 계속 누르고 있어!")
		                       : LOCTEXT("HoldHint", "상자 근처에서 마우스 우클릭을 꾹!"));
		break;
	}

	case EMagnetTutorialStep::SwapPolarity:
		if (true == IsCarryingBox())
		{
			CompleteStep(LOCTEXT("SwapOK", "붙었다! 다른 극끼리는 끌어당겨!"));
			return;
		}
		SayHint(IsMagnetHeld() ? LOCTEXT("SwapHint", "마우스로 상자 쪽을 가리키고 Q!")
		                       : LOCTEXT("SwapHoldHint", "우클릭은 계속 누르고 있어야 해!"));
		break;

	case EMagnetTutorialStep::PlaceOnPlate:
	{
		const bool bCarrying = IsCarryingBox();

		// 우클릭은 그대로인데 박스가 떨어짐 = Q로 같은 극이 돼서 튕겨 나감
		if (true == bWasCarrying && false == bCarrying && true == IsMagnetHeld())
			PetSay(LOCTEXT("Repel", "앗! 같은 극이 되면 튕겨나가! Q로 다시 바꿔서 잡아봐!"));
		else if (false == bCarrying)
			SayHint(LOCTEXT("PickAgain", "우클릭을 누르고 Q로 극을 맞춰서 다시 들어 올려봐!"));
		else
			SayHint(LOCTEXT("PlaceHint", "발판 위에서 우클릭을 떼면 내려놓을 수 있어!"));

		bWasCarrying = bCarrying;
		break;
	}

	default:
		break;
	}
}

void AMagnetTutorialManager::CompleteStep(const FText& _praise)
{
	bStepDone = true;
	PetSay(_praise, StepDelay + 0.5f);

	// 칭찬을 잠깐 보여준 뒤 다음 단계로
	const EMagnetTutorialStep Next = static_cast<EMagnetTutorialStep>(static_cast<uint8>(CurrentStep) + 1);
	const FTimerDelegate NextDelegate = FTimerDelegate::CreateUObject(this, &AMagnetTutorialManager::EnterStep, Next);
	GetWorldTimerManager().SetTimer(NextStepTimerHandle, NextDelegate, FMath::Max(0.01f, StepDelay), false);
}

void AMagnetTutorialManager::PetSay(const FText& _message, float _duration)
{
	if (APetCharacter* P = Pet.Get())
		if (USpeechBubbleComponent* Bubble = P->GetSpeechBubble())
			Bubble->Say(_message, _duration > 0.f ? _duration : InstructionDuration);

	LastSayTime = GetWorld()->GetTimeSeconds();
}

void AMagnetTutorialManager::SayHint(const FText& _hint)
{
	// 방금 말했으면 참기 (말풍선 도배 방지)
	if (GetWorld()->GetTimeSeconds() - LastSayTime < HintInterval) return;
	PetSay(_hint);
}

APetCharacter* AMagnetTutorialManager::FindPet() const
{
	const AMainGameMode* GM = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	return GM ? GM->GetPet() : nullptr;
}

float AMagnetTutorialManager::GetPlayerDistanceTo(const AActor* _target) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0); // 리스폰 대비 매번 찾음
	if (nullptr == Player || false == IsValid(_target)) return TNumericLimits<float>::Max();
	return FVector::Dist2D(Player->GetActorLocation(), _target->GetActorLocation());
}

bool AMagnetTutorialManager::IsMagnetHeld() const
{
	const APetCharacter* P = Pet.Get();
	return nullptr != P && true == P->IsModeAbilityActive(EPetMode::Magnet);
}

bool AMagnetTutorialManager::IsCarryingBox() const
{
	const APetCharacter* P = Pet.Get();
	return nullptr != P && IsValid(MagnetBox) && P->GetCarriedActor() == MagnetBox;
}

void AMagnetTutorialManager::HandleAbilityAcquireFinished(FGameplayTag _abilityTag, UItemData* _item)
{
	if (true == _abilityTag.MatchesTagExact(ItemAbilityTags::Ability_Pet_Magnet))
		bAcquireFinished = true; // 다음 CheckStep에서 1단계 완료
}

#undef LOCTEXT_NAMESPACE
