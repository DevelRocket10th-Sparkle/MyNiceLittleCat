/// 작성자: 최요환
/// 작성일자: 2026-10-22
///
/// 보스 페이즈 기믹 연결 컴포넌트 구현

#include "Gimmicks/Boss/BossGimmickComponent.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/Components/BossPhaseComponent.h"
#include "Gimmicks/Boss/Barrier.h"
#include "Gimmicks/Boss/MagneticCoinPatternComponent.h"

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
	const UBossPhaseComponent* PhaseComponent = Boss ? Boss->GetPhaseComponent() : nullptr;
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
		Barrier->ActivateBarrier();
		UE_LOG(LogTemp, Log, TEXT("[BossGimmick] %s 진입: 배리어 %s 활성"), *PhaseId.ToString(), *Barrier->GetName());
	}

	if (CoinPatternPhaseIds.Contains(PhaseId))
	{
		if (UMagneticCoinPatternComponent* CoinPattern = GetCoinPattern())
		{
			CoinPattern->SetSpawnCenterActor(AreaCenter);
			CoinPattern->StartPattern();
			UE_LOG(LogTemp, Log, TEXT("[BossGimmick] %s 진입: 자성 코인 패턴 시작"), *PhaseId.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BossGimmick] %s: UMagneticCoinPatternComponent가 없어 코인 패턴을 건너뜁니다."), *GetNameSafe(GetOwner()));
		}
	}

	return true;
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

	if (UMagneticCoinPatternComponent* CoinPattern = GetCoinPattern())
	{
		CoinPattern->StopPattern(bClearCoinsOnStop);
	}

	// 페이즈 진입·재도전 때 항상 첫 순번부터
	CurrentLaserTurn = FirstLaserTurn;
}

bool UBossGimmickComponent::IsLaserAlternationPhase() const
{
	const ABossCharacter* Boss = Cast<ABossCharacter>(GetOwner());
	const UBossPhaseComponent* PhaseComponent = Boss ? Boss->GetPhaseComponent() : nullptr;
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

UMagneticCoinPatternComponent* UBossGimmickComponent::GetCoinPattern() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMagneticCoinPatternComponent>() : nullptr;
}

void UBossGimmickComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAll();

	Super::EndPlay(EndPlayReason);
}
