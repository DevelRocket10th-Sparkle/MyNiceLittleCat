// 작성자: 우진
// 작성일자: 2026-10-12

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Pet/PetTypes.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "PetCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPetModeChanged, EPetMode, PreviousMode, EPetMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPetModeUnlocked, EPetMode, UnlockedMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPetModeUseInput, EPetModeUseInputPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPetAbilityMoveFinished, FGuid, RequestId, bool, bSucceeded);
// 괴냥이 클래스
UCLASS()
class MYNICELITTLECAT_API APetCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APetCharacter();

public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PostInitializeComponents() override; //! CarraySocket 초기화가 안되어서 임시로 여기서..

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// =============================================================================
public:
	UFUNCTION(BlueprintCallable, Category = "Pet|Init")
	void InitializePet(const FPetInitializationData& _data);

	UFUNCTION(BlueprintPure, Category = "Pet|Init")
	bool IsPetInitialized() const { return bInitialized; }

	UFUNCTION(BlueprintPure, Category = "Pet")
	APawn* GetPartnerPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Pet")
	EPetBehavior GetRequestedBehavior() const { return RequestedBehavior; }

	UFUNCTION(BlueprintCallable, Category = "Pet|Behavior")
	bool RequestBehavior(EPetBehavior NewBehavior); // 행동을 요청하는 함수

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugRequestIdle(); //! State 테스트용

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugRequestFollow(); //! State 테스트용

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugRequestMoveTo(); //! State 테스트용

	UFUNCTION(BlueprintCallable, Category = "Pet|Behavior")
	bool RequestMoveTo(const FVector& _destination);

	UFUNCTION(BlueprintPure, Category = "Pet|Behavior")
	FVector GetMoveDestination() const { return MoveDestination;}

	UFUNCTION(BlueprintCallable, Category = "Pet|Mode")
	bool RequestMode(EPetMode _newMode); // 실제 UI에서 모드를 변경하는 함수

	UFUNCTION(BlueprintPure)
	EPetMode GetCurrentMode() const { return CurrentMode; }

	// 이 모드를 쓸 수 있는지 (Basic은 항상 true, 나머지는 능력 해금 아이템 보유 여부)
	// 의도: 모드 능력을 "얻은 것만" 쓰게 해서 튜토리얼/진행도에 따라 모드가 열리게
	UFUNCTION(BlueprintPure, Category = "Pet|Mode")
	bool IsModeUnlocked(EPetMode _mode) const;

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugRequestBasicMode(); //! Mode 테스트용

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugRequestMagnetMode(); //! Mode 테스트용

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugRequestReflectMode(); //! Mode 테스트용

	void SetAimDirection(const FVector& _direction);
	FVector GetAimDirection() const { return AimDirection; }

	bool TryGetModeAbilityHandle(EPetMode _mode, FGameplayAbilitySpecHandle& _outHandle) const;

	//void SetModeAbilityInputHeld(bool _held) { bModeAbilityInputHeld = _held;}
	void RequestModeUseInput(EPetModeUseInputPhase Phase);

	void RequestSpecialAction();

	USceneComponent* GetCarryPoint() const { return CarryPoint.Get(); }

	FGuid RequestAbilityMoveTo(const FVector& Destination);

	void CompleteAbilityMove(const FGuid& RequestId, bool bSucceeded);

	void CancelAbilityMove(const FGuid& RequestId);

	FGuid GetActiveAbilityMoveRequestId() const { return ActiveAbilityMoveRequestId; }

	// 해당 모드의 능력이 실제 실행 중인지 확인
	UFUNCTION(BlueprintPure, Category = "Pet|Mode")
	bool IsModeAbilityActive(EPetMode _mode) const;

	UFUNCTION(BlueprintPure, Category = "Pet|Animation")
	EPetMovementStyle GetAnmationMovementStyle() const { return AnimationMovementStyle; }

	void SetAnimationMovementStyle(EPetMovementStyle _newStyle) { AnimationMovementStyle = _newStyle; }

	class USpeechBubbleComponent* GetSpeechBubble() const { return SpeechBubble; }

	UFUNCTION(BlueprintPure, Category = "Pet|Carry")
	AActor* GetCarriedActor() const;

		// MaxPathLength: 0이면 경로 길이 제한 없음 (기존 호출은 그대로 동작)
	bool CanResumeFollowingAt(const FVector& CapsuleLocation, float MaxPathLength = 0.f);

	// 성공했을 때만 OutLocation에 안전한 캡슐 중심 위치를 반환
	bool TryFindSafeRecoveryLocation(const FVector& PreferredLocation, FVector& OutLocation, bool bRequireLineOfSight = false);

	void ResumeNavigationSafely(EMovementMode RestoreMode, uint8 RestoreCustomMode = 0);

	void CancelNavigationRecovery();

	bool IsNavigationRecoveryPending() { return bNavigationRecoveryPending;}

	// ==== 자석 극 고정 ===
	// 외부에서 펫의 자성 극을 특저 극으로 고정
	UFUNCTION(BlueprintCallable, Category = "Pet|Magnet")
	void LockMagnetPolarity(EPetMagnetPolarity _polarity);

	// 극 고정 해제 (극은 고정됐던 극 그대로 유지되고, 다시 Q로 바꿀 수 있어요)
	UFUNCTION(BlueprintCallable, Category = "Pet|Magnet")
	void UnlockMagnetPolarity();

	// 고정 중일 때만 true + 고정된 극을 돌려줘요 (GA 자성 능력이 매 틱 읽어요)
	bool TryGetLockedMagnetPolarity(EPetMagnetPolarity& _outPolarity) const;

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugLockPolarityN(); //! 극 고정 테스트용

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugLockPolarityS(); //! 극 고정 테스트용

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugUnlockPolarity(); //! 극 고정 테스트용

	// 자성 사용 중 이 캡슐 위치에 배치해도 되는지 검사
	bool CanUseMagnetPosition(const FVector& CapsuleLocation);

	// 플레이어와 위치 사이에 World Static(벽)이 없는지
	// 의도: 경로가 돌아서라도 이어지면 통과되는 문제를 막고, "벽 너머" 배치를 확실히 거르기 위함
	bool HasClearLineFromPlayer(const FVector& Location) const;

	// 자성 공전 중: 이 자리에서 자기장을 써도 되는지 (벽 속·허공·벽 너머가 아닌지)
	bool IsMagnetPlacementValidAt(const FVector& CapsuleLocation);

	// 자성 해제 시: 이 자리에 펫을 그대로 내려놔도 되는지 (플레이어까지 경로 + 벽 없음)
	bool CanDropPetAt(const FVector& CapsuleLocation);

	// 공전 Task가 매 틱 기록 → 자성 능력이 읽어서 자기장을 끄고 켬
	void SetMagnetPlacementValid(bool bValid) { bMagnetPlacementValid = bValid; }
	bool IsMagnetPlacementValid() const { return bMagnetPlacementValid; }

	// 자성 모드의 자기장 영역 표시용 메시 (모양 갱신은 GameplayAbility_PetMagnet이 담당)
	class UProceduralMeshComponent* GetMagnetFieldMesh() const { return MagnetFieldMesh; }

	// ==== 효과음 ====
	// 능력(GA)들이 펫을 통해 사운드 데이터를 꺼내 쓰게
	class UPetSoundData* GetSoundData() const { return SoundData; }

	// 2D 재생: 플레이어 조작 피드백용 (거리와 상관없이 같은 크기)
	void PlayPetSound2D(class USoundBase* _sound) const;

	// 3D 재생: 월드 사건용, 공통 감쇠를 적용해서 펫 위치에서 재생
	void PlayPetSound3D(class USoundBase* _sound) const;

	// 3D 재생: 펫이 아닌 곳에서 나는 소리 (반사판 설치/철거 위치 등)
	void PlayPetSoundAtLocation(class USoundBase* _sound, const FVector& _location) const;
