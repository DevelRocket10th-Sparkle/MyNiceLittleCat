// 작성자: 강진구
// 작성일자: 2026-10-12
// 
// 플레이어 클래스입니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Interfaces/TargetableInterface.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class ULockOnComponent;
class UCameraOcclusionComponent;
class UInputMappingContext;
class UInputAction;
class UAnimMontage;
class UStaticMeshComponent;
class UPlayerAttributeSet;
class UGA_MeleeAttack;
class UPlayerConfig;
class UNiagaraSystem;
class UNiagaraComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPotionCountChanged, int32 /*Current*/, int32 /*Max*/);

UCLASS()
class MYNICELITTLECAT_API APlayerCharacter : public ACharacter, public IAbilitySystemInterface, public ITargetableInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	APlayerCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 엔진 표준 피해 경로 (UGameplayStatics::ApplyDamage, ApplyPointDamage) -> ApplyPlayerHit
	// - 무적, 사망 확인과 GAS 피해 적용은 ApplyPlayerHit이 그대로 처리
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ITargetableInterface: 공격/락온 대상으로 유효한지 (사망 전)
	// - 락온은 자기 자신을 제외하므로 플레이어가 구현해도 자기 자신을 락온하지 않음
	virtual bool IsTargetAlive_Implementation() const override { return !bIsDead; }

	void SetbIsAbleToDodge(bool bNewIsAbleToDodge) { bIsAbleToDodge = bNewIsAbleToDodge; }
	bool GetbIsAbleToAttack() const { return bIsAbleToAttack; }
	void SetbIsAbleToMove(bool bNewIsAbleToMove) { bIsAbleToMove = bNewIsAbleToMove; }
	bool GetbIsDead() const { return bIsDead; }
	void SetDodgeInvincible(bool bNewDodgeInvincible);
	bool GetbIsDodgeInvincible() const { return bIsDodgeInvincible; }
	bool GetbIsRunning() const { return bIsRunning; }
	// 쉬움 모드
	bool IsEasyMode() const { return bEasyMode; }
	float GetAttackSpeedMultiplier() const { return bEasyMode ? EasyAttackSpeedMultiplier : 1.f; }
	bool GetbIsDeflected() const { return bIsDeflected; }
	bool GetbIsHealing() const { return bIsHealing; }
	int32 GetPotionCount() const { return CurrentPotions; }
	int32 GetMaxPotions() const { return MaxPotions; }

	UStaticMeshComponent* GetWeaponMesh() const{ return WeaponMesh; }
	UStaticMeshComponent* GetPotionMesh() const { return PotionMesh; }

	void PlayDeflect();
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

	UFUNCTION()
	void RefillPotions();
protected:
	// 이동 함수
	void Move(const FInputActionValue& Value);
	// 잠금 함수
	void ToggleTargetLock();
	// 잠금 타겟 변경 함수
	void SwitchTarget(const FInputActionValue& Value);
	// 구르기 시작
	void StartDodge();
	void UpdateDodge(float DeltaTime);
	// 구르기 종료
	void EndDodge();

	// 구르기 입력 시 실행 함수
	void OnDodgeInput(const FInputActionValue& Value);
	void OnDodgeReleased(const FInputActionValue& Value);
	void StartRun();
	void StopRun();
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

	void StartHeal();

	UFUNCTION()
	void OnHealMontageNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	void OnHealMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	// 근접한 Enemy 밀어내기
	void PushNearbyEnemies(const FVector& WorldMoveInput);

private:
	void StartHitReaction(FVector HitDirection);
	void ToggleHitFlash();
	void EndInvincibility();
	void EndHitStun();
	void DropWeapon();

	UFUNCTION()
	void OnDeflectBlendingOut(UAnimMontage* Montage, bool bInterrupted);


public:
	UPROPERTY(BlueprintAssignable, Category = "Character|Death")
	FOnPlayerDied OnPlayerDied;
	FOnPotionCountChanged OnPotionCountChanged;

