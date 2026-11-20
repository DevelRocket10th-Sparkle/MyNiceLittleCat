// Fill out your copyright notice in the Description page of Project Settings.


#include "Stages/StageEncounterDirector.h"
#include "Cinematics/CinematicLog.h"

#include "Animation/AnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "TimerManager.h"

#include "Characters/PlayerCharacter.h"
#include "Characters/PlayerMainController.h"
#include "Frameworks/PlayerLookup.h"
#include "Cinematics/CinematicSubsystem.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/BossEncounterManager.h"
#include "Frameworks/Sound/BGMSubSystem.h"
#include "Gimmicks/Stage/GimmickDoor.h"
#include "Pet/PetAIController.h"
#include "Pet/PetCharacter.h"
#include "Stages/StageDoorControlComponent.h"

namespace
{
	// 아레나 진입 확인 간격
	constexpr float ArenaCheckInterval = 0.1f;
}

const FName AStageEncounterDirector::IntroCutTag(TEXT("Intro"));
const FName AStageEncounterDirector::PhaseTransitionCutTag(TEXT("PhaseTransition"));
const FName AStageEncounterDirector::EndingCutTag(TEXT("Ending"));

AStageEncounterDirector::AStageEncounterDirector()
{
	// 문 회전은 각 문의 Mover가, 주기 확인은 단계별 타이머가 처리하므로 Tick 불필요
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DoorControl = CreateDefaultSubobject<UStageDoorControlComponent>(TEXT("DoorControl"));
	MarkerEffects = CreateDefaultSubobject<UCinematicMarkerEffectsComponent>(TEXT("MarkerEffects"));
}

void AStageEncounterDirector::PostLoad()
{
	Super::PostLoad();

	// 2026.11.20. 문·이벤트 대상·큐 설정을 컴포넌트로 옮김: 예전 레벨의 값을 한 번 옮기고 비움 (기본값과 같은 값은 저장되지 않아 옮길 필요 없음)
	if (!Doors.IsEmpty() && DoorControl->Doors.IsEmpty())
	{
		DoorControl->Doors = MoveTemp(Doors);
	}
	Doors.Reset();

	if (!FMath::IsNearlyEqual(PhaseDoorCloseSeconds, 0.18f))
	{
		DoorControl->PhaseDoorCloseSeconds = PhaseDoorCloseSeconds;
		PhaseDoorCloseSeconds = 0.18f;
	}
	if (!bPlayerControlsDoorsAfterClear)
	{
		DoorControl->bPlayerControlsDoorsAfterClear = false;
		bPlayerControlsDoorsAfterClear = true;
	}

	if (!EventReceivers.IsEmpty() && MarkerEffects->EventReceivers.IsEmpty())
	{
		MarkerEffects->EventReceivers = MoveTemp(EventReceivers);
	}
	EventReceivers.Reset();

	if (!CinematicCues.IsEmpty() && MarkerEffects->CinematicCues.IsEmpty())
	{
		MarkerEffects->CinematicCues = MoveTemp(CinematicCues);
	}
	CinematicCues.Reset();
}

/* 엔진 이벤트 */

void AStageEncounterDirector::BeginPlay()
{
	Super::BeginPlay();

	// 엔딩이 있는데 다음 레벨이 없으면 엔딩 뒤 검은 화면에서 멈추므로 설정 오류를 미리 알림
	if (EndingSequence && !HasLevelAfterEnding())
	{
		UE_LOG(LogStage, Warning, TEXT("[StageDirector] %s: EndingSequence is set but the encounter has no LevelAfterDefeat"), *GetName());
	}

	if (!HasAuthority())
	{
		return;
	}

	// 보스방에 연출 담당자로 등록: 이미 생성된 보스가 있으면 OnBossSpawned가 바로 호출됨
	if (IsValid(Encounter))
	{
		Encounter->SetEncounterDirector(this);
	}
	else
	{
		UE_LOG(LogStage, Error, TEXT("[StageDirector] %s: Encounter is not set"), *GetName());
	}
}

void AStageEncounterDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelActiveCut();
	MarkerEffects->StopAll();
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

/* IBossEncounterDirector */

void AStageEncounterDirector::OnBossSpawned(ABossCharacter* Boss)
{
	CurrentBoss = Boss;
	HideBossUntilIntro();
	OnBossReady(Boss);
}

