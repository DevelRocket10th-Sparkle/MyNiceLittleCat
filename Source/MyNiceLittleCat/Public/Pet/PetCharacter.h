// 작성자: 우진
// 작성일자: 2026-10-12

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Pet/PetTypes.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "PetCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPetModeChanged, EPetMode, PreviousMode, EPetMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPetModeUseInput, EPetModeUseInputPhase, Phase);

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

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugRequestBasicMode(); //! Mode 테스트용

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugRequestMagnetMode(); //! Mode 테스트용

	UFUNCTION(CallInEditor, Category = "Pet|Debug")
	void DebugRequestReflectMode(); //! Mode 테스트용

	void SetAimDirection(const FVector& _direction);
	FVector GetAimDirection() const { return AimDirection; }

	bool TryGetModeAbilityHandle(EPetMode _mode, FGameplayAbilitySpecHandle& _outHandle) const;

	void SetModeAbilityInputHeld(bool _held) { bModeAbilityInputHeld = _held;}

	void RequestSpecialAction();

	USceneComponent* GetCarryPoint() const { return CarryPoint.Get(); }
private:
	void GrantModeAbilities();
	// =============================================================================
public:
	UPROPERTY(BlueprintAssignable, Category = "Pet|Mode")
	FOnPetModeChanged OnPetModeChanged;

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
private:
	// 초기화 됨?
	UPROPERTY(Transient)
	bool bInitialized = false;

	// 등록한 능력을 나중에 정확히 실행, 취소하기 위한
	UPROPERTY(Transient)
	TMap<EPetMode, FGameplayAbilitySpecHandle> ModeAbilityHandles;
};
