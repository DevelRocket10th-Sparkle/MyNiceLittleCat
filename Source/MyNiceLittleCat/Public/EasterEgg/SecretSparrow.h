/// 작성자: 우진
/// 작성일자: 2026-11-09
///
/// 이스터에그 상자 안에 숨어 있는 참새입니다.
/// 흐름:
///   1. 상자(Chest)가 닫혀 있는 동안은 숨어 있음 (뚜껑 틈으로 비치지 않게)
///   2. 상자가 열리면 Idle로 앉아 있고, 가끔 Hop 동작으로 "뭔가 있다"는 느낌을 줌
///   3. 플레이어가 가까이 오면 괴냥이가 힌트 대사를 함 (HintText)
///      튜토리얼이 끝나면 괴냥이가 플레이어를 따라다니므로 말풍선이 플레이어 근처에 뜸
///   4. 근처에서 숫자 9 → 2 → 2(SecretCode)를 순서대로 누르면 발동
///      - 상단 숫자키·넘패드 둘 다 인정, 키 사이 간격이 CodeKeyInterval을 넘으면 처음부터
///      - 숫자가 아닌 키(이동 등)는 무시, 틀린 숫자면 처음부터 (단 첫 숫자면 1단계로 인정)
///   5. 발동: 깡총 → 날아올라 플레이어 머리 위를 CircleLaps 바퀴 돈 뒤 하늘로 날아가 사라짐
///      (반짝 이펙트·소리, 괴냥이 반응 대사)
/// 한 번 날아가면 끝 (저장 안 함: 레벨을 다시 들어오면 다시 상자 안에 있음).
/// 메시·애니메이션·이펙트·소리 에셋은 BP에서 지정합니다. 메시 정면이 +X가 아니면 SparrowMesh 회전으로 맞춥니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "SecretSparrow.generated.h"

class AGimmickChest;
class APetCharacter;
class UAnimSequence;
class UInputComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;
class USoundBase;

UENUM(BlueprintType)
enum class ESecretSparrowState : uint8
{
	InClosedChest	UMETA(DisplayName = "닫힌 상자 안"),	// 숨어 있음, 입력 안 받음
	Waiting			UMETA(DisplayName = "대기"),			// 상자 열림, 커맨드 입력 대기
	Circling		UMETA(DisplayName = "선회"),			// 발동: 플레이어 머리 위를 도는 중
	FlyingAway		UMETA(DisplayName = "떠나는 중"),		// 하늘로 날아가는 중
	Gone			UMETA(DisplayName = "사라짐")
};

UCLASS()
class MYNICELITTLECAT_API ASecretSparrow : public AActor
{
	GENERATED_BODY()

public:
	ASecretSparrow();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "EasterEgg|Sparrow")
	ESecretSparrowState GetSparrowState() const { return State; }

	// 커맨드 성공 처리 (테스트용으로 직접 불러도 됨). 대기 상태에서만 동작
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "EasterEgg|Sparrow")
	void TriggerSecret();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 커맨드 숫자를 하나 맞출 때마다 (Progress = 맞춘 개수). 짧은 효과음 등 연출용
	UFUNCTION(BlueprintImplementableEvent, Category = "EasterEgg|Sparrow", meta = (DisplayName = "On Code Progress"))
	void K2_OnCodeProgress(int32 Progress);

	// 커맨드 성공 순간 (추가 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "EasterEgg|Sparrow", meta = (DisplayName = "On Secret Triggered"))
	void K2_OnSecretTriggered();

	// 하늘로 사라지기 직전 (추가 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "EasterEgg|Sparrow", meta = (DisplayName = "On Flown Away"))
	void K2_OnFlownAway();

