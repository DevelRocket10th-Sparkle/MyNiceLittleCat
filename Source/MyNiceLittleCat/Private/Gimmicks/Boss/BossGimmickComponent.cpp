/// 작성자: 최요환
/// 작성일자: 2026-10-22
///
/// 보스 페이즈 기믹 연결 컴포넌트 구현

#include "Gimmicks/Boss/BossGimmickComponent.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Components/EnemyPhaseComponent.h"
#include "Gimmicks/Boss/Barrier.h"
#include "Gimmicks/Boss/MagneticCoinPatternComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "UObject/Package.h"

UBossGimmickComponent::UBossGimmickComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBossGimmickComponent::ReceiveLevelGimmickReferences_Implementation(const TMap<FName, AActor*>& InPhaseActors, AActor* InAreaCenter)
{
	PhaseActors.Reset();
	for (const TPair<FName, AActor*>& Pair : InPhaseActors)
	{
		PhaseActors.Add(Pair.Key, Pair.Value);
	}
	AreaCenter = InAreaCenter;
}

AActor* UBossGimmickComponent::GetPhaseActor(FName Key) const
{
	const TObjectPtr<AActor>* FoundActor = PhaseActors.Find(Key);
	return FoundActor ? FoundActor->Get() : nullptr;
}

bool UBossGimmickComponent::EnterBossPhase_Implementation(FName PhaseId, int64 PhaseExecutionId)
{
	// 이전 페이즈 기믹이 남아 있으면 먼저 정리
	StopAll();

	// 진입 중인 페이즈 종류는 보스 페이즈 컴포넌트에서 확인 (전투 페이즈도 진입 호출이 옴)
	const ABossCharacter* Boss = Cast<ABossCharacter>(GetOwner());
	const UEnemyPhaseComponent* PhaseComponent = Boss ? Boss->GetPhaseComponent() : nullptr;
	const FBossPhaseDefinition* Phase = PhaseComponent ? PhaseComponent->GetCurrentPhase() : nullptr;
	const bool bBarrierPhase = Phase && Phase->PhaseId == PhaseId && Phase->Kind == EBossPhaseKind::Barrier;

	if (bBarrierPhase)
	{
		const TObjectPtr<AActor>* FoundActor = PhaseActors.Find(PhaseId);
		ABarrier* Barrier = FoundActor ? Cast<ABarrier>(FoundActor->Get()) : nullptr;
		if (!IsValid(Barrier))
		{
			UE_LOG(LogTemp, Error, TEXT("[BossGimmick] %s: 배리어 페이즈 %s의 배리어가 없습니다. 매니저 PhaseGimmickActors를 확인하세요."),
				*GetNameSafe(GetOwner()), *PhaseId.ToString());
			return false;
		}

		ActivePhaseId = PhaseId;
		ActivePhaseExecutionId = PhaseExecutionId;
		ActiveBarrier = Barrier;
		Barrier->OnBarrierBroken.AddUniqueDynamic(this, &UBossGimmickComponent::HandleBarrierBroken);
		// 나타나는 연출이 보스 위치에서 시작하도록 켜기 전에 붙임 (꺼지고 다 사라지면 배리어가 원래 자리로 복귀)
		if (bAttachBarrierToBoss)
		{
			Barrier->AttachToFollowTarget(GetOwner());
		}
		Barrier->ActivateBarrier();
		UE_LOG(LogTemp, Log, TEXT("[BossGimmick] %s 진입: 배리어 %s 활성"), *PhaseId.ToString(), *Barrier->GetName());
	}

	StartCoinPattern(PhaseId);

	return true;
}