bool AStageEncounterDirector::TryBeginEncounterIntro(ABossEncounterManager& Manager, APlayerCharacter* Player)
{
	// 담당 보스방, 다른 컷 없음, 살아 있는 플레이어, 스테이지가 허락한 단계일 때만
	if (!HasAuthority() || &Manager != Encounter || IsCutPlaying()
		|| !Player || Player != GetAlivePlayer() || !CanBeginIntro(Player))
	{
		return false;
	}

	// 숨겨 두었던 보스를 등장 컷부터 보이게 함
	bIntroStarted = true;
	ABossCharacter* Boss = GetCurrentBoss();
	if (bHideBossUntilIntro && Boss)
	{
		Boss->SetActorHiddenInGame(false);
	}

	OnIntroStarting(Boss);

	const FStagePhaseStaging* Staging = FindPhaseStaging(IntroPhaseId);
	if (!Staging)
	{
		UE_LOG(LogStage, Warning, TEXT("[StageDirector] Intro staging '%s' not found. Starting without cut."), *IntroPhaseId.ToString());
	}

	PlayCutInternal(Staging ? Staging->Sequence.Get() : nullptr, IntroCutTag, 0.f);
	return true;
}

void AStageEncounterDirector::OnEncounterReset()
{
	CancelActiveCut();
	ClearActiveTransition();
	MarkerEffects->StopAll();

	// 넘긴 문 복원, 연출 강제 해제
	DoorControl->RestoreForReset();

	OnStageReset();

	// 재도전: 등장 컷부터 다시 (그 전까지 보스 숨김)
	bIntroStarted = false;
	HideBossUntilIntro();
}

void AStageEncounterDirector::OnEncounterCleared()
{
	MarkerEffects->StopAll();

	// 모든 문을 열고 연출 통제에서 돌려줌
	DoorControl->ReleaseForClear();

	OnStageCleared();

	// 엔딩은 다음 틱에 시작: 지금은 피해 적용 → BeginDeath → OnDefeated 방송 콜스택 안이라
	// 여기서 컷을 시작하고 보스를 숨기면 남은 사망 처리와 순서가 섞임
	if (EndingSequence && !bEndingStarted)
	{
		bEndingStarted = true;
		GetWorldTimerManager().SetTimerForNextTick(this, &AStageEncounterDirector::StartEnding);
	}
}

/* IBossPhaseTransitionHandler */

bool AStageEncounterDirector::BeginPhaseTransition(const FBossPhaseTransitionContext& Context)
{
	// 이 스테이지가 연출을 갖지 않은 페이즈는 보스의 기본 처리에 맡김
	const FStagePhaseStaging* Staging = FindPhaseStaging(Context.ToPhaseId);
	if (!HasAuthority() || !Staging || !Context.Boss)
	{
		return false;
	}

	CancelActiveCut();
	ClearActiveTransition();

	CurrentBoss = Context.Boss;
	ActiveTransitionId = Context.TransitionId;
	ActiveTransitionPhaseId = Context.ToPhaseId;
	ActiveTransitionFromPhaseId = Context.FromPhaseId;

	OnPhaseStagingBegin(Context.ToPhaseId, Context.Boss);

	// 재생할 수 없으면 바로 FinishPhaseStaging 실행
	PlayCutInternal(Staging->Sequence, PhaseTransitionCutTag, Context.TimeoutSeconds);
	return true;
}

void AStageEncounterDirector::CancelPhaseTransition(int64 TransitionId)
{
	if (TransitionId == 0 || TransitionId != ActiveTransitionId)
	{
		return;
	}

	if (ActiveCutTag == PhaseTransitionCutTag)
	{
		CancelActiveCut();
	}

	ClearActiveTransition();
}

/* 도우미 */

void AStageEncounterDirector::SetDoor(int32 DoorIndex, bool bOpen, float Duration)
{
	DoorControl->SetDoor(DoorIndex, bOpen, Duration);
}

void AStageEncounterDirector::FreeDoorForPlayer(int32 DoorIndex)
{
	DoorControl->FreeDoorForPlayer(DoorIndex);
}

bool AStageEncounterDirector::IsDoorOpen(int32 DoorIndex) const
{
	return DoorControl->IsDoorOpen(DoorIndex);
}

void AStageEncounterDirector::PlayStageCut(ULevelSequence* Sequence, FName CutTag, float TimeoutSeconds)
{
	// 공통 태그는 등장/전환/엔딩 흐름 전용
	if (CutTag == IntroCutTag || CutTag == PhaseTransitionCutTag || CutTag == EndingCutTag)
	{
		UE_LOG(LogStage, Error, TEXT("[StageDirector] Cut tag '%s' is reserved"), *CutTag.ToString());
		return;
	}

	PlayCutInternal(Sequence, CutTag, TimeoutSeconds);
}

