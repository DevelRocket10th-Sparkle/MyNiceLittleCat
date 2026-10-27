// 작성자: 우진
// 작성일자: 2026-10-26
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialManager.generated.h"

// 스테이지 구간
UENUM(BlueprintType)
enum class ETutorialStage : uint8
{
    None       UMETA(DisplayName = "시작 전"),
    Movement   UMETA(DisplayName = "이동"),
    Dash       UMETA(DisplayName = "대시"),
    LockOn     UMETA(DisplayName = "락온 및 공격"),
    Combat     UMETA(DisplayName = "회복 및 전투"),
    Finish     UMETA(DisplayName = "마무리"),
    Completed  UMETA(DisplayName = "완료")
};

// 한 동작 뒤에 무엇을 기다릴지
enum class ETutorialWait : uint8
{
    None,           // 즉시 다음 동작
    Seconds,        // N초 기다리기 (N초 뒤 다음 동작)
    PlayerEnter,    // 플레이어가 지정한 트리거에 들어오면 다음 동작
};

// 구간 안에서 순서대로 실행되는 동작 하나
struct FTutorialAction
{
    TFunction<void()> Run;                    // 실행할 일 (비어 있어도 됨)
    ETutorialWait Wait = ETutorialWait::None; // 다음 동작으로 넘어가기 전에 무엇을 기다릴지

    float Seconds = 0.f;                      // Wait == Seconds 일 때 기다릴 시간
    TWeakObjectPtr<AActor> Trigger;           // Wait == PlayerEnter 일 때 기다릴 트리거
};

UCLASS()
class MYNICELITTLECAT_API ATutorialManager : public AActor
{
	GENERATED_BODY()
	
public:	
	ATutorialManager();

protected:
	virtual void BeginPlay() override;
    // =========================================================
public:
    // 구간 변경하는 함수
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void ChangeStage(ETutorialStage NewStage);

protected:
    // 구간 시작/끝 함수
    void StageStart(ETutorialStage Stage);
    void StageEnd(ETutorialStage Stage);

    // 구간별(ETutorialStage) 동작 목록 만들기
    void BuildActions(ETutorialStage Stage, TArray<FTutorialAction>& OutActions);
    void BuildMovementActions(TArray<FTutorialAction>& OutActions);

    // 동작 실행기
    void RunNextAction();
    void OnActionsFinished(); // 현재 구간의 모든 동작이 끝나면 다음 구간으로

    // 동작(Action) 만들기 편의 함수
    static FTutorialAction Do(TFunction<void()> _Run);
    static FTutorialAction WaitSeconds(float _Seconds);
    static FTutorialAction WaitPlayerEnter(AActor* _Trigger);

    // 태그가 붙은 레벨 액터 찾기.
    AActor* FindActorByTag(FName _Tag) const;

    // ETutorialWait == PlayerEnter
    bool BeginWaitPlayerEnter(AActor* _Trigger);  // PlayerEnter 대기 시작.  계속 기다려야 하면 true, 바로 넘어가도 되면 false
    void StopWaitPlayerEnter(); // PlayerEnter 대기 정리 (바인딩 해제)

    // 기다리는 트리거에 무언가 들어왔을 때
    UFUNCTION()
    void OnWaitTriggerBeginOverlap(AActor* _OverlappedActor, AActor* _OtherActor);

    // 플레이어 판별 함수 (펫이나 적이 트리거를 밟아도 진행되지 않게)
    bool IsPlayerPawn(const AActor* _Actor) const;

    // =========================================================
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tutorial")
    ETutorialStage CurrentStage = ETutorialStage::None; // 현재 스테이지 구간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Debug")
    ETutorialStage StartStage = ETutorialStage::Movement; //(디버그용) 시작할 구간

private:
    bool bChangingStage = false; // 구간 전환 도중에 또 전환되는 것을 막는 불값
    
    // FTutorialAction 관련
    TArray<FTutorialAction> Actions; // 현재 구간의 동작 목록
    int32 ActionIndex = INDEX_NONE;  // 지금 실행 중인 동작 번호

    // ETutorialWait 별로 필요한 것들
    FTimerHandle WaitTimerHandle; // N초 대기 중일 때의 타이머
    TWeakObjectPtr<AActor> WaitingTrigger; // 플레이어 진입을 기다리는 중일 떄, 그 트리거
};
