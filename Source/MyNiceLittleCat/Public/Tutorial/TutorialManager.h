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
    PetArrive,      // 펫이 특정 지점에 도착하면 다음 동작
};

// 구간 안에서 순서대로 실행되는 동작 하나
struct FTutorialAction
{
    TFunction<void()> Run;                    // 실행할 일 (비어 있어도 됨)
    ETutorialWait Wait = ETutorialWait::None; // 다음 동작으로 넘어가기 전에 무엇을 기다릴지

    float Seconds = 0.f;                      // Wait == Seconds 일 때 기다릴 시간
    TWeakObjectPtr<AActor> Target;           // Wait == PlayerEnter 일 때 기다릴 트리거, == PetArrive 일 때 펫 도착 지점
};

UCLASS()
class MYNICELITTLECAT_API ATutorialManager : public AActor
{
	GENERATED_BODY()
	
public:	
	ATutorialManager();

protected:
	virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override; // 펫 비행 중에만 켜짐
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // =========================================================
public:
    // 구간 변경하는 함수
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void ChangeStage(ETutorialStage NewStage);

protected:
    // 펫이 준비됐는지 확인하고, 준비됐으면 튜토리얼 시작 (BeginPlay 에서 타이머로 반복 호출)
    void TryStartTutorial();

    // 구간 시작/끝 함수
    void StageStart(ETutorialStage Stage);
    void StageEnd(ETutorialStage Stage);

    // 구간별(ETutorialStage) 동작 목록 만들기
    void BuildActions(ETutorialStage Stage, TArray<FTutorialAction>& OutActions);
    void BuildMovementActions(TArray<FTutorialAction>& OutActions);
    void BuildDashActions(TArray<FTutorialAction>& OutActions);

    // 동작 실행기
    void RunNextAction();
    void OnActionsFinished(); // 현재 구간의 모든 동작이 끝나면 다음 구간으로

    // 동작(Action) 만들기 편의 함수
    static FTutorialAction Do(TFunction<void()> _Run);
    static FTutorialAction WaitSeconds(float _Seconds);
    static FTutorialAction WaitPlayerEnter(AActor* _Trigger);
    static FTutorialAction WaitPetArrive(AActor* _Point, float _Timeout = 8.f);

    // 태그가 붙은 레벨 액터 찾기.
    AActor* FindActorByTag(FName _Tag) const;

    // ETutorialWait == PlayerEnter
    bool BeginWaitPlayerEnter(AActor* _Trigger);  // PlayerEnter 대기 시작.  계속 기다려야 하면 true, 바로 넘어가도 되면 false
    void StopWaitPlayerEnter(); // PlayerEnter 대기 정리 (바인딩 해제)

    // 액터에 Activate / Deactivate 신호 보내기 (IActivatableInterface 를 구현한 액터만)
    void SetActivated(AActor* _Target, bool _bActivate);

    // 기다리는 트리거에 무언가 들어왔을 때
    UFUNCTION()
    void OnWaitTriggerBeginOverlap(AActor* _OverlappedActor, AActor* _OtherActor);

    // 플레이어 판별 함수 (펫이나 적이 트리거를 밟아도 진행되지 않게)
    bool IsPlayerPawn(const AActor* _Actor) const;

    // ===== 펫 다루기 (동작 목록 안에서 사용) =====
    void PlacePet(AActor* _Point);                      // 지점으로 순간이동
    void PetMoveTo(AActor* _Point, float _Speed = 0.f); // 지점까지 걸어가기. _Speed 0 = 원래 속도
    void PetSay(const FText& _Message);                 // 말풍선
    void PetFlyTo(AActor* _Point, float _Duration); // 펫 날리기: 지금 위치에서 지점까지 포물선으로 _Duration 초 동안 날아감
    void StopPetFly(); // 비행 정리 (이동 모드/애니메이션 원래대로, Tick 끄기)
    void PetFacePlayer(); // 펫을 플레이어 쪽으로 한 번 돌려놓기 (실시간 추적 X)

    // ETutorialWait == PetArrive
    bool BeginWaitPetArrive(AActor* _Point, float _TimeOut); // 계속 기다려야 하면 true, 바로 넘어가도 되면 false
    void CheckPetArrive(); // 0.1초마다 펫이 도착했는지 확인
    void StopWaitPetArrive(); // 펫 도착 대기 정리