private:
	void GrantModeAbilities();         // 해금된 모드만 능력 부여
	bool GrantModeAbility(EPetMode _mode); // 모드 하나만 능력 부여 (이미 있으면 false)
	static FGameplayTag GetUnlockTag(EPetMode _mode); // 모드 → 해금 태그 (Basic은 빈 태그)
	void TryCompleteNavigationRecovery();

	// 인벤토리에서 능력 아이템을 처음 얻은 순간 → 해당 모드 능력 바로 부여
	UFUNCTION()
	void HandleAbilityUnlocked(FGameplayTag _abilityTag, class UItemData* _item);
	// =============================================================================
public:
	UPROPERTY(BlueprintAssignable, Category = "Pet|Mode")
	FOnPetModeChanged OnPetModeChanged;

	// 게임 중 새 모드가 해금된 순간 (UI 연출용)
	UPROPERTY(BlueprintAssignable, Category = "Pet|Mode")
	FOnPetModeUnlocked OnPetModeUnlocked;

	UPROPERTY(BlueprintAssignable, Category = "Pet|Input")
	FOnPetModeUseInput OnPetModeUseInput;

	UPROPERTY(BlueprintAssignable, Category = "Pet|Behavior")
	FOnPetAbilityMoveFinished OnPetAbilityMoveFinished;