APlayerCharacter* AStageEncounterDirector::GetAlivePlayer() const
{
	return PlayerLookup::GetAlivePlayer(this);
}

ABossCharacter* AStageEncounterDirector::GetCurrentBoss() const
{
	return CurrentBoss.Get();
}

FTransform AStageEncounterDirector::GetStageTransform() const
{
	return UCinematicSubsystem::GetLevelTransform(*this);
}

const FStagePhaseStaging* AStageEncounterDirector::FindPhaseStaging(FName PhaseId) const
{
	return PhaseStagings.Find(PhaseId);
}

/* 컷 */

void AStageEncounterDirector::PlayCutInternal(ULevelSequence* Sequence, FName CutTag, float TimeoutSeconds)
{
	CancelActiveCut();

	UCinematicSubsystem* Cinematics = GetWorld()->GetSubsystem<UCinematicSubsystem>();
	if (!Sequence || !Cinematics)
	{
		// 컷이 없으면 연출 없이 다음 단계로 진행
		CompleteCut(CutTag);
		return;
	}

	FCinematicRequest Request;
	Request.Sequence = Sequence;
	Request.bCutscene = true;
	Request.bSkippable = true;
	Request.TimeoutSeconds = TimeoutSeconds;

	// 섹션별 완료 모드 사용: 플레이어 위치 트랙만 KeepState → 컷이 옮긴 위치에서 플레이 재개
	Request.EndState = ECinematicEndState::UseSectionSettings;

	// 시퀀스 키는 레벨 인스턴스 로컬 좌표로 작업 → 현재 배치 위치로 이동
	Request.bUseTransformOrigin = true;
	Request.TransformOrigin = GetStageTransform();

	// 동적으로 생성한 보스와 플레이어를 태그로 연결
	if (ABossCharacter* Boss = GetCurrentBoss())
	{
		Request.Bindings.Emplace(Boss->GetSequenceBindingTag(), Boss);
	}
	if (APlayerCharacter* Player = GetAlivePlayer())
	{
		Request.Bindings.Emplace(PlayerBindingTag, Player);
	}

	// 엔딩 컷: 펫 연결 (엔딩 시작 때 AI를 멈춘 펫)
	if (CutTag == EndingCutTag)
	{
		if (APetCharacter* Pet = EndingPet.Get())
		{
			Request.Bindings.Emplace(PetBindingTag, Pet);
		}
	}

	// 스테이지 전용 바인딩
	AddCutBindings(CutTag, Request);

	ActiveCutTag = CutTag;
	ActiveCut = Cinematics->Play(Request,
		FOnCinematicMarker::CreateUObject(this, &AStageEncounterDirector::HandleCutMarker),
		FOnCinematicEnded::CreateUObject(this, &AStageEncounterDirector::HandleCutEnded));

	if (!ActiveCut.IsValid())
	{
		ActiveCutTag = NAME_None;
		CompleteCut(CutTag);
	}
}

void AStageEncounterDirector::CancelActiveCut()
{
	if (!ActiveCut.IsValid())
	{
		return;
	}

	const FCinematicHandle Cut = ActiveCut;
	ActiveCut.Reset();
	ActiveCutTag = NAME_None;
	MarkerEffects->StopAll();

	if (UCinematicSubsystem* Cinematics = GetWorld() ? GetWorld()->GetSubsystem<UCinematicSubsystem>() : nullptr)
	{
		Cinematics->Cancel(Cut);
	}
}

void AStageEncounterDirector::HandleCutMarker(FCinematicHandle Handle, const FCinematicMarker& Marker)
{
	if (Handle != ActiveCut)
	{
		return;
	}

	const FCinematicMarkerArgs Args(Marker.Label);

	// 스킵으로 한꺼번에 전달된 마커는 애니메이션 없이 최종 상태만 적용
	const bool bInstant = !Marker.bReachedByPlayback;

	if (Args.IsCommand(TEXT("Door")))
	{
		DoorControl->HandleDoorMarker(Args, bInstant);
		return;
	}

	// Event·Cue: 기준 위치(스테이지 좌표, 보스, 플레이어)를 넘겨 처리
	FCinematicCueAnchors Anchors;
	Anchors.StageTransform = GetStageTransform();
	if (ABossCharacter* Boss = GetCurrentBoss())
	{
		Anchors.BossMesh = Boss->GetMesh();
	}
	APlayerCharacter* Player = GetAlivePlayer();
	if (Player)
	{
		Anchors.PlayerMesh = Player->GetMesh();
	}
	if (MarkerEffects->HandleMarker(Marker, Anchors))
	{
		return;
	}

	// Collapse:Player → 엔딩 등: 플레이어가 힘이 풀리며 쓰러짐 (쓰러지는 동작은 시퀀스 애님, 여기서는 조작/몽타주만 정리)
	if (Args.IsCommand(TEXT("Collapse")) && Args.ArgEquals(0, TEXT("Player")))
	{
		if (Player)
		{
			Player->BeginCinematicCollapse();
		}
		return;
	}

	// 공통 마커가 아니면 스테이지에 맡김 (처리하지 않은 마커는 무시: 데칼 구간 마커 등)
	HandleStageMarker(Marker);
}

