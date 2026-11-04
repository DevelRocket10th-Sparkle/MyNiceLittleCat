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
    Condition,      // 조건이 true가 될 때 까지 0.1초마다 확인
};

// 구간 안에서 순서대로 실행되는 동작 하나
struct FTutorialAction
{
    TFunction<void()> Run;                    // 실행할 일 (비어 있어도 됨)
    ETutorialWait Wait = ETutorialWait::None; // 다음 동작으로 넘어가기 전에 무엇을 기다릴지

    float Seconds = 0.f;                      // Wait == Seconds 일 때 기다릴 시간
    TWeakObjectPtr<AActor> Target;            // Wait == PlayerEnter 일 때 기다릴 트리거, == PetArrive 일 때 펫 도착 지점
    TFunction<bool()> Condition;              // Wait == Condition 일 때 확인할 조건
};

// 연출 순간 하나에 같이 나오는 효과 묶음 (비워 둔 칸은 그냥 건너뜀 -> 에셋 없이도 연출은 그대로 진행)
USTRUCT(BlueprintType)
struct FTutorialFXSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    TObjectPtr<class UNiagaraSystem> Effect;           // 파티클 효과

    UPROPERTY(EditAnywhere)
    TObjectPtr<class USoundBase> Sound;                // 효과음

    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
    float VolumeMultiplier = 1.f;                // 효과음 볼륨 배율

    UPROPERTY(EditAnywhere)
    TSubclassOf<class UCameraShakeBase> CameraShake;   // 카메라 흔들림

    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
    float ShakeScale = 1.f;                      // 흔들림 세기 배율
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
    void BuildLockOnActions(TArray<FTutorialAction>& OutActions);
    void BuildCombatActions(TArray<FTutorialAction>& OutActions);

    // 동작 실행기
    void RunNextAction();
    void OnActionsFinished(); // 현재 구간의 모든 동작이 끝나면 다음 구간으로

    // 동작(Action) 만들기 편의 함수
    static FTutorialAction Do(TFunction<void()> _Run);
    static FTutorialAction WaitSeconds(float _Seconds);
    static FTutorialAction WaitPlayerEnter(AActor* _Trigger);
    static FTutorialAction WaitPetArrive(AActor* _Point, float _Timeout = 8.f);
    static FTutorialAction WaitUntil(TFunction<bool()> _Condition);


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
    void PetFlyToLocation(const FVector& _Location, float _Duration); // 좌표로 날리기 (_Location = 바닥 위치)
    void PetWalkToLocation(const FVector& _Location); // 좌표까지 걸어가기 (StateTree 거치지 않고 매니저가 직접. 도중에 다시 불러도 OK)
    bool IsPetWalking() const;                        // 펫이 걷는 중인지 (도착하면 false)
    void StopPetFly(); // 비행 정리 (이동 모드/애니메이션 원래대로, Tick 끄기)
    void PetFacePlayer(); // 펫을 플레이어 쪽으로 한 번 돌려놓기 (실시간 추적 X)

    // ETutorialWait == PetArrive
    bool BeginWaitPetArrive(AActor* _Point, float _TimeOut); // 계속 기다려야 하면 true, 바로 넘어가도 되면 false
    void CheckPetArrive(); // 0.1초마다 펫이 도착했는지 확인
    void StopWaitPetArrive(); // 펫 도착 대기 정리

    // ETutorialWait == Condition
    bool BeginWaitCondition(const TFunction<bool()>& _Condition); // 계속 기다려야 하면 true, 이미 참이면 false
    void CheckCondition();     // 0.1초마다 조건 확인
    void StopWaitCondition();  // 조건 대기 정리

    // 카메라 행동
    bool ApplyIntroCinematic(); // 시작 연출 적용. 플레이어가 아직 없으면 false
    void BeginCinematic(AActor* _Camera, float _BlendTime); // 입력 잠금 + 카메라 전환
    void EndCinematic(float _BlendTime); // 카메라 복귀 + 입력 해제
    void SetPlayerInputLocked(class APlayerController* _PC, bool _bLock); // 캐릭터 + 컨트롤러 입력 껐켜

    // ===== 락온 구간 =====
    class ULockOnComponent* GetPlayerLockOn() const;        // 플레이어의 락온 컴포넌트 (리스폰 대비 매번 찾음)
    bool IsPlayerLockedOn() const;                    // 플레이어가 락온 중인지
    static bool IsEnemyAlive(AActor* _Enemy);         // 락온 가능한 대상이 살아있는지
    bool AreLockOnEnemiesDead() const;                // 락온 구간 적이 전부 죽었는지
    AActor* FindLockOnHintTarget() const;             // 펫이 처음 가리킬 적 (휠 누르면 잡힐 적 우선)
    bool HasPlayerSwitchedLockTarget();               // 휠 스크롤로 락온 대상을 바꿨는지 (바꿀 대상이 없으면 통과)

    FVector GetGroundLocation(const AActor* _Actor) const;     // 캐릭터면 발바닥 위치, 아니면 액터 위치
    FVector GetPetPointNearPlayer() const;                     // 플레이어 앞 바닥 위치
    FVector GetPetPointNearEnemy(const AActor* _Enemy) const;  // 적 옆 바닥 위치 (플레이어 시야를 가리지 않게)

    void PetWalkToEnemy(AActor* _Enemy);   // 적 옆으로 짧게 날아가기
    void StartPetFollowLockTarget();      // 락온 대상이 바뀌면 펫이 따라가기 시작
    void UpdatePetFollowLockTarget();     // 0.1초마다 락온 대상 확인
    void StopPetFollowLockTarget();       // 따라가기 끝
    
    UFUNCTION()
    void HandlePlayerRespawned(AController* _Controller, APawn* _NewPawn); // 리스폰되면 연출 강제 종료

    void RestartBossFight(); // 보스전 중 사망 -> 보스 정리 후 보스 등장 연출부터 다시

    void ApplyTutorialCamera(APawn* _PlayerPawn); // 튜토리얼 전용 카메라 설정 (벽/천장 밖으로 카메라가 나가지 않게)

    // ===== 전투 구간 =====
    void RefillPlayerHealth();                           // 플레이어 목숨을 최대치로 채움 (보스전 시작 공정하게)
    void SetActorShown(AActor* _Actor, bool _bShow);     // 액터 보이기/숨기기 + 충돌 켜기/끄기 (체크포인트 등장용)
    void SetPetHidden(bool _bHidden);                    // 펫 숨기기 (보스로 변한 동안)
    void SetPetMeshScale(float _Scale);                  // 펫 "메시만" 크기 바로 바꾸기 (원래 크기 기준 배율)
    void StartPetMeshScale(float _TargetScale, float _Duration); // 펫 메시를 _Duration 초 동안 _TargetScale 배로 (Tick 에서 진행)
    bool IsPetMeshScaling() const { return bPetScaling; }
    void RestorePetForm();                               // 펫 크기/보이기 원래대로 (구간이 끊겨도 펫이 거대하거나 숨은 채로 남지 않게)
    void SwapPetToBoss();                                // 거대해진 펫 자리에 보스 스폰 + 펫 숨김
    void StartBossAI();                                  // 보스 AI 시작 (연출이 끝나고 조작이 돌아올 때)
    void SwapBossToPet(AActor* _AppearPoint, AActor* _FaceTarget); // 보스 제거 -> 지정 지점에 거대한 펫 등장 (_FaceTarget 쪽을 바라봄)
    bool IsBossDead() const;                             // 보스가 죽었는지

    float GetPlayerHealthRatio() const;                  // 플레이어 체력 비율 (0~1, 못 읽으면 1)
    int32 GetPlayerPotionCount() const;                  // 플레이어 남은 포션 수 (못 읽으면 0)
    void RefillPlayerPotions();                          // 포션 가득 (회복 연습 전에)
    void BossSay(const FText& _Message);                 // 보스(변신한 펫) 머리 위 말풍선
    void CheckBossHealHint();                            // 보스전 중 체력이 낮아지면 회복 안내 (한 번만)