protected:
	// 펫 초기화 데이터
	UPROPERTY(Transient)
	FPetInitializationData InitializationData;

	// 펫이 요청받은 행동
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pet|Behavior", meta = (AllowPrivateAccess = "true"))
	EPetBehavior RequestedBehavior = EPetBehavior::Idle;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pet|Behavior", meta = (AllowPrivateAccess = "true"))
	FVector MoveDestination = FVector::ZeroVector;

	//! State 테스트용
	UPROPERTY(EditAnywhere, Category = "Pet|Debug")
	FVector DebugMoveDestination = FVector::ZeroVector; 

	// 펫 모드
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pet", meta = (AllowPrivateAccess = "true"))
	EPetMode CurrentMode = EPetMode::Basic;

	// 자성 극이 고정됐나요? (고정 중에는 Q 극 변환 무시)
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Pet|Magnet")
	bool bMagnetPolarityLocked = false;

	// 고정된 극 (bMagnetPolarityLocked가 true일 때만 의미 있어요)
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Pet|Magnet")
	EPetMagnetPolarity LockedMagnetPolarity = EPetMagnetPolarity::N;

	// 마우스 커서 방향 저장
	UPROPERTY(VisibleInstanceOnly, Category = "Pet|Input")
	FVector AimDirection = FVector::ForwardVector;

	// GAS!!
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pet|GAS")
	TObjectPtr<class UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Pet|GAS")
	TMap<EPetMode, TSubclassOf<class UGameplayAbility>> ModeAbilities;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pet|Input", meta = (AllowPrivateAccess = "true"))
	bool bModeAbilityInputHeld = false; // 입력 누르는 중?

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pet|Carry")
	TObjectPtr<class USceneComponent> CarryPoint;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pet|Animation")
	EPetMovementStyle AnimationMovementStyle = EPetMovementStyle::Normal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pet|SpeechBubble")
	TObjectPtr<class USpeechBubbleComponent> SpeechBubble; // 머리 위 말풍선

	UPROPERTY(EditDefaultsOnly, Category = "Pet|Recovery",
		meta = (ClampMin = "100.0", Units = "cm"))
	float RecoveryStartRadius = 180.f;

	UPROPERTY(EditDefaultsOnly, Category = "Pet|Recovery",
		meta = (ClampMin = "10.0", Units = "cm"))
	float RecoveryRadiusStep = 100.f;

	// 착지 후보에서 플레이어까지 "걸어서" 갈 수 있는 최대 경로 길이 (0이면 제한 없음)
	// 의도: 보이긴 하지만 난간·구멍 건너편이라 빙 돌아야 하는 자리에 착지해
	//       비행 → 착지 → 다시 비행이 반복되는 것을 방지
	UPROPERTY(EditDefaultsOnly, Category = "Pet|Recovery", meta = (ClampMin = "0.0", Units = "cm"))
	float RecoveryMaxPathLength = 800.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pet|Magnet")
	TObjectPtr<class UProceduralMeshComponent> MagnetFieldMesh; // 자기장 부채꼴 표시

	// 지금 펫 위치에서 자기장을 써도 되는지 (PetMagnetPositionTask가 갱신)
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Pet|Magnet")
	bool bMagnetPlacementValid = true;

	// 펫 효과음 데이터 (BP에서 DA_PetSound 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Pet|Sound")
	TObjectPtr<class UPetSoundData> SoundData;
private:
	// 초기화 됨?
	UPROPERTY(Transient)
	bool bInitialized = false;

	// 등록한 능력을 나중에 정확히 실행, 취소하기 위한
	UPROPERTY(Transient)
	TMap<EPetMode, FGameplayAbilitySpecHandle> ModeAbilityHandles;

	// 요청 벊로
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Pet|Behavior")
	FGuid ActiveAbilityMoveRequestId;

	FTimerHandle NavigationRecoveryTimer;

	bool bNavigationRecoveryPending = false;

	TEnumAsByte<EMovementMode> RecoveryMovementMode = MOVE_Walking;

	uint8 RecoveryCustomMovementMode = 0;
};
