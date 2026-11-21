// Fill out your copyright notice in the Description page of Project Settings.


#include "Stages/Stage3/Stage3BossDirector.h"
#include "Cinematics/CinematicLog.h"
#include "Systems/TimeOfDaySubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "TimerManager.h"

#include "Characters/PlayerCharacter.h"
#include "Cinematics/CinematicTrailDecalComponent.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/BossEncounterManager.h"
#include "Enemy/Components/EnemyEyeGlowComponent.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Gimmicks/Stage/GimmickDoor.h"
#include "Gimmicks/Stage/GimmickKeyLock.h"
#include "Stages/StageDoorControlComponent.h"

namespace
{
	// 주기 확인 간격: 해당 단계에서만 짧은 타이머로 확인
	constexpr float ArrivalCheckInterval = 0.25f;
	constexpr float EntranceCheckInterval = 0.2f;

	// 문 연출 시간
	constexpr float IntroGateCloseSeconds = 0.16f;
	constexpr float EntranceCloseSeconds = 0.18f;
}

const FName AStage3BossDirector::ArrivalCutTag(TEXT("Arrival"));
const FName AStage3BossDirector::UnlockCutTag(TEXT("Unlock"));

AStage3BossDirector::AStage3BossDirector()
{
	BloodTrailDecals = CreateDefaultSubobject<UCinematicTrailDecalComponent>(TEXT("BloodTrailDecals"));

	// 보스는 오프닝(도착 → 대문 → 마당) 동안 이미 생성되어 있으므로 등장 컷 전까지 숨김
	bHideBossUntilIntro = true;
}

void AStage3BossDirector::BeginPlay()
{
	// 베이스: 보스방에 연출 담당자로 등록, 엔딩 설정 확인
	Super::BeginPlay();

	SetStairBlocked(true);

	// 핏자국 경로를 로딩 시점에 미리 계산 → 컷 시작 시 샘플링 비용 없음
	for (const TPair<FName, FStage3PhaseStaging>& Pair : Phases)
	{
		BloodTrailDecals->PrecomputeTrail(Pair.Value.Sequence);
	}

	if (HasAuthority())
	{
		// 자물쇠 풀림 → 대문 열림 컷 (열쇠 확인, 덜컹 소리, 안내 문구는 자물쇠가 처리)
		if (IsValid(GateLock))
		{
			GateLock->OnActivationChanged.AddDynamic(this, &AStage3BossDirector::HandleGateLockActivated);
		}

		GetWorldTimerManager().SetTimer(ArrivalCheckTimer, this, &AStage3BossDirector::UpdateArrivalCheck, ArrivalCheckInterval, true);
	}
}

/* AStageEncounterDirector: 설정과 바인딩 */

const FStagePhaseStaging* AStage3BossDirector::FindPhaseStaging(FName PhaseId) const
{
	// Stage3 전용 구조체의 공통 부분을 베이스에 제공
	return Phases.Find(PhaseId);
}

void AStage3BossDirector::AddCutBindings(FName CutTag, FCinematicRequest& Request) const
{
	// 대문 열림 컷: 다른 레벨 기준으로 작업된 자물쇠 장치 바인딩을 현재 레벨의 컴포넌트로 연결
	if (CutTag != UnlockCutTag)
	{
		return;
	}

	if (UActorComponent* LockMechanism = GateLockMechanism.GetComponent(GateLock))
	{
		Request.Bindings.Emplace(GateLockBindingTag, LockMechanism);
	}
}

/* AStageEncounterDirector: 등장 */

bool AStage3BossDirector::CanBeginIntro_Implementation(APlayerCharacter* Player) const
{
	// 대문을 열고 마당에 들어온 뒤에만 등장 컷 시작
	return OpeningStep == EOpeningStep::Courtyard;
}

void AStage3BossDirector::OnIntroStarting_Implementation(ABossCharacter* Boss)
{
	OpeningStep = EOpeningStep::BossIntro;
	if (UEnemyEyeGlowComponent* EyeGlow = GetBossEyeGlow(Boss))
	{
		EyeGlow->StopWarningPulse();
		EyeGlow->SetGlowColor(FLinearColor::Black);
	}
}

void AStage3BossDirector::OnIntroFinished_Implementation()
{
	// 등장 컷이 끝나면 대문을 닫아 마당으로 돌아갈 수 없게 함
	SetDoor(GateDoorIndex, false, IntroGateCloseSeconds);

	// 시작 실패 시 베이스가 OnStageReset을 호출하므로 먼저 전투 단계로 표시
	OpeningStep = EOpeningStep::Combat;
}

void AStage3BossDirector::OnEncounterStarted_Implementation()
{
	// 첫 페이즈 눈 발광은 베이스가 OnStagePhaseStarted로 처리
	bEntranceClosed = false;
	GetWorldTimerManager().SetTimer(EntranceCheckTimer, this, &AStage3BossDirector::UpdateEntranceCheck, EntranceCheckInterval, true);
}