void UBossGimmickComponent::StartCoinPattern(FName PhaseId)
{
	const FName* ActorKey = CoinPatternPhases.Find(PhaseId);
	if (!ActorKey)
	{
		return;
	}

	UMagneticCoinPatternComponent* CoinPattern = nullptr;
	if (ActorKey->IsNone())
	{
		// 보스 자신의 컴포넌트: 영역 중심은 매니저가 넘긴 중심
		CoinPattern = GetOwner() ? GetOwner()->FindComponentByClass<UMagneticCoinPatternComponent>() : nullptr;
		if (CoinPattern)
		{
			CoinPattern->SetSpawnCenterActor(AreaCenter);
		}
	}
	else
	{
		// 레벨 액터의 컴포넌트: 영역 중심은 그 액터 (인스턴스 설정 유지)
		const AActor* PatternActor = GetPhaseActor(*ActorKey);
		CoinPattern = PatternActor ? PatternActor->FindComponentByClass<UMagneticCoinPatternComponent>() : nullptr;
	}

	if (!CoinPattern)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossGimmick] %s: %s의 코인 패턴을 찾지 못해 건너뜁니다 (키 %s). 매니저 PhaseGimmickActors 또는 보스 컴포넌트를 확인하세요."),
			*GetNameSafe(GetOwner()), *PhaseId.ToString(), *ActorKey->ToString());
		return;
	}

	ActiveCoinPattern = CoinPattern;
	CoinPattern->StartPattern();
	UE_LOG(LogTemp, Log, TEXT("[BossGimmick] %s 진입: 자성 코인 패턴 시작 (%s)"), *PhaseId.ToString(), *GetNameSafe(CoinPattern->GetOwner()));
}

void UBossGimmickComponent::StopBossGimmicks_Implementation()
{
	StopAll();
}

void UBossGimmickComponent::HandleBarrierBroken(ABarrier* Barrier)
{
	// 이전 배리어의 알림이나 중복 완료 무시
	if (Barrier != ActiveBarrier || ActivePhaseExecutionId == 0)
	{
		return;
	}

	// 정리 함수가 저장 값을 비우므로 먼저 복사
	const FName PhaseId = ActivePhaseId;
	const int64 PhaseExecutionId = ActivePhaseExecutionId;

	// 구독 해제, 코인 패턴 정지, 남은 기믹 정리 후 다음 페이즈 요청
	StopAll();

	ABossCharacter* Boss = Cast<ABossCharacter>(GetOwner());
	const bool bAccepted = IsValid(Boss) && Boss->NotifyPhaseGimmickCompleted(this, PhaseId, PhaseExecutionId);
	UE_LOG(LogTemp, Log, TEXT("[BossGimmick] %s 배리어 붕괴 → 완료 통지 %s"), *PhaseId.ToString(), bAccepted ? TEXT("수락") : TEXT("거부"));
}

void UBossGimmickComponent::StopAll()
{
	if (IsValid(ActiveBarrier))
	{
		ActiveBarrier->OnBarrierBroken.RemoveDynamic(this, &UBossGimmickComponent::HandleBarrierBroken);
		if (bDeactivateBarrierOnStop)
		{
			ActiveBarrier->DeactivateBarrier();
		}
	}
	ActiveBarrier = nullptr;
	ActivePhaseId = NAME_None;
	ActivePhaseExecutionId = 0;

	if (IsValid(ActiveCoinPattern))
	{
		ActiveCoinPattern->StopPattern(bClearCoinsOnStop);
	}
	ActiveCoinPattern = nullptr;

	// 페이즈 진입·재도전 때 항상 첫 순번부터
	CurrentLaserTurn = FirstLaserTurn;
}

bool UBossGimmickComponent::IsLaserAlternationPhase() const
{
	const ABossCharacter* Boss = Cast<ABossCharacter>(GetOwner());
	const UEnemyPhaseComponent* PhaseComponent = Boss ? Boss->GetPhaseComponent() : nullptr;
	return PhaseComponent && LaserAlternationPhaseIds.Contains(PhaseComponent->GetCurrentPhaseId());
}

bool UBossGimmickComponent::IsLaserTurnAllowed(EBossLaserTurn Turn) const
{
	return !IsLaserAlternationPhase() || CurrentLaserTurn == Turn;
}

void UBossGimmickComponent::CompleteLaserTurn(EBossLaserTurn FinishedTurn)
{
	if (!IsLaserAlternationPhase() || CurrentLaserTurn != FinishedTurn)
	{
		return;
	}

	CurrentLaserTurn = FinishedTurn == EBossLaserTurn::Grid ? EBossLaserTurn::Beam : EBossLaserTurn::Grid;
	UE_LOG(LogTemp, Log, TEXT("[BossGimmick] 레이저 순번 → %s"), CurrentLaserTurn == EBossLaserTurn::Grid ? TEXT("격자") : TEXT("단일"));
}

