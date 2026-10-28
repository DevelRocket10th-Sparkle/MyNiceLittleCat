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
class UPlayerConfig;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

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
	void SetbIsAbleToMove(bool bNewIsAbleToMove) { bIsAbleToMove = bNewIsAbleToMove; }
	bool GetbIsDead() const { return bIsDead; }
	void SetDodgeInvincible(bool bNewDodgeInvincible) { bIsDodgeInvincible = bNewDodgeInvincible; }

	UStaticMeshComponent* GetWeaponMesh() const{ return WeaponMesh; }
	UStaticMeshComponent* GetPotionMesh() const { return PotionMesh; }
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

	UFUNCTION(CallInEditor, Category = "Hit Reaction")
	void TestRespawn();

	UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
	void ApplyPlayerHit(float Damage, FVector HitDirection);

protected:
	// 이동 함수
	void Move(const FInputActionValue& Value);
	// 잠금 함수
	void ToggleTargetLock();
	// 잠금 타겟 변경 함수
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

	// 체력 변경 시 처리 함수
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	// 사망
	void Die();

	// PlayerConfig 값을 어트리뷰트에 적용하는 함수
	void ApplyStatData();

	UFUNCTION()
	void OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// 배트 떨구기만을 위한 애님 노티파이 콜백 함수
	UFUNCTION()
	void OnDeathMontageNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	// 근접한 Enemy 밀어내기
	void PushNearbyEnemies(const FVector& WorldMoveInput);

private:
	void StartHitReaction(FVector HitDirection);
	void ToggleHitFlash();
	void EndInvincibility();
	void DropWeapon();


public:
	UPROPERTY(BlueprintAssignable, Category = "Character|Death")
	FOnPlayerDied OnPlayerDied;

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
	float KnockbackStrength = 700.f;

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
	TObjectPtr<UPlayerAttributeSet> PlayerAttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	TObjectPtr<UPlayerConfig> StatData;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGA_MeleeAttack> MeleeAttackAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Character|Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> PotionMesh;

	/* Enemy 밀어내기 관련 */

	// 입력 세기와 방향 일치도를 곱하기 전의 기준 밀기 속력 (cm/s)
	UPROPERTY(EditDefaultsOnly, Category = "Push", meta = (ClampMin = "0.0"))
	float EnemyPushSpeed = 120.f;

	// 직접 밀기 대상 탐색 시 캡슐 반경에 더할 여유 폭 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Push", meta = (ClampMin = "0.0"))
	float EnemyPushContactMargin = 8.f;

	// 이 값보다 방향 일치도가 큰 Enemy만 직접 밀기 대상으로 선택
	UPROPERTY(EditDefaultsOnly, Category = "Push", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinPushAlignment = 0.2f;

private:
	bool bIsDodging = false;
	bool bIsAbleToDodge = true;
	bool bIsAbleToAttack = true;
	bool bIsAbleToMove = true;
	bool bIsDodgeInvincible = false;
	float OriginalMaxWalkSpeed = 0.0f;
	FVector DodgeDirection = FVector::ForwardVector;

	bool bIsInvincible = false;
	bool bMeshWasVisible = true;
	bool bHitFlashVisible = true;
	bool bIsDead = false;


};