void AStageEncounterDirector::HandleCutEnded(FCinematicHandle Handle, ECinematicEndReason Reason)
{
	if (Handle != ActiveCut)
	{
		return;
	}

	const FName CutTag = ActiveCutTag;
	ActiveCut.Reset();
	ActiveCutTag = NAME_None;

	if (Reason != ECinematicEndReason::Finished)
	{
		MarkerEffects->StopAll();
	}
	CompleteCut(CutTag);
}

void AStageEncounterDirector::CompleteCut(FName CutTag)
{
	if (CutTag == IntroCutTag)
	{
		CompleteIntro();
	}
	else if (CutTag == PhaseTransitionCutTag)
	{
		FinishPhaseStaging();
	}
	else if (CutTag == EndingCutTag)
	{
		// 페이드 아웃(KeepState)된 화면을 잠시 유지한 뒤 다음 레벨
		GetWorldTimerManager().SetTimer(EndingTimer, this, &AStageEncounterDirector::FinishEnding,
			FMath::Max(EndingBlackHoldSeconds, KINDA_SMALL_NUMBER), false);
	}
	else
	{
		OnCutFinished(CutTag);

		// 등장 컷 전의 컷(RestoreState)이 보스 가시성을 되돌려도 다시 숨김
		HideBossUntilIntro();
	}
}

/* 등장과 페이즈 전환 */

void AStageEncounterDirector::CompleteIntro()
{
	// 전투 시작 직전 스테이지 처리 (문 닫기 등)
	OnIntroFinished();

	// 시작에 실패하면 매니저가 보스방을 초기화하고 OnEncounterReset을 호출
	APlayerCharacter* Player = GetAlivePlayer();
	if (!Player || !IsValid(Encounter))
	{
		OnEncounterReset();
		return;
	}

	if (Encounter->StartEncounter(Player))
	{
		// 첫 페이즈 시작: 전환 컷을 거치지 않으므로 여기서 페이즈 시작 처리 (눈 발광 등)
		if (ABossCharacter* Boss = GetCurrentBoss())
		{
			OnStagePhaseStarted(Boss->GetCurrentPhaseId(), Boss);
		}

		OnEncounterStarted();
		return;
	}

	// 매니저가 이미 초기화했더라도 한 번 더 복원해도 안전 (모든 복원은 같은 상태로 수렴)
	OnEncounterReset();
}

