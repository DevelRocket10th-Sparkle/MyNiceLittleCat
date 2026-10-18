// 작성자: 강진구
// 작성일자: 2026-10-12
// 
// 플레이어 클래스입니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class ULockOnComponent;
class UInputMappingContext;
class UInputAction;
class UAnimMontage;
class UStaticMeshComponent;
class UPlayerAttributeSet;
class UGA_MeleeAttack;
struct FInputActionValue;

UCLASS()
class MYNICELITTLECAT_API APlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	APlayerCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	void SetbIsAbleToDodge(bool bNewIsAbleToDodge) { bIsAbleToDodge = bNewIsAbleToDodge; }
	bool GetbIsAbleToAttack() const { return bIsAbleToAttack; }

	UStaticMeshComponent* GetWeaponMesh() const{ return WeaponMesh.Get(); }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 에디터에서 호출하는 피격 테스트 버튼
	UFUNCTION(CallInEditor, Category = "Hit Reaction")
	void TestReceiveHit();

	UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
	void 
		ApplyPlayerHit(float Damage, FVector HitDirection);

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

	// 공격 입력 시 실행 함수
	void OnMeleeAttackInput(const FInputActionValue& Value);

private:
	void StartHitReaction(FVector HitDirection);
	void ToggleHitFlash();
	void EndInvincibility();

protected:

	FTimerHandle DodgeUpdateTimer;
	FTimerHandle DodgeEndTimer;
	FTimerHandle InvincibilityTimerHandle;
	FTimerHandle BlinkTimerHandle;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction")
	TObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction",
		meta = (ClampMin = "0.0"))
	float TestDamage = 10.f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction",
		meta = (ClampMin = "0.0"))
	float InvincibilityDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction",
		meta = (ClampMin = "0.01"))
	float BlinkInterval = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction")
	float KnockbackStrength = 500.f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction")
	float KnockbackUpwardStrength = 150.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LockOn")
	TObjectPtr<ULockOnComponent> LockOnComponent;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn")
	float BodyTurnThreshold = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn")
	float BodyTurnInterpSpeed = 13.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Dodge;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_MeleeAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_TargetLock;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_TargetSwitch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UPlayerAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGA_MeleeAttack> MeleeAttackAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TWeakObjectPtr<UStaticMeshComponent> WeaponMesh;

private:
	bool bIsDodging = false;
	bool bIsAbleToDodge = true;
	bool bIsAbleToAttack = true;
	float OriginalMaxWalkSpeed = 0.0f;
	FVector DodgeDirection = FVector::ForwardVector;

	bool bIsInvincible = false;
	bool bMeshWasVisible = true;
	bool bHitFlashVisible = true;


};
