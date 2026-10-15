// 작성자: 우진
// 작성일자: 2026-10-12

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Pet/PetTypes.h"
#include "PetCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPetModeChanged, EPetMode, PreviousMode, EPetMode, NewMode);

// 괴냥이 클래스
UCLASS()
class MYNICELITTLECAT_API APetCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APetCharacter();

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

private:
	// 초기화 됨?
	UPROPERTY(Transient)
	bool bInitialized = false;
};
