// 작성자: 우진
// 작성일자: 2026-11-07
//
// 스테이지 1 시작 구간 자성 튜토리얼
// 플레이어가 StartTriggerTag 태그가 붙은 트리거(TriggerBoxMagnetMode)에 들어오면 시작, 자성을 이미 가졌으면 생략
// (1) 자성 구슬 획득 → (2) 우클릭 유지로 자기장 켜기 → (3) 우클릭 중 Q로 극 변경 → (4) 발판 위에서 우클릭 떼서 내려놓기
// 의도: 기존 ATutorialManager는 튜토리얼 맵 전용 연출이 많아서, 단계가 적은 자성 튜토리얼은
//       "현재 단계 + 0.2초마다 조건 확인" 구조로 따로 가볍게 만듦.
//       완료는 키 입력이 아니라 실제 결과(능력 실행 중/들고 있음/발판 켜짐)로만 판단해서 입력 코드를 건드리지 않음.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "MagnetTutorialManager.generated.h"

UENUM(BlueprintType)
enum class EMagnetTutorialStep : uint8
{
	None         UMETA(DisplayName = "시작 전"),
	Acquire      UMETA(DisplayName = "1. 자성 구슬 획득"),
	HoldField    UMETA(DisplayName = "2. 우클릭 유지"),
	SwapPolarity UMETA(DisplayName = "3. Q 극 변경"),
	PlaceOnPlate UMETA(DisplayName = "4. 발판에 내려놓기"),
	Completed    UMETA(DisplayName = "완료"),
};

UCLASS()
class MYNICELITTLECAT_API AMagnetTutorialManager : public AActor
{
	GENERATED_BODY()

public:
	AMagnetTutorialManager();

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	EMagnetTutorialStep GetCurrentStep() const { return CurrentStep; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// =============================================================================
protected:
	void TryStart();                           // 펫이 준비될 때까지 반복 확인
	void EnterStep(EMagnetTutorialStep _step); // 단계 시작 (안내 말풍선 + 극 고정 등)
	void CheckStep();                          // CheckInterval마다 완료/힌트 확인
	void CompleteStep(const FText& _praise);   // 칭찬 말풍선 → StepDelay 뒤 다음 단계

	void PetSay(const FText& _message, float _duration = 0.f); // 말풍선 (0이면 InstructionDuration)
	void SayHint(const FText& _hint);                          // HintInterval 동안 말이 없었을 때만

	class APetCharacter* FindPet() const;
	float GetPlayerDistanceTo(const AActor* _target) const;
	bool IsMagnetHeld() const;  // 우클릭으로 자성 능력 실행 중?
	bool IsCarryingBox() const; // 펫이 튜토리얼 박스를 들고 있나?

	UFUNCTION()
	void HandleAbilityAcquireFinished(FGameplayTag _abilityTag, class UItemData* _item);

	// 시작 트리거에 플레이어가 들어옴 → 튜토리얼 시작 허용
	UFUNCTION()
	void HandleStartTriggerOverlap(AActor* _overlappedActor, AActor* _otherActor);

	void UnbindStartTriggers(); // 한 번 시작하면 트리거 감시 해제
	// =============================================================================
protected:
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Tutorial")
	EMagnetTutorialStep CurrentStep = EMagnetTutorialStep::None;

	// 튜토리얼에서 옮길 자성 박스 (펫 시작 극 N과 같은 N극이어야 Q 단계가 자연스러움)
	UPROPERTY(EditInstanceOnly, Category = "Tutorial|Level")
	TObjectPtr<AActor> MagnetBox;

	// 박스를 올릴 발판 (발판이 문과 연결돼 있어야 함)
	UPROPERTY(EditInstanceOnly, Category = "Tutorial|Level")
	TObjectPtr<class AGimmickPressurePlate> Plate;

	// 끄면 예전처럼 레벨 시작하자마자 튜토리얼 시작 (테스트용)
	UPROPERTY(EditAnywhere, Category = "Tutorial|Start")
	bool bStartOnTrigger = true;

	// 시작 트리거 액터에 붙인 태그
	// 의도: 트리거는 메인 레벨, 매니저는 레벨 인스턴스 안이라 직접 참조가 안 되므로 런타임에 태그로 찾음
	UPROPERTY(EditAnywhere, Category = "Tutorial|Start", meta = (EditCondition = "bStartOnTrigger"))
	FName StartTriggerTag = TEXT("MagnetTutorialStart");

	UPROPERTY(EditAnywhere, Category = "Tutorial|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float CheckInterval = 0.2f;      // 조건 확인 주기

	UPROPERTY(EditAnywhere, Category = "Tutorial|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float StepDelay = 2.f;           // 칭찬 말풍선 후 다음 안내까지

	UPROPERTY(EditAnywhere, Category = "Tutorial|Timing", meta = (ClampMin = "0.5", Units = "s"))
	float InstructionDuration = 5.f; // 안내 말풍선 표시 시간

	UPROPERTY(EditAnywhere, Category = "Tutorial|Timing", meta = (ClampMin = "1.0", Units = "s"))
	float HintInterval = 8.f;        // 진전이 없으면 몇 초마다 다시 알려줄지

	// 이 거리 안에서 우클릭을 유지해야 2단계 완료 (자기장 사거리 안에서 "안 붙는다"를 보여주려고)
	UPROPERTY(EditAnywhere, Category = "Tutorial|Magnet", meta = (ClampMin = "0.0", Units = "cm"))
	float NearBoxDistance = 450.f;

	// 2단계에서 우클릭을 이만큼 유지해야 완료 (바로 끝나면 "안 붙는" 걸 볼 시간이 없어서)
	UPROPERTY(EditAnywhere, Category = "Tutorial|Magnet", meta = (ClampMin = "0.0", Units = "s"))
	float HoldFieldRequiredTime = 1.f;

	// 2단계에서 상자와 이 거리보다 멀어지면 극 고정 해제 (돌아오면 다시 고정)
	// 의도: 튜토리얼 도중 뒤로 빠졌을 때 다른 자성 기믹을 극 고정 때문에 못 푸는 일이 없게
	UPROPERTY(EditAnywhere, Category = "Tutorial|Magnet", meta = (ClampMin = "0.0", Units = "cm"))
	float PolarityLockDistance = 1500.f;

private:
	TWeakObjectPtr<class APetCharacter> Pet;

	FTimerHandle StartTimerHandle;
	FTimerHandle CheckTimerHandle;
	FTimerHandle NextStepTimerHandle;

	TArray<TWeakObjectPtr<AActor>> StartTriggers; // 바인딩한 시작 트리거들 (해제용)
	bool bPlayerEntered = false;                  // 플레이어가 시작 트리거에 들어왔나

	bool bAcquireFinished = false; // 구슬 획득 연출이 끝났나
	bool bStepDone = false;        // 완료 처리 중복 방지 (StepDelay 동안)
	bool bWasCarrying = false;     // 4단계: 직전 체크에 들고 있었나 (Q로 튕겼는지 판단)
	float HoldFieldTime = 0.f;     // 2단계: 박스 근처에서 우클릭 유지한 시간
	float LastSayTime = -1000.f;   // 마지막 말풍선 시각
};
