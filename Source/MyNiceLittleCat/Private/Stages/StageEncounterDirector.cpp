// Fill out your copyright notice in the Description page of Project Settings.


#include "Stages/StageEncounterDirector.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "TimerManager.h"

#include "Characters/PlayerCharacter.h"
#include "Cinematics/CinematicSubsystem.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/BossEncounterManager.h"
#include "Gimmicks/Stage/GimmickDoor.h"
#include "Interfaces/ActivatableInterface.h"

namespace
{
	// 아레나 진입 확인 간격
	constexpr float ArenaCheckInterval = 0.1f;

	// 문 마커에 시간이 없을 때 기본 회전 시간
	constexpr float DefaultDoorOpenSeconds = 0.85f;
	constexpr float DefaultDoorCloseSeconds = 0.18f;
}

const FName AStageEncounterDirector::IntroCutTag(TEXT("Intro"));
const FName AStageEncounterDirector::PhaseTransitionCutTag(TEXT("PhaseTransition"));

AStageEncounterDirector::AStageEncounterDirector()
{
	// 문 회전은 각 문의 Mover가, 주기 확인은 단계별 타이머가 처리하므로 Tick 불필요
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

/* 엔진 이벤트 */

void AStageEncounterDirector::BeginPlay()
{
	Super::BeginPlay();

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
		UE_LOG(LogTemp, Error, TEXT("[StageDirector] %s: Encounter is not set"), *GetName());
	}
}

void AStageEncounterDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelActiveCut();
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

/* IBossEncounterDirector */

void AStageEncounterDirector::OnBossSpawned(ABossCharacter* Boss)
{
	CurrentBoss = Boss;
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

	OnIntroStarting(GetCurrentBoss());

	const FStagePhaseStaging* Staging = FindPhaseStaging(IntroPhaseId);
	if (!Staging)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StageDirector] Intro staging '%s' not found. Starting without cut."), *IntroPhaseId.ToString());
	}

	PlayCutInternal(Staging ? Staging->Sequence.Get() : nullptr, IntroCutTag, 0.f);
	return true;
}

void AStageEncounterDirector::OnEncounterReset()
{
	CancelActiveCut();
	ClearActiveTransition();

	// 연출이 강제한 문 상태를 풀어 문 자체 상태(신호 기준, 기본 닫힘)로 즉시 복귀
	for (AGimmickDoor* Door : Doors)
	{
		if (Door)
		{
			Door->ReleaseForcedDoorState(true);
		}
	}

	OnStageReset();
}

void AStageEncounterDirector::OnEncounterCleared()
{
	// 처치 후 모든 문을 열고 연출 통제에서 돌려줌
	// - 문 자체 상태(신호)를 열림으로 바꾼 뒤 강제를 풀면, 열린 채로 이후에는 신호·플레이어 조작을 따름
	//   (예: 옥상 문은 다시 플레이어가 여닫을 수 있음)
	// - 강제한 채 두면 클리어 후에도 플레이어가 문을 조작할 수 없음
	for (AGimmickDoor* Door : Doors)
	{
		if (Door)
		{
			Door->SetActivated(true, this);
			Door->ReleaseForcedDoorState();
		}
	}

	OnStageCleared();
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
	AGimmickDoor* Door = Doors.IsValidIndex(DoorIndex) ? Doors[DoorIndex].Get() : nullptr;
	if (!Door)
	{
		return;
	}

	// 연출이 문 상태를 강제: 전투 초기화 전까지 신호·플레이어 조작보다 우선
	// - 시간 0 이하(스킵, 초기화)는 즉시 이동: 문 효과음 없음
	const bool bInstant = Duration <= 0.f;
	Door->ForceDoorState(bOpen, Duration, bInstant);
}

bool AStageEncounterDirector::IsDoorOpen(int32 DoorIndex) const
{
	const AGimmickDoor* Door = Doors.IsValidIndex(DoorIndex) ? Doors[DoorIndex].Get() : nullptr;
	return Door && Door->IsDoorOpen();
}

void AStageEncounterDirector::PlayStageCut(ULevelSequence* Sequence, FName CutTag, float TimeoutSeconds)
{
	// 공통 태그는 등장/전환 흐름 전용
	if (CutTag == IntroCutTag || CutTag == PhaseTransitionCutTag)
	{
		UE_LOG(LogTemp, Error, TEXT("[StageDirector] Cut tag '%s' is reserved"), *CutTag.ToString());
		return;
	}

	PlayCutInternal(Sequence, CutTag, TimeoutSeconds);
}

APlayerCharacter* AStageEncounterDirector::GetAlivePlayer() const
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	return Player && !Player->GetbIsDead() ? Player : nullptr;
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
		HandleDoorMarker(Args, bInstant);
		return;
	}

	if (Args.IsCommand(TEXT("Event")))
	{
		HandleEventMarker(Args);
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
	else
	{
		OnCutFinished(CutTag);
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

	// 아레나에 들어온 플레이어 뒤의 문을 닫음
	SetDoor(DoorToClose, false, PhaseDoorCloseSeconds);

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

void AStageEncounterDirector::ClearActiveTransition()
{
	GetWorldTimerManager().ClearTimer(ArenaEntryTimer);
	ActiveTransitionId = 0;
	ActiveTransitionPhaseId = NAME_None;
}

/* 공통 마커 */

// Door:<번호>:Open|Close[:초] → 연출 강제 이동
// Door:<번호>:Lock|Unlock     → 플레이어 상호작용 잠금 (스킵돼도 최종 잠금 상태가 남음)
void AStageEncounterDirector::HandleDoorMarker(const FCinematicMarkerArgs& Args, bool bInstant)
{
	int32 DoorIndex = INDEX_NONE;
	if (!Args.TryGetInt(0, DoorIndex))
	{
		return;
	}

	AGimmickDoor* Door = Doors.IsValidIndex(DoorIndex) ? Doors[DoorIndex].Get() : nullptr;
	if (!Door)
	{
		return;
	}

	// Lock|Unlock: 플레이어 상호작용만 막거나 풂 (문 위치는 그대로)
	if (Args.ArgEquals(1, TEXT("Lock")) || Args.ArgEquals(1, TEXT("Unlock")))
	{
		Door->SetLocked(Args.ArgEquals(1, TEXT("Lock")), this);
		return;
	}

	// Open|Close: 연출 강제 이동
	const bool bOpen = Args.ArgEquals(1, TEXT("Open"));
	const float Duration = Args.GetFloat(2, bOpen ? DefaultDoorOpenSeconds : DefaultDoorCloseSeconds);
	SetDoor(DoorIndex, bOpen, bInstant ? 0.f : Duration);
}

// Event:<이름>[:Off] → 등록된 액터에 기믹 신호(IActivatableInterface)로 전달
void AStageEncounterDirector::HandleEventMarker(const FCinematicMarkerArgs& Args)
{
	const FName EventName(*Args.GetArg(0));
	const bool bDeactivate = Args.ArgEquals(1, TEXT("Off"));

	AActor* Receiver = EventReceivers.FindRef(EventName);
	if (!Receiver || !Receiver->Implements<UActivatableInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[StageDirector] No IActivatableInterface receiver for cinematic event '%s'"), *EventName.ToString());
		return;
	}

	// 신호를 보낸 쪽은 이 Director
	if (bDeactivate)
	{
		IActivatableInterface::Execute_Deactivate(Receiver, this);
	}
	else
	{
		IActivatableInterface::Execute_Activate(Receiver, this);
	}
}