    // 카메라 행동
    bool ApplyIntroCinematic(); // 시작 연출 적용. 플레이어가 아직 없으면 false
    void BeginCinematic(AActor* _Camera, float _BlendTime); // 입력 잠금 + 카메라 전환
    void EndCinematic(float _BlendTime); // 카메라 복귀 + 입력 해제
    void SetPlayerInputLocked(class APlayerController* _PC, bool _bLock); // 캐릭터 + 컨트롤러 입력 껐켜

    UFUNCTION()
    void HandlePlayerRespawned(AController* _Controller, APawn* _NewPawn); // 리스폰되면 연출 강제 종료
    // =========================================================
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tutorial")
    ETutorialStage CurrentStage = ETutorialStage::None; // 현재 스테이지 구간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Debug")
    ETutorialStage StartStage = ETutorialStage::Movement; //(디버그용) 시작할 구간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Pet", meta = (ClampMin = "0.0"))
    float SlowPetSpeed = 200.f; // "천천히 따라와" 안내할 때 펫 속도

    UPROPERTY(EditAnywhere, Category = "Tutorial|Pet", meta = (ClampMin = "10.0"))
    float PetArriveRadius = 100.f; // 이 거리 안에 들어오면 "도착"으로 봄

    UPROPERTY(EditAnywhere, Category = "Tutorial|Camera", meta = (ClampMin = "0.0"))
    float CameraReturnBlendTime = 1.f; // 연출 끝나고 플레이어 카메라로 돌아오는 시간(초)

    // ===== 대시 구간 연출 시간 (PIE 보면서 조절) =====
    UPROPERTY(EditAnywhere, Category = "Tutorial|Dash", meta = (ClampMin = "0.0"))
    float DashCameraBlendTime = 1.f; // 플레이어 카메라 -> 통로 전체 샷으로 넘어가는 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Dash", meta = (ClampMin = "0.1"))
    float DashFlyTime = 2.f;         // 펫이 레이저 위를 날아 통로 끝까지 가는 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Dash", meta = (ClampMin = "0.0"))
    float DashShowTime = 1.5f;        // 말풍선 + Shift 데칼 보여주는 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Pet", meta = (ClampMin = "0.0"))
    float PetFlyArcHeight = 150.f;    // 비행 포물선의 가장 높은 지점 (시작/끝 높이 기준 +)

    UPROPERTY(EditAnywhere, Category = "Tutorial|Pet", meta = (ClampMin = "1.0"))
    float PetTurnSpeed = 540.f; // 플레이어 쪽으로 돌 때 초당 회전 각도 (180도 = 약 0.33초)

private:
    bool bChangingStage = false; // 구간 전환 도중에 또 전환되는 것을 막는 불값
    
    // FTutorialAction 관련
    TArray<FTutorialAction> Actions; // 현재 구간의 동작 목록
    int32 ActionIndex = INDEX_NONE;  // 지금 실행 중인 동작 번호

    // ETutorialWait 별로 필요한 것들
    FTimerHandle WaitTimerHandle; // N초 대기 중일 때의 타이머
    TWeakObjectPtr<AActor> WaitingTrigger; // 플레이어 진입을 기다리는 중일 떄, 그 트리거

    TWeakObjectPtr<class APetCharacter> Pet; // 튜토리얼이 조종하는 펫 (GameMode 가 스폰한)
    float PetDefaultSpeed = 0.f;       // 펫 원래 걷기 속도. 느리게 바꾼 뒤 되돌릴 때 사용
    FTimerHandle StartTimerHandle;     // 펫 준비 확인용 반복 타이머

    //PetArrive..
    FTimerHandle PetArriveTimerHandle; // 도착 확인 반복 타이머
    TWeakObjectPtr<AActor> WaitingPetPoint; // 펫이 가야 하는 지점
    float PetArriveTimeLeft = 0.f; // 남은 최대 대기 시간

    bool bInCinematic = false; // 연출중? (카메라 전환 + 입력 잠금)
    bool bIntroApplied = false; // 시작 연출을 이미 걸었는지
    TWeakObjectPtr<APawn> LockedPawn; // 입력을 끈 캐릭터

    // 펫 비행 관련 변수들
    bool bPetFlying = false;
    FVector FlyStart = FVector::ZeroVector;
    FVector FlyEnd = FVector::ZeroVector;
    float FlyDuration = 0.f;
    float FlyElapsed = 0.f;

    // 펫을 플레이어 방향으로 돌려놓기
    bool bPetTurning = false;
    float PetTurnTargetYaw = 0.f;
};
