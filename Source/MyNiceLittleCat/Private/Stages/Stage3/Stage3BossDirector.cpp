// Fill out your copyright notice in the Description page of Project Settings.


#include "Stages/Stage3/Stage3BossDirector.h"
#include "Cinematics/CinematicLog.h"
#include "Stages/Stage3/MansionLightingSubsystem.h"

#include "Animation/AnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "TimerManager.h"

#include "Characters/PlayerCharacter.h"
#include "Characters/PlayerMainController.h"
#include "Cinematics/CinematicTrailDecalComponent.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/BossEncounterManager.h"
#include "Enemy/Components/EnemyEyeGlowComponent.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Frameworks/Sound/BGMSubSystem.h"
#include "Gimmicks/Stage/GimmickDoor.h"
#include "Gimmicks/Stage/GimmickKeyLock.h"
#include "Pet/PetAIController.h"
#include "Pet/PetCharacter.h"

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
const FName AStage3BossDirector::EndingCutTag(TEXT("Ending"));

AStage3BossDirector::AStage3BossDirector()
{
	BloodTrailDecals = CreateDefaultSubobject<UCinematicTrailDecalComponent>(TEXT("BloodTrailDecals"));
}

void AStage3BossDirector::BeginPlay()
{
	GetWorld()->GetSubsystem<UMansionLightingSubsystem>()->InitializeForStage();
	// 베이스: 보스방에 연출 담당자로 등록
	Super::BeginPlay();

	// 엔딩이 있는데 다음 레벨이 없으면 엔딩 뒤 검은 화면에서 멈추므로 설정 오류를 미리 알림
	if (EndingSequence && !HasLevelAfterEnding())
	{
		UE_LOG(LogStage, Warning, TEXT("[Stage3] EndingSequence is set but the encounter has no LevelAfterDefeat"));
	}

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
	// 엔딩 컷: 펫 연결 (보스는 시퀀스 안의 대역이 연기하므로 실제 보스 바인딩은 쓰지 않음)
	if (CutTag == EndingCutTag)
	{
		if (APetCharacter* Pet = EndingPet.Get())
		{
			Request.Bindings.Emplace(PetBindingTag, Pet);
		}
		return;
	}

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

void AStage3BossDirector::OnBossReady_Implementation(ABossCharacter* Boss)
{
	HideBossBeforeIntro();
}

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

	// 숨겨 두었던 보스를 등장 컷부터 보이게 함
	if (Boss)
	{
		Boss->SetActorHiddenInGame(false);
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
	if (ABossCharacter* Boss = GetCurrentBoss())
	{
		OnStagePhaseStarted(Boss->GetCurrentPhaseId(), Boss);
	}

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
	// Collapse:Player → 엔딩: 플레이어가 힘이 풀리며 쓰러짐 (쓰러지는 동작은 시퀀스 애님, 여기서는 조작/몽타주만 정리)
	if (Args.IsCommand(TEXT("Collapse")) && Args.ArgEquals(0, TEXT("Player")))
	{
		if (APlayerCharacter* Player = GetAlivePlayer())
		{
			Player->BeginCinematicCollapse();
		}
		return true;
	}
	if (Args.IsCommand(TEXT("Stair")) && Args.ArgEquals(0, TEXT("Break")))
	{
		GetWorld()->GetSubsystem<UMansionLightingSubsystem>()->SetEvening(true);
		SetStairBlocked(false);
		return true;
	}

	// Blood:* 마커는 핏자국 컴포넌트가 경로 계산에 사용하므로 여기서는 처리하지 않음
	return false;
}

void AStage3BossDirector::OnCutFinished_Implementation(FName CutTag)
{
	// 엔딩: 페이드 아웃(KeepState)된 화면을 잠시 유지한 뒤 다음 레벨
	if (CutTag == EndingCutTag)
	{
		GetWorldTimerManager().SetTimer(EndingTimer, this, &AStage3BossDirector::FinishEnding,
			FMath::Max(EndingBlackHoldSeconds, KINDA_SMALL_NUMBER), false);
		return;
	}

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

	// 등장 컷 전의 컷(RestoreState)이 보스 가시성을 되돌려도 다시 숨김
	HideBossBeforeIntro();
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

	HideBossBeforeIntro();
}

void AStage3BossDirector::OnStageCleared_Implementation()
{
	// 출구 개방은 베이스가 처리 (모든 문 열기)
	GetWorldTimerManager().ClearTimer(EntranceCheckTimer);

	// 엔딩은 다음 틱에 시작: 지금은 피해 적용 → BeginDeath → OnDefeated 방송 콜스택 안이라
	// 여기서 컷을 시작하고 보스를 숨기면 남은 사망 처리와 순서가 섞임
	if (EndingSequence && !bEndingStarted)
	{
		bEndingStarted = true;
		GetWorldTimerManager().SetTimerForNextTick(this, &AStage3BossDirector::StartEnding);
	}
}

/* 엔딩 */

bool AStage3BossDirector::HoldsLevelAfterDefeat() const
{
	return EndingSequence != nullptr;
}

bool AStage3BossDirector::HasLevelAfterEnding() const
{
	return Encounter && Encounter->HasLevelAfterDefeat();
}

void AStage3BossDirector::StartEnding()
{
	APlayerCharacter* Player = GetAlivePlayer();
	if (!Player)
	{
		// 플레이어가 없으면 연출 없이 다음 레벨
		FinishEnding();
		return;
	}

	// 실제 보스는 숨기고 사망 처리(래그돌, 제거)는 그대로 진행: 시퀀스의 보스 대역이 연기
	if (ABossCharacter* Boss = GetCurrentBoss())
	{
		Boss->SetActorHiddenInGame(true);
	}

	// 펫: StateTree 이동과 능력이 시퀀스 위치를 덮어쓰지 않도록 정지
	EndingPet = FindPartnerPet();
	if (APetCharacter* Pet = EndingPet.Get())
	{
		Pet->RequestMode(EPetMode::Basic);
		Pet->CancelNavigationRecovery();
		if (APetAIController* PetAI = Pet->GetController<APetAIController>())
		{
			PetAI->StopPetLogic();
		}
	}

	// 공격·회복 등 진행 중인 몽타주가 시퀀스 애님 위에 남지 않게 정리
	if (UAnimInstance* AnimInstance = Player->GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.1f);
	}

	// 엔딩 BGM: 이벤트 스택 맨 위라 컷이 다른 BGM 존(마을)으로 넘어가도 곡이 이어짐
	if (EndingBGM)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UBGMSubSystem* BGM = GI->GetSubsystem<UBGMSubSystem>())
			{
				BGM->PushEventBGM(this, EndingBGM);
			}
		}
	}

	PlayStageCut(EndingSequence, EndingCutTag);
}

