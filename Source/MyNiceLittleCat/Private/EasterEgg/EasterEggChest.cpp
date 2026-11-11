/// 작성자: 우진
/// 작성일자: 2026-11-09
///
/// 이스터에그 상자 구현

#include "EasterEgg/EasterEggChest.h"
#include "Pet/PetCharacter.h"
#include "Pet/SpeechBubbleComponent.h"
#include "Tutorial/TutorialPortal.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

AEasterEggChest::AEasterEggChest()
{
	// 잠금 해제(관문 열림)를 확인할 때만 Tick. 자주 볼 필요 없어 0.1초 간격, 풀리면 끔
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	// 이스터에그라 흐름을 끊지 않음: 일시정지·획득 UI 없이 뚜껑만 열림
	bPauseGameDuringAcquire = false;
	bShowAcquireUI = false;

	PromptText = NSLOCTEXT("EasterEgg", "ChestPrompt", "열기");

	// 기본 대사 2개 = 두 번 헛탕 친 뒤 세 번째부터 열 수 있음
	FakeAttemptLines.Add(NSLOCTEXT("EasterEgg", "ChestFake1", "음... 여긴 아무것도 없는 것 같아."));
	FakeAttemptLines.Add(NSLOCTEXT("EasterEgg", "ChestFake2", "진짜... 진짜로 아무것도 없는 것 같은데?"));
}

void AEasterEggChest::BeginPlay()
{
	Super::BeginPlay();

	// 관문이 없으면 처음부터 열림, 있으면 관문이 열릴 때까지 상호작용 끔 (감지도 안 돼서 그냥 장식처럼 보임)
	bUnlocked = (UnlockPortal == nullptr);
	SetInteractionEnabled(bUnlocked);
	SetActorTickEnabled(!bUnlocked);
}

void AEasterEggChest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 관문이 다 열린 뒤부터 UnlockDelay를 셈
	if (bUnlocked || !UnlockPortal || !UnlockPortal->IsOpen())
	{
		return;
	}

	UnlockElapsed += DeltaSeconds;
	if (UnlockElapsed >= UnlockDelay)
	{
		bUnlocked = true;
		SetInteractionEnabled(true);
		SetActorTickEnabled(false); // 더 확인할 게 없음
		UE_LOG(LogTemp, Log, TEXT("[EasterEggChest] %s 잠금 해제 (관문 열림)"), *GetName());
	}
}

void AEasterEggChest::Interact_Implementation(AActor* InteractInstigator)
{
	// 이미 열렸거나 가짜 시도를 다 채웠으면 원래 상자처럼 열기
	if (IsOpened() || IsRevealed())
	{
		Super::Interact_Implementation(InteractInstigator);
		return;
	}

	// 아직 헛탕 단계: 열지 않고 괴냥이 대사만 (괴냥이는 플레이어를 따라다니는 중이라 근처에서 말함)
	const int32 AttemptIndex = FakeAttemptCount++;
	PetSay(this, FakeAttemptLines[AttemptIndex], FakeLineDuration);
	UE_LOG(LogTemp, Log, TEXT("[EasterEggChest] %s 가짜 시도 %d/%d"), *GetName(), FakeAttemptCount, FakeAttemptLines.Num());

	// 마지막 헛탕이었으면 잠깐 상호작용을 막아서, 대사를 읽을 틈을 준 뒤 E 안내가 나타나게
	if (IsRevealed())
	{
		InteractBlockedUntil = GetWorld()->GetTimeSeconds() + RevealInteractDelay;
	}

	K2_OnFakeAttempt(InteractInstigator, AttemptIndex);
}

bool AEasterEggChest::CanInteract_Implementation(AActor* InteractInstigator) const
{
	// 막힌 동안은 감지 대상에서 빠짐 → E 안내도 안 뜨고, 시간이 지나면 다음 감지 때 다시 잡힘
	if (GetWorld() && GetWorld()->GetTimeSeconds() < InteractBlockedUntil)
	{
		return false;
	}
	return Super::CanInteract_Implementation(InteractInstigator);
}

FInteractPromptInfo AEasterEggChest::GetInteractPrompt_Implementation(AActor* InteractInstigator) const
{
	FInteractPromptInfo Result = Super::GetInteractPrompt_Implementation(InteractInstigator);

	// 헛탕 단계에서는 E 안내를 숨겨서 그냥 장식처럼 보이게 (E 입력 자체는 받음)
	if (!IsRevealed())
	{
		Result.bShowPrompt = false;
	}
	return Result;
}

APetCharacter* AEasterEggChest::FindPet(const UObject* WorldContextObject)
{
	// 보통은 게임모드가 펫을 들고 있음
	if (const AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(WorldContextObject)))
	{
		if (APetCharacter* Pet = GameMode->GetPet())
		{
			return Pet;
		}
	}

	// 다른 게임모드로 테스트할 때를 위한 대비: 레벨에서 직접 찾기
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (World)
	{
		for (TActorIterator<APetCharacter> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void AEasterEggChest::PetSay(const UObject* WorldContextObject, const FText& Message, float Duration)
{
	if (Message.IsEmpty())
	{
		return;
	}

	APetCharacter* Pet = FindPet(WorldContextObject);
	USpeechBubbleComponent* Bubble = Pet ? Pet->GetSpeechBubble() : nullptr;
	if (!Bubble)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EasterEgg] 괴냥이(말풍선)를 찾지 못함: %s"), *Message.ToString());
		return;
	}

	Bubble->Say(Message, Duration);
}