private:
	// ---- 상태 진행 ----
	void EnterWaiting();
	void TickWaiting(float DeltaSeconds);
	void TickCircling(float DeltaSeconds);
	void TickFlyingAway(float DeltaSeconds);
	void FinishFlyAway();

	// ---- 커맨드 입력 ----
	void BindCodeInput();
	void UnbindCodeInput();
	void HandleAnyKey(FKey Key);
	static int32 KeyToDigit(const FKey& Key);

	// ---- 애니메이션 ----
	void PlayAnim(UAnimSequence* Anim, bool bLoop);
	void ScheduleIdleHop();
	void PlayIdleHop();

	// ---- 유틸 ----
	APawn* GetPlayerPawn() const;
	bool IsPlayerInRange(float Radius) const;
	void FaceDirection(const FVector& Direction, float DeltaSeconds, bool bAllowPitch);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// 참새 메시 (SK_Sparrow). 콜리전 없음: 상자 안에 겹쳐 있어도, 날아갈 때도 아무것도 막지 않음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> SparrowMesh;

	// ===== 상자 =====

	// 들어 있는 상자 (레벨에서 지정). 비우면 처음부터 열린 상자로 취급
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "EasterEgg|Chest")
	TObjectPtr<AGimmickChest> Chest;

	// 상자가 닫혀 있는 동안 숨기기 (뚜껑 틈으로 비치는 것 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Chest")
	bool bHideWhileChestClosed = true;

	// ===== 애니메이션 =====

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Anim")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Anim")
	TObjectPtr<UAnimSequence> HopAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Anim")
	TObjectPtr<UAnimSequence> FlyAnim;

	// 대기 중 가끔 깡총거리는 간격 (최소~최대 사이 무작위)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Anim", meta = (ClampMin = "0.5", Units = "s"))
	float IdleHopIntervalMin = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Anim", meta = (ClampMin = "0.5", Units = "s"))
	float IdleHopIntervalMax = 6.f;

	// 대기 중 플레이어 쪽으로 고개(몸)를 돌리는 거리 (0이면 안 돌림)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Anim", meta = (ClampMin = "0.0", Units = "cm"))
	float LookAtPlayerRadius = 400.f;

	// ===== 비밀 커맨드 =====

	// 순서대로 누를 숫자 (0~9). 기본 9, 2, 2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Code")
	TArray<int32> SecretCode;

	// 플레이어가 이 거리 안에 있을 때만 커맨드를 받음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Code", meta = (ClampMin = "0.0", Units = "cm"))
	float CodeListenRadius = 500.f;

	// 숫자 사이 최대 간격. 넘으면 처음부터 다시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Code", meta = (ClampMin = "0.1", Units = "s"))
	float CodeKeyInterval = 1.5f;

	// ===== 괴냥이 힌트 =====

	// 플레이어가 이 거리 안으로 들어오면 (괴냥이가 상자 옆에 있을 때) 힌트를 말함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Hint", meta = (ClampMin = "0.0", Units = "cm"))
	float HintTriggerRadius = 350.f;

	// 힌트 대사 (커맨드를 소리로 암시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Hint")
	FText HintText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Hint", meta = (ClampMin = "0.1", Units = "s"))
	float HintDuration = 4.f;

	// 상자가 열리고 참새가 나온 뒤 첫 힌트까지 기다리는 시간 (헛탕 대사·뚜껑 연출과 겹치지 않게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Hint", meta = (ClampMin = "0.0", Units = "s"))
	float HintDelayAfterOpen = 1.5f;

	// 힌트를 한 번 말한 뒤 다시 말하기까지 최소 시간 (플레이어가 멀어졌다 돌아오면 다시 말함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Hint", meta = (ClampMin = "0.0", Units = "s"))
	float HintCooldown = 15.f;

	// ===== 발동 연출 =====

	// 커맨드 성공 시 괴냥이 반응 대사
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly")
	FText PetReactionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly")
	TObjectPtr<USoundBase> SecretSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly")
	TObjectPtr<UNiagaraSystem> SecretEffect;

	// 날기 전 깡총 동작 시간 (이 동안은 제자리)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly", meta = (ClampMin = "0.0", Units = "s"))
	float TakeOffDelay = 0.35f;

	// 플레이어 중심 선회 반지름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly", meta = (ClampMin = "10.0", Units = "cm"))
	float CircleRadius = 120.f;

	// 플레이어 위치 기준 선회 높이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly", meta = (Units = "cm"))
	float CircleHeight = 170.f;

	// 선회 각속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly", meta = (ClampMin = "10.0"))
	float CircleDegreesPerSecond = 220.f;

	// 도는 바퀴 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly", meta = (ClampMin = "0.0"))
	float CircleLaps = 2.f;

	// 선회 궤도를 따라가는 부드러움 (클수록 궤도에 딱 붙음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly", meta = (ClampMin = "0.1"))
	float CircleFollowSpeed = 5.f;

	// 떠날 때 시작 속도·가속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly", meta = (ClampMin = "0.0", Units = "cm/s"))
	float FlyAwayStartSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly", meta = (ClampMin = "0.0"))
	float FlyAwayAcceleration = 700.f;

	// 떠나는 비행 시간 (끝나면 액터 제거)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EasterEgg|Fly", meta = (ClampMin = "0.1", Units = "s"))
	float FlyAwayDuration = 2.5f;

private:
	ESecretSparrowState State = ESecretSparrowState::InClosedChest;
	float StateElapsed = 0.f;

	// 커맨드 진행
	int32 CodeProgress = 0;
	double LastCodeKeyTime = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> CodeInput;

	TWeakObjectPtr<APlayerController> CodeInputController;

	// 힌트 진행
	bool bPlayerWasInHintRadius = false;
	double NextHintAllowedTime = 0.0;

	// 비행
	float CircleAngle = 0.f;			// 현재 선회 각도 (도)
	float CircledDegrees = 0.f;			// 누적 선회량 (도)
	FVector LastCircleCenter = FVector::ZeroVector;
	FVector FlyAwayDirection = FVector::UpVector;
	float FlyAwaySpeed = 0.f;

	FTimerHandle IdleHopTimer;
	FTimerHandle AnimReturnTimer;
};