void AStage3BossDirector::FinishEnding()
{
	if (HasLevelAfterEnding())
	{
		Encounter->OpenLevelAfterDefeat();
		return;
	}

	// 다음 레벨이 없으면 엔딩의 검은 화면(KeepState 페이드)에 갇히므로 화면과 펫을 되돌림
	UE_LOG(LogStage, Error, TEXT("[Stage3] Ending finished but the encounter has no LevelAfterDefeat; restoring view"));
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StopCameraFade();
	}
	if (APetCharacter* Pet = EndingPet.Get())
	{
		if (APetAIController* PetAI = Pet->GetController<APetAIController>())
		{
			PetAI->StartPetLogic();
		}
	}
}

APetCharacter* AStage3BossDirector::FindPartnerPet() const
{
	const APlayerCharacter* Player = GetAlivePlayer();
	const APlayerMainController* PC = Player ? Player->GetController<APlayerMainController>() : nullptr;
	return PC ? PC->GetPartnerPet() : nullptr;
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
		: (Doors.IsValidIndex(GateDoorIndex) ? Doors[GateDoorIndex].Get() : nullptr);

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
	const FVector Delta = Player->GetActorLocation() - Doors[EntranceDoorIndex]->GetActorLocation();
	if (GetStageTransform().InverseTransformVector(Delta).X > EntranceCloseDistance)
	{
		bEntranceClosed = true;
		SetDoor(EntranceDoorIndex, false, EntranceCloseSeconds);
		GetWorldTimerManager().ClearTimer(EntranceCheckTimer);
	}
}

void AStage3BossDirector::HideBossBeforeIntro()
{
	const bool bBeforeIntro = OpeningStep == EOpeningStep::WaitingForArrival
		|| OpeningStep == EOpeningStep::GateLocked
		|| OpeningStep == EOpeningStep::Unlocking
		|| OpeningStep == EOpeningStep::Courtyard;

	ABossCharacter* Boss = GetCurrentBoss();
	if (bBeforeIntro && Boss && !Boss->IsHidden())
	{
		Boss->SetActorHiddenInGame(true);
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