void AStageEncounterDirector::FinishPhaseStaging()
{
	ABossCharacter* Boss = GetCurrentBoss();
	const FStagePhaseStaging* Staging = FindPhaseStaging(ActiveTransitionPhaseId);

	// 컷이 보스를 원래 위치로 되돌리므로(RestoreState) 다음 페이즈 시작 위치로 이동
	if (Boss && Staging && Staging->BossStart)
	{
		Boss->SetActorLocationAndRotation(Staging->BossStart->GetActorLocation(), Staging->BossStart->GetActorRotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
	}

	OnPhaseStagingFinished(ActiveTransitionPhaseId, Boss);

	// 끝난 페이즈를 치른 방의 문을 플레이어에게 넘김
	// - 전환 컷이 끝나 플레이어가 다음 아레나로 걸어가는 동안부터 여닫을 수 있음
	//   (아레나 진입 후에 넘기면 플레이어는 이미 새 아레나에 갇혀 있어 처치 전까지 쓸 수 없음)
	// - 다음 아레나의 닫을 문은 넘기지 않음 (진입 때 다시 강제로 닫힘)
	if (const FStagePhaseStaging* EndedStaging = FindPhaseStaging(ActiveTransitionFromPhaseId))
	{
		const int32 NextDoorToClose = Staging ? Staging->DoorToClose : INDEX_NONE;
		for (const int32 DoorIndex : EndedStaging->DoorsFreedOnPhaseEnd)
		{
			if (DoorIndex != NextDoorToClose)
			{
				DoorControl->FreeDoorForPlayer(DoorIndex);
			}
		}
	}

	// 플레이어가 아레나에 들어올 때까지 대기 후 다음 페이즈 시작
	UpdateArenaEntryCheck();
	if (ActiveTransitionId != 0)
	{
		GetWorldTimerManager().SetTimer(ArenaEntryTimer, this, &AStageEncounterDirector::UpdateArenaEntryCheck, ArenaCheckInterval, true);
	}
}

void AStageEncounterDirector::UpdateArenaEntryCheck()
{
	const FStagePhaseStaging* Staging = FindPhaseStaging(ActiveTransitionPhaseId);
	if (ActiveTransitionId == 0 || !Staging)
	{
		GetWorldTimerManager().ClearTimer(ArenaEntryTimer);
		return;
	}

	if (!HasPlayerEnteredArena(*Staging))
	{
		return;
	}

	// 통지 중 다른 전환이 시작될 수 있으므로 상태를 먼저 비운 뒤 통지
	const int64 TransitionId = ActiveTransitionId;
	const FName PhaseId = ActiveTransitionPhaseId;
	const int32 DoorToClose = Staging->DoorToClose;
	ClearActiveTransition();

	ABossCharacter* Boss = GetCurrentBoss();
	if (!Boss || !Boss->NotifyPhaseTransitionPresentationFinished(TransitionId))
	{
		return;
	}

	// 아레나에 들어온 플레이어 뒤의 문을 닫음 (넘겨준 문이었어도 다시 연출이 강제)
	DoorControl->CloseForPhase(DoorToClose);

	OnStagePhaseStarted(PhaseId, Boss);
}

bool AStageEncounterDirector::HasPlayerEnteredArena(const FStagePhaseStaging& Staging) const
{
	// 진입 영역이 없으면 대기하지 않음
	if (Staging.EntryExtent.IsNearlyZero())
	{
		return true;
	}

	const APlayerCharacter* Player = GetAlivePlayer();
	if (!Player)
	{
		return false;
	}

	// 캡슐 반경까지 포함해 수평으로 완전히 들어왔을 때만 진입으로 판정
	const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector StagePosition = GetStageTransform().InverseTransformPosition(Player->GetActorLocation());
	const FVector Delta = (StagePosition - Staging.EntryCenter).GetAbs();

	return Delta.X + Radius < Staging.EntryExtent.X
		&& Delta.Y + Radius < Staging.EntryExtent.Y
		&& Delta.Z < Staging.EntryExtent.Z;
}

bool AStageEncounterDirector::IsWaitingForPlayer(int64 TransitionId) const
{
	// 전환 컷이 끝나 아레나 진입 확인 타이머가 도는 동안
	return TransitionId != 0 && TransitionId == ActiveTransitionId
		&& GetWorldTimerManager().IsTimerActive(ArenaEntryTimer);
}

void AStageEncounterDirector::ClearActiveTransition()
{
	GetWorldTimerManager().ClearTimer(ArenaEntryTimer);
	ActiveTransitionId = 0;
	ActiveTransitionPhaseId = NAME_None;
	ActiveTransitionFromPhaseId = NAME_None;
}

void AStageEncounterDirector::HideBossUntilIntro()
{
	ABossCharacter* Boss = GetCurrentBoss();
	if (bHideBossUntilIntro && !bIntroStarted && Boss && !Boss->IsHidden())
	{
		Boss->SetActorHiddenInGame(true);
	}
}

/* 엔딩 */

bool AStageEncounterDirector::HoldsLevelAfterDefeat() const
{
	return EndingSequence != nullptr;
}

bool AStageEncounterDirector::HasLevelAfterEnding() const
{
	return Encounter && Encounter->HasLevelAfterDefeat();
}

void AStageEncounterDirector::StartEnding()
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

	// 엔딩 BGM: 이벤트 스택 맨 위라 컷이 다른 BGM 존으로 넘어가도 곡이 이어짐
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

	PlayCutInternal(EndingSequence, EndingCutTag, 0.f);
}

void AStageEncounterDirector::FinishEnding()
{
	if (HasLevelAfterEnding())
	{
		Encounter->OpenLevelAfterDefeat();
		return;
	}

	// 다음 레벨이 없으면 엔딩의 검은 화면(KeepState 페이드)에 갇히므로 화면과 펫을 되돌림
	UE_LOG(LogStage, Error, TEXT("[StageDirector] %s: ending finished but the encounter has no LevelAfterDefeat; restoring view"), *GetName());
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

APetCharacter* AStageEncounterDirector::FindPartnerPet() const
{
	// 플레이어가 죽어 있는 동안(리스폰 대기)은 펫 연출도 하지 않음
	return GetAlivePlayer() ? PlayerLookup::GetPartnerPet(this) : nullptr;
}
