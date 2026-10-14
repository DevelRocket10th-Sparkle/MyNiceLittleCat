// 작성자: 강진구
// 작성일자: 2026-10-12
// 
// 플레이어 클래스입니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PlayerComponents/LockOnComponent.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UAnimMontage;

UCLASS()
class MYNICELITTLECAT_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	APlayerCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	// 이동 함수
	void Move(const FInputActionValue& Value);
	// 락온 함수
	void ToggleTargetLock();
	// 락온 타겟 변경 함수
	void SwitchTarget(const FInputActionValue& Value);
	// 구르기 시작
	void StartDodge();
	void UpdateDodge();
	// 구르기 종료
	void EndDodge();

	// 구르기 입력 시 실행 함수
	void OnDodgeInput(const FInputActionValue& Value);

protected:
	FTimerHandle DodgeUpdateTimer;
	FTimerHandle DodgeEndTimer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LockOn")
	TObjectPtr<ULockOnComponent> LockOnComponent;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn")
	float BodyTurnThreshold = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn")
	float BodyTurnInterpSpeed = 4.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Dodge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_TargetLock;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_TargetSwitch;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeMontage;

private:
	bool bIsDodging = false;
	float OriginalMaxWalkSpeed = 0.0f;
	FVector DodgeDirection = FVector::ForwardVector;

};