private:
    void PlayFXSetAtGround(const FTutorialFXSet& _Set, AActor* _Actor); // 액터 발밑에서 묶음 1회 재생
    void StartLoopFXOnPet(const FTutorialFXSet& _Set);                  // 펫에 붙여 반복 재생 (흔들림은 시작할 때 1회)
    void StopLoopFX();                                                  // 반복 중인 효과/소리 정리
    void PlayCameraShake(TSubclassOf<UCameraShakeBase> _Shake, float _Scale); // 지금 보는 카메라 흔들기
    void PlayFXSetAtLocation(const FTutorialFXSet& _Set, const FVector& _Location); // 지정 위치에서 묶음 1회 재생

    UFUNCTION()
    void HandleLockOnEnemyEndPlay(AActor* _Actor, EEndPlayReason::Type _Reason); // 락온 구간 적이 사라질 때
    void HandleLockOnEnemyDeathStarted(class AEnemyCharacter* _Enemy); // 락온 구간 적의 HP 가 0 이 된 순간
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

    UPROPERTY(EditAnywhere, Category = "Tutorial|Camera", meta = (ClampMin = "0.0"))
    float TutorialLockOnMaxExtraArm = 0.f; // 튜토리얼에서 락온 시 카메라가 추가로 멀어지는 최대 거리 (원래 700). 실내라 크게 빠지면 방 밖이 보임

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

    // ===== 락온 구간 연출 =====
    UPROPERTY(EditAnywhere, Category = "Tutorial|LockOn", meta = (ClampMin = "0.0"))
    float LockOnCameraBlendTime = 1.f;  // 플레이어 카메라 -> 아레나 전체 샷 전환 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|LockOn", meta = (ClampMin = "0.0"))
    float LockOnShowTime = 1.5f;        // 아레나 샷에서 적을 보여주는 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|LockOn", meta = (ClampMin = "0.1"))
    float LockOnPetFlyTime = 1.2f;      // 펫이 플레이어 앞으로 날아오는 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|LockOn", meta = (ClampMin = "0.0"))
    float PetFrontDistance = 200.f;     // 플레이어 앞 몇 cm 에 착지할지

    UPROPERTY(EditAnywhere, Category = "Tutorial|LockOn", meta = (ClampMin = "0.0"))
    float PetEnemyFrontOffset = 100.f;  // 적에서 플레이어 쪽으로 얼마나 나와 있을지

    UPROPERTY(EditAnywhere, Category = "Tutorial|LockOn", meta = (ClampMin = "0.0"))
    float PetEnemySideOffset = 150.f;   // 적 옆으로 얼마나 비켜설지 (적 캡슐과 겹치면 늘리기)

    // ===== 전투 구간 =====
    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat")
    TSubclassOf<class AEnemyCharacter> BossClass;  // 등장시킬 보스 (BP_Tutorial_Boss)

    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat", meta = (ClampMin = "1.0"))
    float BossMeshScale = 10.f;        // 보스 BP 메시 크기와 같게! (펫 메시를 여기까지 키운 뒤 보스로 바꿔서 이어 보이게)

    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat", meta = (ClampMin = "0.0"))
    float BossCameraBlendTime = 0.8f;   // 플레이어 카메라 -> 보스 아레나 샷 전환 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat", meta = (ClampMin = "0.1"))
    float PetGrowTime = 1.5f;          // 펫이 보스 크기까지 커지는 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat", meta = (ClampMin = "0.1"))
    float PetShrinkTime = 1.f;         // 보스를 잡은 뒤 펫이 원래 크기로 줄어드는 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat", meta = (ClampMin = "0.0"))
    float BossShowTime = 1.5f;         // 보스 등장 후 조작을 돌려주기 전까지 보여주는 시간

    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BossHealHintRatio = 0.5f;    // 보스전 중 체력이 이 비율 이하가 되면 회복 안내

    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat", meta = (ClampMin = "1.0"))
    float HealLessonRepeatTime = 6.f;  // 입구 회복 연습에서 안 마시면 몇 초마다 다시 알려줄지

    // ===== 전투 구간 효과 =====
    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat|FX")
    FTutorialFXSet PetGrowFX;      // 펫이 커지는 동안 (펫에 붙어서 반복 재생 -> 다 커지면 멈춤. 반복 에셋 사용)

    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat|FX")
    FTutorialFXSet BossAppearFX;   // 다 커져서 보스로 바뀌는 순간 (1회)

    UPROPERTY(EditAnywhere, Category = "Tutorial|Combat|FX")
    FTutorialFXSet PetShrinkFX;    // 보스를 잡고 펫이 줄어들기 시작할 때 (1회)

    // ===== 펫 =====
    UPROPERTY(EditAnywhere, Category = "Tutorial|Pet")
    TObjectPtr<class USoundBase> PetSaySound; // 말풍선이 뜰 때 소리 (짧은 UI음)

    UPROPERTY(EditAnywhere, Category = "Tutorial|Pet")
    FTutorialFXSet PetAppearFX;     // 펫이 처음 나타날 때 (순간이동 자리에서 반짝)

    UPROPERTY(EditAnywhere, Category = "Tutorial|LockOn")
    FTutorialFXSet EnemyVanishFX;   // 쓰러진 적이 사라질 때 (연기 팡)

    UPROPERTY(EditAnywhere, Category = "Tutorial|LockOn")
    FTutorialFXSet EnemyDyingFX;    // 쓰러져 있는 동안 몸에서 피어오르는 연기 (반복 에셋. 적이 사라질 때 같이 사라짐)

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

    // Condition 대기
    FTimerHandle ConditionTimerHandle;
    TFunction<bool()> WaitingCondition; // 지금 기다리는 조건

    // 락온 구간
    TArray<TWeakObjectPtr<AActor>> LockOnEnemies; // 락온 구간 적 전부 (구간 시작 시 태그로 모음)
    FTimerHandle PetFollowTimerHandle;            // 락온 대상 따라가기 확인 타이머
    TWeakObjectPtr<AActor> PetFollowedTarget;     // 펫이 지금 옆에 가 있는 적
    TWeakObjectPtr<AActor> SwitchBaseTarget;      // 대상 바꾸기 판정 기준 (이 적과 다른 적을 잡으면 "바꿈")
    bool bLockReleasedDuringSwitch = false;       // 바꾸기 안내 중에 락온을 풀었는지 (다시 잡으면 안내 한 번 더)

    // 전투 구간
    TWeakObjectPtr<class AEnemyCharacter> Boss; // 스폰한 보스
    bool bPetHidden = false;           // 펫을 숨겨둔 상태인지
    bool bPetScaling = false;          // 펫 메시 크기 변하는 중인지 (Tick)
    bool bPetMeshBaseSaved = false;    // 펫 메시 원래 크기를 저장했는지
    FVector PetMeshBaseScale = FVector::OneVector; // 펫 메시 원래 크기
    float PetScaleFrom = 1.f;
    float PetScaleTo = 1.f;
    float PetScaleDuration = 0.f;
    float PetScaleElapsed = 0.f;

    // ===== 회복 안내 =====
    bool bHealLesson = false;          // 입구에서 직접 마셔 보는 연습 중인지 (다쳤을 때만)
    int32 HealLessonPotions = 0;       // 연습 시작할 때 포션 수 (줄어들면 = 마셨음)
    float HealLessonSayTime = 0.f;     // 마지막으로 안내한 시각 (다시 알려주기용)
    bool bBossHealHintShown = false;   // 보스전 회복 안내를 이미 했는지

    bool bInBossFight = false; // 보스방에 들어와서 보스를 잡기 전까지 (이때 죽으면 보스방에서 다시 시작)
    bool bBossRetry = false;   // 재도전 중이면 통로(회복 안내) 단계는 건너뜀

    // 반복 재생 중인 효과 (UPROPERTY: GC 가 몰래 지우지 않게)
    UPROPERTY()
    TObjectPtr<class UNiagaraComponent> LoopFXComp;

    UPROPERTY()
    TObjectPtr<class UAudioComponent> LoopSoundComp;
};
