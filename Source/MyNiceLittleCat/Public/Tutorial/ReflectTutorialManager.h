// 작성자: 우진
// 작성일자: 2026-11-08
//
// 스테이지 2 신전 구간(S2_Alley_Door ~ S2_Shrine_Door) 반사 튜토리얼
// 플레이어가 S2_Alley_Door를 지나 시작 트리거(StartTrigger)에 들어오면 시작
// (1) 반사 구슬 획득 → (2) 설치 프리뷰 소개 → (3) 우클릭 드래그로 방향 정해 설치
// → (4) 반사판에 마우스 올리고 Q로 철거 → (5) 레이저를 꺾어 수신기를 켜서 문 열기
// 의도: 자성 튜토리얼(AMagnetTutorialManager)과 같은 "현재 단계 + 0.2초마다 조건 확인" 구조.
//       완료는 키 입력이 아니라 실제 결과(반사판 생김/철거됨/수신기 켜짐)로만 판단해서
//       반사 능력·입력 코드를 건드리지 않음.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ReflectTutorialManager.generated.h"

UENUM(BlueprintType)
enum class EReflectTutorialStep : uint8
{
	None      UMETA(DisplayName = "시작 전"),
	Acquire   UMETA(DisplayName = "1. 반사 구슬 획득"),
	Preview   UMETA(DisplayName = "2. 설치 프리뷰 소개"),
	Place     UMETA(DisplayName = "3. 우클릭 드래그로 설치"),
	Remove    UMETA(DisplayName = "4. Q로 철거"),
	Solve     UMETA(DisplayName = "5. 레이저 꺾어서 문 열기"),
	Completed UMETA(DisplayName = "완료"),
};

UCLASS()
class MYNICELITTLECAT_API AReflectTutorialManager : public AActor
{
	GENERATED_BODY()

public:
	AReflectTutorialManager();

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	EReflectTutorialStep GetCurrentStep() const { return CurrentStep; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// =============================================================================
protected:
	void TryStart();                            // 펫이 준비될 때까지 반복 확인
	void EnterStep(EReflectTutorialStep _step); // 단계 시작 (안내 말풍선)
	void CheckStep();                           // CheckInterval마다 완료/힌트 확인
	void CompleteStep(const FText& _praise);    // 칭찬 말풍선 → StepDelay 뒤 다음 단계

	void PetSay(const FText& _message, float _duration = 0.f); // 말풍선 (0이면 InstructionDuration)
	void SayHint(const FText& _hint);                          // HintInterval 동안 말이 없었을 때만

	class APetCharacter* FindPet() const;
	bool IsReflectMode() const;    // 펫이 지금 반사 모드인가
	bool IsReceiverOn() const;     // 수신기가 켜졌나 (= 문 열림)

	// 펫이 새로 설치한 반사판을 찾아 철거 감시 등록 (설치 단계 완료 판정에도 사용)
	int32 TrackPetReflectors();

	// 감시 중인 반사판이 사라짐 → 수명이 남아 있었으면 Q로 철거한 것
	// 의도: OnDestroyed가 아니라 OnEndPlay에 연결. 엔진이 OnDestroyed 알림 전에 수명 타이머를 먼저 지워서
	//       OnDestroyed에서는 Q로 철거해도 남은 수명이 항상 0으로 읽힘 (EndPlay 시점엔 아직 남아 있음)
	UFUNCTION()
	void HandleReflectorEndPlay(AActor* _actor, EEndPlayReason::Type _reason);

	UFUNCTION()
	void HandleAbilityAcquireFinished(FGameplayTag _abilityTag, class UItemData* _item);

	// 플레이어가 시작 트리거(S2_Alley_Door 바로 안쪽)에 들어옴 → 튜토리얼 시작 허용
	UFUNCTION()
	void HandleStartTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	// =============================================================================
protected:
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Tutorial")
	EReflectTutorialStep CurrentStep = EReflectTutorialStep::None;

	// 플레이어가 이 박스에 들어와야 튜토리얼이 시작됨 (매니저를 S2_Alley_Door 바로 안쪽에 놓고 크기를 통로에 맞춤)
	// 의도: 문이 열리고 플레이어가 지나갈 때 시작. 문이 닫혀 있으면 들어올 수 없으니 "문 열림" 확인을 따로 안 해도 됨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tutorial")
	TObjectPtr<class UBoxComponent> StartTrigger;

	// 끄면 예전처럼 레벨 시작하자마자 튜토리얼 시작 (테스트용)
	UPROPERTY(EditAnywhere, Category = "Tutorial")
	bool bStartOnTrigger = true;

	// 마지막 단계에서 켜야 하는 레이저 수신기 (문과 연결돼 있어야 함, 예: S2_Shrine_Receiver)
	UPROPERTY(EditInstanceOnly, Category = "Tutorial|Level")
	TObjectPtr<class AGimmickActorBase> Receiver;

	UPROPERTY(EditAnywhere, Category = "Tutorial|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float CheckInterval = 0.2f;      // 조건 확인 주기

	UPROPERTY(EditAnywhere, Category = "Tutorial|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float StepDelay = 2.f;           // 칭찬 말풍선 후 다음 안내까지

	UPROPERTY(EditAnywhere, Category = "Tutorial|Timing", meta = (ClampMin = "0.5", Units = "s"))
	float InstructionDuration = 6.f; // 안내 말풍선 표시 시간

	UPROPERTY(EditAnywhere, Category = "Tutorial|Timing", meta = (ClampMin = "1.0", Units = "s"))
	float HintInterval = 9.f;        // 진전이 없으면 몇 초마다 다시 알려줄지

	// 2단계: 반사 모드로 프리뷰를 이만큼 본 뒤 다음 단계로 (읽을 시간 + 마우스를 움직여 볼 시간)
	UPROPERTY(EditAnywhere, Category = "Tutorial|Reflect", meta = (ClampMin = "0.0", Units = "s"))
	float PreviewLookTime = 5.f;

	// 반사판이 사라질 때 수명이 이보다 많이 남아 있으면 "Q로 철거"로 판단 (수명 만료와 구분)
	UPROPERTY(EditAnywhere, Category = "Tutorial|Reflect", meta = (ClampMin = "0.0", Units = "s"))
	float RemoveLifeSpanThreshold = 0.1f;

private:
	TWeakObjectPtr<class APetCharacter> Pet;

	// 철거 감시 중인 펫 반사판 (OnDestroyed 바인딩 중복 방지)
	TSet<TWeakObjectPtr<AActor>> TrackedReflectors;

	FTimerHandle StartTimerHandle;
	FTimerHandle CheckTimerHandle;
	FTimerHandle NextStepTimerHandle;

	bool bPlayerEntered = false;   // 플레이어가 시작 트리거에 들어왔나
	bool bAcquireFinished = false; // 구슬 획득 연출이 끝났나
	bool bStepDone = false;        // 완료 처리 중복 방지 (StepDelay 동안)
	bool bRemovedByPlayer = false; // 4단계: Q로 철거한 반사판이 있나
	float PreviewTime = 0.f;       // 2단계: 반사 모드로 프리뷰를 본 시간
	float LastSayTime = -1000.f;   // 마지막 말풍선 시각
};