/* AStageEncounterDirector: 페이즈 전환 */

void AStage3BossDirector::OnPhaseStagingBegin_Implementation(FName PhaseId, ABossCharacter* Boss)
{
	const FStage3PhaseStaging* Staging = Phases.Find(PhaseId);
	if (!Staging || !Boss)
	{
		return;
	}

	// 핏자국: 지금 모두 깔고, 보스가 각 지점을 지나는 시각에 맞춰 페이드인
	const TArray<const AActor*> IgnoredActors = { Boss, GetAlivePlayer() };
	BloodTrailDecals->SpawnTrail(Staging->Sequence, GetStageTransform(), Staging->BloodTrail, IgnoredActors);

	// 눈 발광: 컷 시작 때 바꾸도록 설정한 페이즈만 (사이렌 경고는 색을 바꾼 뒤 깜빡임)
	if (Staging->bEyeGlowAtCutStart || Staging->bSirenWarning)
	{
		ApplyPhaseEyeGlow(Boss, *Staging, true);
		if (UEnemyEyeGlowComponent* EyeGlow = GetBossEyeGlow(Boss); EyeGlow && Staging->bSirenWarning)
		{
			EyeGlow->PlayWarningPulse(Staging->EyeGlow, SirenSeconds);
		}
	}
}

void AStage3BossDirector::OnPhaseStagingFinished_Implementation(FName PhaseId, ABossCharacter* Boss)
{
	// 정상 종료/스킵 모두 아직 안 보인 핏자국을 즉시 표시
	BloodTrailDecals->RevealAll();

	if (const FStage3PhaseStaging* Staging = Phases.Find(PhaseId))
	{
		ApplyPhaseEyeGlow(Boss, *Staging, true);
	}
}

void AStage3BossDirector::OnStagePhaseStarted_Implementation(FName PhaseId, ABossCharacter* Boss)
{
	if (const FStage3PhaseStaging* Staging = Phases.Find(PhaseId))
	{
		ApplyPhaseEyeGlow(Boss, *Staging, false);
	}
}

/* AStageEncounterDirector: 마커와 컷 */

bool AStage3BossDirector::HandleStageMarker_Implementation(const FCinematicMarker& Marker)
{
	// Stair:Break → 계단 막이 제거
	const FCinematicMarkerArgs Args(Marker.Label);
	if (Args.IsCommand(TEXT("EyeGlow")))
	{
		if (const FStage3PhaseStaging* Staging = Phases.Find(FName(*Args.GetArg(0))))
		{
			ApplyPhaseEyeGlow(GetCurrentBoss(), *Staging, true);
		}
		return true;
	}
	if (Args.IsCommand(TEXT("Stair")) && Args.ArgEquals(0, TEXT("Break")))
	{
		// 계단이 부서지면 시간대 전환 (메인 맵의 시간대 컨트롤러가 적용)
		if (UTimeOfDaySubsystem* TimeOfDay = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>(); TimeOfDay && StairBreakTimeOfDay.IsValid())
		{
			TimeOfDay->SetTimeOfDay(StairBreakTimeOfDay);
		}
		SetStairBlocked(false);
		return true;
	}

	// Blood:* 마커는 핏자국 컴포넌트가 경로 계산에 사용하므로 여기서는 처리하지 않음
	return false;
}

void AStage3BossDirector::OnCutFinished_Implementation(FName CutTag)
{
	if (CutTag == ArrivalCutTag)
	{
		CompleteArrival();
	}
	else if (CutTag == UnlockCutTag)
	{
		// 마커가 없거나 컷 재생에 실패해도 대문은 열린 상태로 진행
		SetDoor(GateDoorIndex, true, 0.f);
		OpeningStep = EOpeningStep::Courtyard;
	}
}

/* AStageEncounterDirector: 초기화와 처치 */

void AStage3BossDirector::OnStageReset_Implementation()
{
	GetWorldTimerManager().ClearTimer(EntranceCheckTimer);

	// 베이스가 문 강제를 푼 뒤, 자물쇠를 연 경우 대문만 다시 열어 둠 (풀린 상태는 자물쇠에 남아 있음)
	if (IsGateUnlocked())
	{
		SetDoor(GateDoorIndex, true, 0.f);
	}

	BloodTrailDecals->ClearTrail();

	if (UEnemyEyeGlowComponent* EyeGlow = GetBossEyeGlow(GetCurrentBoss()))
	{
		EyeGlow->StopWarningPulse();
	}

	bEntranceClosed = false;
	SetStairBlocked(true);

	// 도착 전이 아니면 마당(또는 잠긴 대문)으로 되돌림 → 재도전 시 등장 컷부터 다시 시작
	if (OpeningStep != EOpeningStep::WaitingForArrival)
	{
		OpeningStep = IsGateUnlocked() ? EOpeningStep::Courtyard : EOpeningStep::GateLocked;
	}
}

