#include "Enemy/GAS/GA_EnemyPatternAttack.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Enemy/GAS/EnemyCombatTags.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Characters/PlayerCharacter.h"
#include "Interfaces/TargetableInterface.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_EnemyPatternAttack::UGA_EnemyPatternAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

// 요청 컨텍스트를 복사하고 판정 이벤트 및 몽타주 실행
void UGA_EnemyPatternAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	PatternComponent = Enemy.IsValid() ? Enemy->GetPatternComponent() : nullptr;
	// GetActiveExecution이 출력할 원래 타깃
	AActor* Target = nullptr;
	if (!Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->GetActiveExecution(ExecutionPattern, Target, RequestId)
		|| !ExecutionPattern.Montage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ExecutionTarget = Target;
	bHitApplied = false;
	if (AAIController* AI = Cast<AAIController>(Enemy->GetController())) AI->StopMovement();
	Enemy->GetCharacterMovement()->StopMovementImmediately();
	if (ExecutionPattern.bFaceTargetOnStart && IsValid(Target))
	{
		// 공격 시작 시에만 타깃 방향으로 수평 정렬
		FVector Direction = Target->GetActorLocation() - Enemy->GetActorLocation();
		Direction.Z = 0.f;
		if (!Direction.IsNearlyZero()) Enemy->SetActorRotation(Direction.Rotation());
	}
	Enemy->AcquireAttackMovementLock();
	bOwnsMovementLock = true;
	if (ExecutionPattern.bLockRotation)
	{
		Enemy->AcquireRotationLock();
		bOwnsRotationLock = true;
	}
	// Notify 이벤트는 몽타주 시작 전에 구독
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, EnemyCombatTags::PatternHit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &UGA_EnemyPatternAttack::HandleHitEvent);
	HitTask->ReadyForActivation();
	// Ability가 취소되면 몽타주도 정지
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, ExecutionPattern.Montage, 1.f, ExecutionPattern.MontageSection, true);
	MontageTask->OnCompleted.AddDynamic(this, &UGA_EnemyPatternAttack::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_EnemyPatternAttack::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_EnemyPatternAttack::HandleMontageCancelled);
	MontageTask->ReadyForActivation();
}

// 현재 요청의 판정 시점에 실제 타깃 위치를 검사하고 피해 GE 적용
void UGA_EnemyPatternAttack::HandleHitEvent(FGameplayEventData Payload)
{
	if (!Enemy.IsValid() || !Enemy->IsAlive() || !PatternComponent.IsValid()
		|| !PatternComponent->IsRequestRunning(RequestId) || bHitApplied || !ExecutionPattern.DamageEffect) return;
	// 기본 근접 공격은 이번 요청의 원래 타깃 한 명만 판정
	AActor* Target = ExecutionTarget;
	if (!IsValid(Target)) return;
	if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Target))
	{
		if (Player->GetbIsDead()) return;
	}
	else if (Target->Implements<UTargetableInterface>() && !ITargetableInterface::Execute_IsTargetAlive(Target)) return;
	// 애니메이션 판정 시점의 실제 위치에서 거리와 장애물을 다시 검사
	const FVector Center = Enemy->GetActorLocation() + Enemy->GetActorForwardVector() * ExecutionPattern.HitForwardOffset;
	if (FVector::DistSquared(Center, Target->GetActorLocation()) > FMath::Square(ExecutionPattern.HitRadius)) return;
	AAIController* AI = Cast<AAIController>(Enemy->GetController());
	if (!AI || !AI->LineOfSightTo(Target)) return;
	// 피해를 주는 쪽과 받는 쪽의 ASC
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!SourceASC || !TargetASC) return;
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(Enemy.Get());
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(ExecutionPattern.DamageEffect, GetAbilityLevel(), Context);
	if (!Spec.IsValid()) return;
	if (ExecutionPattern.DamageMagnitudeTag.IsValid())
		Spec.Data->SetSetByCallerMagnitude(ExecutionPattern.DamageMagnitudeTag,
			Enemy->GetAttributeSet()->GetAttackPower() * ExecutionPattern.DamageMultiplier);
	bHitApplied = true;
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
}

void UGA_EnemyPatternAttack::HandleMontageCompleted()
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_EnemyPatternAttack::HandleMontageCancelled()
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

// 정상 완료와 취소 모두 이 능력이 획득한 제한 및 참조 정리
void UGA_EnemyPatternAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (Enemy.IsValid())
	{
		if (bOwnsRotationLock) Enemy->ReleaseRotationLock();
		if (bOwnsMovementLock) Enemy->ReleaseAttackMovementLock();
	}
	bOwnsRotationLock = false;
	bOwnsMovementLock = false;
	ExecutionTarget = nullptr;
	RequestId = 0;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
