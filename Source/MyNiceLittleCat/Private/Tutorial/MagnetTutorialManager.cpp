#include "Tutorial/MagnetTutorialManager.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Pet/PetCharacter.h"
#include "Pet/SpeechBubbleComponent.h"
#include "Gimmicks/Stage/GimmickPressurePlate.h"
#include "Items/InventorySubsystem.h"
#include "Items/ItemAbilityTags.h"
#include "Kismet/GameplayStatics.h"
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

	// 펫은 GameMode가 나중에 스폰/초기화하므로 준비될 때까지 반복 확인
	GetWorldTimerManager().SetTimer(StartTimerHandle, this, &AMagnetTutorialManager::TryStart, 0.2f, true);
}

void AMagnetTutorialManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

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
	APetCharacter* FoundPet = FindPet();
	if (false == IsValid(FoundPet) || false == FoundPet->IsPetInitialized()) return;
	if (nullptr == UGameplayStatics::GetPlayerPawn(this, 0)) return;

	Pet = FoundPet;
	GetWorldTimerManager().ClearTimer(StartTimerHandle);

	// 문이 이미 열린 상태(체크포인트 재시작 등)면 튜토리얼 없이 끝
	if (IsValid(Plate) && true == Plate->IsActivated())
	{
		CurrentStep = EMagnetTutorialStep::Completed;
		return;
	}

	// 이미 자성을 가진 상태면 획득 단계는 건너뜀
	EnterStep(FoundPet->IsModeUnlocked(EPetMode::Magnet) ? EMagnetTutorialStep::HoldField : EMagnetTutorialStep::Acquire);
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
		P->RequestMode(EPetMode::Magnet);             // 획득 단계를 건너뛰고 들어온 경우 대비
		P->LockMagnetPolarity(EPetMagnetPolarity::N); // Q로 미리 바꿔서 단계를 건너뛰지 못하게
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
		PetSay(LOCTEXT("Done", "문이 열렸다! 이제 같이 가자!"), 3.f);
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