void AStage3BossDirector::OnStageCleared_Implementation()
{
	// 출구 개방과 엔딩 컷은 베이스가 처리
	GetWorldTimerManager().ClearTimer(EntranceCheckTimer);
}

/* 오프닝 */

void AStage3BossDirector::UpdateArrivalCheck()
{
	if (OpeningStep != EOpeningStep::WaitingForArrival)
	{
		GetWorldTimerManager().ClearTimer(ArrivalCheckTimer);
		return;
	}

	const APlayerCharacter* Player = GetAlivePlayer();
	if (!Player || IsCutPlaying())
	{
		return;
	}

	// 이미 본 도착 컷이거나 컷이 없으면 바로 다음 단계
	const UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	if (!ArrivalSequence || (Progress && Progress->HasProgressFlag(ArrivalSeenProgressFlag)))
	{
		GetWorldTimerManager().ClearTimer(ArrivalCheckTimer);
		CompleteArrival();
		return;
	}

	// 대문 자물쇠(없으면 대문 액터) 근처에 오면 도착 컷
	const AActor* Anchor = IsValid(GateLock) ? static_cast<const AActor*>(GateLock.Get())
		: static_cast<const AActor*>(GetDoorControl()->GetDoor(GateDoorIndex));

	if (Anchor && FVector::DistSquared(Player->GetActorLocation(), Anchor->GetActorLocation()) <= FMath::Square(ArrivalTriggerRadius))
	{
		GetWorldTimerManager().ClearTimer(ArrivalCheckTimer);
		PlayStageCut(ArrivalSequence, ArrivalCutTag);
	}
}

void AStage3BossDirector::CompleteArrival()
{
	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		Progress->SetProgressFlag(ArrivalSeenProgressFlag);
	}

	OpeningStep = EOpeningStep::GateLocked;

	// 도착 컷 전에 이미 자물쇠가 풀렸으면(자물쇠가 없는 경우 포함) 바로 대문 열림 컷
	if (IsGateUnlocked())
	{
		StartUnlockCut();
	}
}

void AStage3BossDirector::HandleGateLockActivated(AGimmickActorBase* Gimmick, bool bActivated, AActor* ChangeInstigator)
{
	// 도착 컷이 끝나 대문 앞 단계일 때만 열림 컷 (그 전에 풀리면 CompleteArrival에서 처리)
	if (bActivated && OpeningStep == EOpeningStep::GateLocked)
	{
		StartUnlockCut();
	}
}

void AStage3BossDirector::StartUnlockCut()
{
	OpeningStep = EOpeningStep::Unlocking;
	PlayStageCut(UnlockSequence, UnlockCutTag);
}

bool AStage3BossDirector::IsGateUnlocked() const
{
	// 자물쇠가 지정되지 않았으면 잠기지 않은 것으로 봄
	return !IsValid(GateLock) || GateLock->IsUnlocked();
}

void AStage3BossDirector::UpdateEntranceCheck()
{
	if (OpeningStep != EOpeningStep::Combat || bEntranceClosed)
	{
		GetWorldTimerManager().ClearTimer(EntranceCheckTimer);
		return;
	}

	// 입구 문이 열린 뒤(Barrier70 컷)에만 확인
	const APlayerCharacter* Player = GetAlivePlayer();
	if (!Player || !IsDoorOpen(EntranceDoorIndex))
	{
		return;
	}

	// 문 기준 스테이지 로컬 X로 통과 판정 (레벨 인스턴스 회전과 무관)
	const FVector Delta = Player->GetActorLocation() - GetDoorControl()->GetDoor(EntranceDoorIndex)->GetActorLocation();
	if (GetStageTransform().InverseTransformVector(Delta).X > EntranceCloseDistance)
	{
		bEntranceClosed = true;
		SetDoor(EntranceDoorIndex, false, EntranceCloseSeconds);
		GetWorldTimerManager().ClearTimer(EntranceCheckTimer);
	}
}

/* 레벨 상태 */

void AStage3BossDirector::SetStairBlocked(bool bBlocked)
{
	if (IsValid(StairBlocker))
	{
		StairBlocker->SetActorHiddenInGame(!bBlocked);
		StairBlocker->SetActorEnableCollision(bBlocked);
	}
}

void AStage3BossDirector::ApplyPhaseEyeGlow(const ABossCharacter* Boss, const FStage3PhaseStaging& Staging, bool bStopWarningPulse) const
{
	UEnemyEyeGlowComponent* EyeGlow = GetBossEyeGlow(Boss);
	if (!EyeGlow)
	{
		return;
	}

	if (bStopWarningPulse)
	{
		EyeGlow->StopWarningPulse();
	}
	EyeGlow->SetGlowColor(Staging.EyeGlow);
}

UEnemyEyeGlowComponent* AStage3BossDirector::GetBossEyeGlow(const ABossCharacter* Boss) const
{
	return Boss ? Boss->FindComponentByClass<UEnemyEyeGlowComponent>() : nullptr;
}
