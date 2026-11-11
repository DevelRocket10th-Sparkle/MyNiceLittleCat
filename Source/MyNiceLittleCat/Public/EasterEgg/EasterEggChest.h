/// 작성자: 우진
/// 작성일자: 2026-11-09
///
/// 이스터에그 상자입니다 (튜토리얼 레벨 Chest_B 자리).
/// 의도: "그냥 장식인 줄 알았던 상자"를 끈질기게 조사한 플레이어만 열 수 있게 합니다.
///   - 잠금: UnlockPortal(튜토리얼 끝 관문)이 열리기 전(= 보스를 잡기 전)에는 상호작용 자체가 꺼져 있어 진짜 장식처럼 보입니다.
///     관문이 열리고 UnlockDelay가 지나면 상호작용이 켜집니다. 관문을 비우면 처음부터 켜져 있습니다.
///   - 헛탕: 처음 FakeAttemptLines 개수만큼은 E를 눌러도 열리지 않고, 괴냥이가 순서대로 대사를 합니다.
///     이때는 E 안내 UI도 숨깁니다. 다 채우면 E 안내가 보이고 다음 E에 뚜껑이 열립니다.
/// 괴냥이 대사: 튜토리얼이 끝나면 괴냥이가 플레이어를 따라다니므로(TutorialManager) 그 자리에서 바로 말풍선을 띄웁니다.
/// 열기 자체(뚜껑 회전, 감지 영역, 열린 상태 저장)는 AGimmickChest를 그대로 씁니다.
/// 이스터에그라서 게임 일시정지·획득 UI는 기본으로 끕니다 (Contents도 비워 둠).
/// 안의 참새는 별도 액터(ASecretSparrow)로 배치하고, 그 액터가 이 상자를 참조합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/Stage/GimmickChest.h"
#include "EasterEggChest.generated.h"

class APetCharacter;
class ATutorialPortal;

UCLASS()
class MYNICELITTLECAT_API AEasterEggChest : public AGimmickChest
{
	GENERATED_BODY()

public:
	AEasterEggChest();

	virtual void Tick(float DeltaSeconds) override;

	// IInteractableInterface: 가짜 시도가 남아 있으면 열지 않고 괴냥이 대사만 출력
	virtual void Interact_Implementation(AActor* InteractInstigator) override;

	// IInteractableInterface: 가짜 시도가 남아 있으면 E 안내 UI를 숨김
	virtual FInteractPromptInfo GetInteractPrompt_Implementation(AActor* InteractInstigator) const override;

	// IInteractableInterface: 마지막 헛탕 직후 잠깐(RevealInteractDelay)은 상호작용 막음 → E 안내도 그동안 안 보임
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;

	// 가짜 시도를 다 채워서 이제 진짜로 열 수 있는지
	UFUNCTION(BlueprintPure, Category = "EasterEgg")
	bool IsRevealed() const { return FakeAttemptCount >= FakeAttemptLines.Num(); }

	// 잠금 해제됐는지 (관문이 열리고 UnlockDelay가 지남, 관문이 없으면 항상 true)
	UFUNCTION(BlueprintPure, Category = "EasterEgg")
	bool IsUnlocked() const { return bUnlocked; }

	// 월드의 괴냥이 (게임모드 우선, 없으면 레벨에서 검색)
	static APetCharacter* FindPet(const UObject* WorldContextObject);

	// 괴냥이 말풍선 출력 (펫이 없으면 로그만 남김). 참새도 이걸로 말함
	static void PetSay(const UObject* WorldContextObject, const FText& Message, float Duration);

protected:
	virtual void BeginPlay() override;

	// 가짜 시도를 할 때마다 (소리·카메라 흔들림 등 연출용). AttemptIndex는 0부터
	UFUNCTION(BlueprintImplementableEvent, Category = "EasterEgg", meta = (DisplayName = "On Fake Attempt"))
	void K2_OnFakeAttempt(AActor* Opener, int32 AttemptIndex);

protected:
	// ===== 잠금 =====

	// 이 관문이 열려야 상호작용이 켜짐 (튜토리얼 보스 처치 후 열리는 관문). 비우면 항상 켜짐
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "EasterEgg|Unlock")
	TObjectPtr<ATutorialPortal> UnlockPortal;

	// 관문이 열린 뒤 이만큼 더 기다림 (튜토리얼 마무리 연출과 겹치지 않게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Unlock", meta = (ClampMin = "0.0", Units = "s"))
	float UnlockDelay = 1.f;

	// ===== 헛탕 =====

	// 가짜 시도마다 괴냥이가 할 대사 (순서대로). 개수 = 열리기 전까지 헛탕 치는 횟수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg")
	TArray<FText> FakeAttemptLines;

	// 가짜 시도 대사 말풍선 표시 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg", meta = (ClampMin = "0.1", Units = "s"))
	float FakeLineDuration = 2.5f;

	// 마지막 헛탕 뒤 이 시간 동안은 상호작용 막음 (헛탕 대사 직후 바로 E 안내가 떠서 열려 버리는 것 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg", meta = (ClampMin = "0.0", Units = "s"))
	float RevealInteractDelay = 1.f;

private:
	// 헛탕 친 횟수 (저장 안 함: 상자가 열린 채로 저장되면 다시 쓸 일이 없음)
	int32 FakeAttemptCount = 0;

	// 이 월드 시간 전까지는 상호작용 막음 (마지막 헛탕 직후)
	double InteractBlockedUntil = 0.0;

	// 잠금
	bool bUnlocked = false;
	float UnlockElapsed = 0.f;
};