bool UBossGimmickComponent::IsAnyBarrierActive() const
{
	if (IsValid(ActiveBarrier) && ActiveBarrier->IsBarrierActive())
	{
		return true;
	}

	// 보스에 붙이지 않은 배리어나 다른 페이즈 배리어가 남아 켜져 있는 경우도 막음
	for (const TPair<FName, TObjectPtr<AActor>>& Pair : PhaseActors)
	{
		const ABarrier* Barrier = Cast<ABarrier>(Pair.Value.Get());
		if (IsValid(Barrier) && Barrier->IsBarrierActive())
		{
			return true;
		}
	}
	return false;
}

bool UBossGimmickComponent::CanReceiveCounterDamage(EBossCounterDamageType Type) const
{
	const ABossCharacter* Boss = Cast<ABossCharacter>(GetOwner());
	if (!IsValid(Boss) || !Boss->IsAlive())
	{
		return false;
	}

	const UEnemyPhaseComponent* PhaseComponent = Boss->GetPhaseComponent();
	const FName PhaseId = PhaseComponent ? PhaseComponent->GetCurrentPhaseId() : NAME_None;
	const TArray<FName>& AllowedPhases = Type == EBossCounterDamageType::MagneticCoin ? CoinDamagePhaseIds : ReflectedLaserDamagePhaseIds;
	if (!AllowedPhases.Contains(PhaseId))
	{
		return false;
	}

	return !(bIgnoreWhileBarrierActive && IsAnyBarrierActive());
}

bool UBossGimmickComponent::TryApplyCounterDamage(AActor* Source, EBossCounterDamageType Type, float DamageOverride)
{
	if (!CanReceiveCounterDamage(Type))
	{
		return false;
	}

	const bool bReflectedLaser = Type == EBossCounterDamageType::ReflectedLaser;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (bReflectedLaser && ReflectedLaserDamageInterval > 0.f && LastReflectedLaserDamageTime >= 0.f
		&& Now - LastReflectedLaserDamageTime < ReflectedLaserDamageInterval)
	{
		return false;
	}

	const float Damage = DamageOverride > 0.f ? DamageOverride
		: (Type == EBossCounterDamageType::MagneticCoin ? CoinDamage : ReflectedLaserDamage);
	UAbilitySystemComponent* BossASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (Damage <= 0.f || !BossASC)
	{
		return false;
	}

	FGameplayEffectContextHandle Context = BossASC->MakeEffectContext();
	Context.AddSourceObject(Source);

	if (CounterDamageEffectClass)
	{
		// 지정한 GE: SetByCaller Data.Damage로 피해량 전달
		const FGameplayEffectSpecHandle Spec = BossASC->MakeOutgoingSpec(CounterDamageEffectClass, 1.f, Context);
		if (!Spec.IsValid())
		{
			return false;
		}
		Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), Damage);
		BossASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	else
	{
		// GE 미지정: 적 Damage 어트리뷰트에 피해량을 더하는 즉시 GE를 만들어 적용
		// (팀 적 피해 GE는 고정값이라 피해량을 넘길 수 없음 → 에셋 없이도 동작하게)
		UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient);
		DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = DamageEffect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = UEnemyAttributeSet::GetDamageAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Damage));

		const FGameplayEffectSpec Spec(DamageEffect, Context, 1.f);
		BossASC->ApplyGameplayEffectSpecToSelf(Spec);
	}

	if (bReflectedLaser)
	{
		LastReflectedLaserDamageTime = Now;
	}

	UE_LOG(LogTemp, Log, TEXT("[BossGimmick] 반격 피해 %s %.1f (출처 %s)"),
		Type == EBossCounterDamageType::MagneticCoin ? TEXT("코인") : TEXT("반사 레이저"), Damage, *GetNameSafe(Source));
	OnCounterDamaged.Broadcast(this, Source, Type, Damage);
	return true;
}

void UBossGimmickComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAll();

	Super::EndPlay(EndPlayReason);
}
