#include "Enemy/GAS/Abilities/GA_Enemy_MeleeAttack.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Enemy/GAS/EnemyCombatTags.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/Components/EnemyMeleeTraceComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AIController.h"
#include "Characters/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

UGA_Enemy_MeleeAttack::UGA_Enemy_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

// 요청 컨텍스트를 복사하고 판정 이벤트 및 몽타주 실행
void UGA_Enemy_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	PatternComponent = Enemy.IsValid() ? Enemy->GetPatternComponent() : nullptr;
	// GetActiveExecution이 출력할 원래 타깃
	AActor* Target = nullptr;
	if (!Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->GetActiveExecution(ExecutionPattern, Target, RequestId)
		)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	MeleeSettings = Cast<UEnemyMeleePatternSettings>(ExecutionPattern.ExecutionSettings);
	ExecutionTarget = Target;
	if (!MeleeSettings || !MeleeSettings->Montage || !ValidatePattern() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ExecutionTarget = Target;
	MeleeTraceComponent = Enemy->GetMeleeTraceComponent();
	if (!MeleeTraceComponent.IsValid() || !MeleeTraceComponent->PrepareAttack(MeleeSettings->MeleeTraceSettings, RequestId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Pattern %s has no valid melee traces"), *Enemy->GetName(), *ExecutionPattern.PatternId.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerHitDelegateHandle = MeleeTraceComponent->OnPlayerHit.AddUObject(this, &UGA_Enemy_MeleeAttack::HandlePlayerHit);
	if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
	{
		AI->StopMovement();
	}
	Enemy->GetCharacterMovement()->StopMovementImmediately();

	// 이동 잠금
	Enemy->AcquireAttackMovementLock();
	bOwnsMovementLock = true;

	// 회전 잠금
	if (ExecutionPattern.bLockRotation)
	{
		Enemy->AcquireRotationLock();
		bOwnsRotationLock = true;
	}

	// Notify 이벤트는 몽타주 시작 전에 구독
	UAbilityTask_WaitGameplayEvent* BeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, EnemyCombatTags::MeleeWindowBegin, nullptr, false, true);
	BeginTask->EventReceived.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleWindowBegin);
	BeginTask->ReadyForActivation();
	UAbilityTask_WaitGameplayEvent* EndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, EnemyCombatTags::MeleeWindowEnd, nullptr, false, true);
	EndTask->EventReceived.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleWindowEnd);
	EndTask->ReadyForActivation();
	// Ability가 취소되면 몽타주도 정지
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MeleeSettings->Montage, 1.f, MeleeSettings->MontageSection, true);
	MontageTask->OnCompleted.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_Enemy_MeleeAttack::HandleMontageCancelled);
	MontageTask->ReadyForActivation();
	if (IsActive())
	{
		OnMontageStarted();
	}
}

void UGA_Enemy_MeleeAttack::HandleWindowBegin(FGameplayEventData Payload)
{
	if (!IsActive() || !Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->IsRequestRunning(RequestId) || !MeleeTraceComponent.IsValid()
		|| Payload.Instigator != Enemy.Get() || Payload.OptionalObject2 != Enemy->GetMesh()) return;
	UAnimInstance* AnimInstance = Enemy->GetMesh()->GetAnimInstance();
	if (!MeleeSettings || !AnimInstance || AnimInstance->GetCurrentActiveMontage() != MeleeSettings->Montage)
	{
		return;
	}
	MeleeTraceComponent->BeginWindow(Payload.OptionalObject.Get());
}

void UGA_Enemy_MeleeAttack::HandleWindowEnd(FGameplayEventData Payload)
{
	if (MeleeTraceComponent.IsValid() && Enemy.IsValid() && Payload.Instigator == Enemy.Get()
		&& Payload.OptionalObject2 == Enemy->GetMesh())
		MeleeTraceComponent->EndWindow(Payload.OptionalObject.Get());
}

void UGA_Enemy_MeleeAttack::HandlePlayerHit(const FHitResult& Hit)
{
	if (!IsActive() || !Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !MeleeSettings || !PatternComponent->IsRequestRunning(RequestId)) return;
	APlayerCharacter* Player = Cast<APlayerCharacter>(Hit.GetActor());
	if (!IsValid(Player) || Player->GetbIsDead() || DamagedActors.Contains(Player))
	{
		return;
	}
	if (ExecutionPattern.bRequiresLineOfSight)
	{
		AAIController* AI = Cast<AAIController>(Enemy->GetController());
		if (!AI || !AI->LineOfSightTo(Player))
		{
			return;
		}
	}
	const float Damage = Enemy->GetAttributeSet()->GetAttackPower() * GetDamageMultiplier();
	if (!FMath::IsFinite(Damage) || Damage <= 0.f)
	{
		return;
	}
	FVector Direction = (Player->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Enemy->GetActorForwardVector().GetSafeNormal2D();
	}
	// 타격 콜백의 재진입과 취소에 대비해 검증 후 피해 적용 전에 타격 기록을 저장한다.
	DamagedActors.Add(Player);
	Player->ApplyPlayerHit(Damage, Direction);
}

float UGA_Enemy_MeleeAttack::GetDamageMultiplier() const
{
	return ExecutionPattern.DamageMultiplier;
}

void UGA_Enemy_MeleeAttack::HandleMontageCompleted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_Enemy_MeleeAttack::HandleMontageCancelled()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

// 정상 완료와 취소 모두 이 능력이 획득한 제한 및 참조 정리
void UGA_Enemy_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MeleeTraceComponent.IsValid())
	{
		MeleeTraceComponent->OnPlayerHit.Remove(PlayerHitDelegateHandle);
		MeleeTraceComponent->StopAttack(RequestId);
	}
	PlayerHitDelegateHandle.Reset();
	MeleeTraceComponent.Reset();
	if (Enemy.IsValid())
	{
		if (bOwnsRotationLock)
		{
			Enemy->ReleaseRotationLock();
		}
		if (bOwnsMovementLock)
		{
			Enemy->ReleaseAttackMovementLock();
		}
	}
	bOwnsRotationLock = false;
	bOwnsMovementLock = false;
	MeleeSettings = nullptr;
	ExecutionTarget = nullptr;
	RequestId = 0;
	DamagedActors.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