protected:

	FTimerHandle DodgeUpdateTimer;
	FTimerHandle DodgeEndTimer;
	FTimerHandle InvincibilityTimerHandle;
	FTimerHandle HitStunTimerHandle;
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
	float InvincibilityDuration = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "Hit Reaction", meta = (ClampMin = "0", Units = "s"))
	float HitStunDuration = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction",
		meta = (ClampMin = "0.01"))
	float BlinkInterval = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction")
	float KnockbackStrength = 700.f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction")
	float KnockbackUpwardStrength = 150.f;

	/* 쉬움 모드: 넉백 없음, 최대 체력 EasyMaxHealth, 항상 달리기 속도 + 대시 키 누르는 즉시 구르기,
	   피격 무적 EasyInvincibilityDuration, 공격 속도 EasyAttackSpeedMultiplier배 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Easy Mode")
	bool bEasyMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Easy Mode", meta = (ClampMin = "1", EditCondition = "bEasyMode"))
	float EasyMaxHealth = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Easy Mode", meta = (ClampMin = "0", Units = "s", EditCondition = "bEasyMode"))
	float EasyInvincibilityDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Easy Mode", meta = (ClampMin = "0.1", EditCondition = "bEasyMode"))
	float EasyAttackSpeedMultiplier = 1.5f;

	// 쉬움 모드일 때 입에 무는 쪽쪽이 (Character_Mouth_Socket)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Easy Mode", meta = (EditCondition = "bEasyMode"))
	TObjectPtr<class UStaticMesh> EasyModePacifierMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Easy Mode", meta = (EditCondition = "bEasyMode"))
	FName EasyModePacifierSocket = TEXT("Character_Mouth_Socket");

	// 소켓 기준 위치·회전·크기 보정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Easy Mode", meta = (EditCondition = "bEasyMode"))
	FTransform EasyModePacifierOffset;

	// 쉬움 모드일 때 포션 대신 드는 젖병 (원래 포션과 높이가 같도록 자동으로 크기 맞춤)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Easy Mode", meta = (EditCondition = "bEasyMode"))
	TObjectPtr<class UStaticMesh> EasyModePotionMesh;

	// 자동으로 맞춘 젖병 크기에 곱할 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Easy Mode", meta = (ClampMin = "0.1", EditCondition = "bEasyMode"))
	float EasyModePotionScale = 1.f;

	// 해당 시간 보다 오래 홀드 시 달리기 시작 짧으면 구르기
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Run", meta = (ClampMin = "0.05"))
	float RunHoldThreshold = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Run", meta = (ClampMin = "1.0"))
	float RunSpeedMultiplier = 1.3f;

	FTimerHandle RunHoldTimer;

	bool bIsRunning = false;
	float WalkSpeedBeforeRun = 0.f;

	// 공격 시 허락되는 회전율
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Attack")
	float AttackTurnRate = 900.f;

	FVector LastMoveInputDir = FVector::ZeroVector;
	uint64 LastMoveInputFrame = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// 카메라와 플레이어 사이를 가리는 벽 / 천장 숨김
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraOcclusionComponent> CameraOcclusionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LockOn")
	TObjectPtr<ULockOnComponent> LockOnComponent;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn")
	float BodyTurnThreshold = 10.0f;

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

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Heal;

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

	UPROPERTY(EditDefaultsOnly, Category = "Deflect")
	TObjectPtr<UAnimMontage> DeflectMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	TObjectPtr<UAnimMontage> DrinkMontage;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "1"))
	int32 MaxConsecutiveDodges = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0", Units = "s"))
	float DodgeCooldown = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0", Units = "s"))
	float DodgeChainWindow = 0.5f;

	// 구르는 중 이동 입력 방향으로 꺾는 속도 (초당 각도, 0이면 조향 안 함)
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0", Units = "deg"))
	float DodgeSteerRate = 180.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float DodgeRollSpeed = 1000.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> PotionMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Heal", meta = (ClampMin = "0.0"))
	float HealAmount = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Heal", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealMoveSpeedMultiplier = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Heal", meta = (ClampMin = "0"))
	int32 MaxPotions = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Heal|FX")
	TObjectPtr<UNiagaraSystem> HealEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Heal|FX")
	FVector HealEffectOffset = FVector::ZeroVector;

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
	bool bIsHealing = false;
	bool bHealApplied = false;
	float SpeedBeforeHeal = 0.f;
	float OriginalMaxWalkSpeed = 0.0f;
	int32 CurrentPotions = 0;
	FVector DodgeDirection = FVector::ForwardVector;

	bool bIsInvincible = false;
	bool bMeshWasVisible = true;
	bool bHitFlashVisible = true;
	bool bIsDead = false;
	bool bIsDeflected = false;
	bool bSuppressHealEffect = false;

	int32 ConsecutiveDodges = 0;
	float LastDodgeEndTime = -100.f;
	float DodgeLockedUntil = 0.f;

	TEnumAsByte<ECollisionResponse> SavedCapsuleProjectileResponse = ECR_Block;

};
