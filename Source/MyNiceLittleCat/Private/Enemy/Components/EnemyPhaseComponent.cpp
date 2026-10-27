// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Components/EnemyPhaseComponent.h"
#include "AbilitySystemComponent.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Engine/World.h"
#include "TimerManager.h"

// 컴포넌트의 기본 실행 상태를 설정한다.
UEnemyPhaseComponent::UEnemyPhaseComponent()
{
	// 전환 시퀀스를 감시할 때만 프레임 갱신을 활성화한다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// ...
}


// 게임 시작 시 초기화한다.
// 컴포넌트 초기화 후 실행 준비
//void UEnemyPhaseComponent::BeginPlay()
//{
//	Super::BeginPlay();
//
//	// ...
//
//}


// 진행 중인 전환 시퀀스의 상태를 확인한다.
void UEnemyPhaseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 플레이어 또는 액터 제거와 비정상 재생 종료도 다음 페이즈 진행으로 처리
	if (IsTransitioning() && TransitionResult == EEnemyPatternResult::Running && SequenceTransitionId == ActiveTransitionId
		&& (!IsValid(TransitionSequencePlayer) || !IsValid(TransitionSequenceActor)
			|| (!TransitionSequencePlayer->IsPlaying() && !TransitionSequencePlayer->IsPaused())))
		FinishTransitionSequence(TEXT("Playback unavailable"));
}


bool UEnemyPhaseComponent::InitializePhases(UAbilitySystemComponent* InASC, UBossPhaseConfigData* InConfig)
{
	StopAndInvalidate();
	ASC = InASC;
	Config = InConfig;
	CurrentPhaseIndex = INDEX_NONE;
	// 설정 오류를 로그에 표시하기 위한 설명
	FString Error;
	if (!GetOwner()->HasAuthority() || !InASC || !InConfig || !InConfig->ValidateConfiguration(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Phase initialization failed: %s"), *Error);
		return false;
	}
	return true;
}

// 첫 페이즈 설정 적용 후 체력 조건 평가 허용
bool UEnemyPhaseComponent::StartPhases()
{
	if (!GetOwner()->HasAuthority() || !Config || !ASC.IsValid() || !ApplyPhase.IsBound()
		|| CurrentPhaseIndex != INDEX_NONE) return false;
	CurrentPhaseIndex = 0;
	// 설정 적용 중에는 피해가 허용되지 않도록 Active 전환을 나중에 수행
	RunState = EBossPhaseRunState::Transitioning;
	if (!ApplyCurrentPhase())
	{
		StopAndInvalidate();
		return false;
	}
	RunState = EBossPhaseRunState::Active;
	if (bPendingGimmickCompletion)
	{
		RequestNextPhase();
	}
	return true;
}

const FBossPhaseDefinition* UEnemyPhaseComponent::GetCurrentPhase() const
{
	return Config && Config->Phases.IsValidIndex(CurrentPhaseIndex) ? &Config->Phases[CurrentPhaseIndex] : nullptr;
}

FName UEnemyPhaseComponent::GetCurrentPhaseId() const
{
	// 초기화 전에는 유효한 페이즈가 없으므로 None 반환
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	return Phase ? Phase->PhaseId : NAME_None;
}

// 현재 전투 페이즈에서 허용하는 최소 체력 반환
float UEnemyPhaseComponent::GetDamageHealthFloor(float MaxHealth) const
{
	// 배리어에서는 체력 하한 대신 BossCharacter의 피해 차단을 사용
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	if (!Phase || IsCurrentPhaseLogOnly())
	{
		return 0.f;
	}
	// 배리어는 자체 비율 대신 직전 전투의 체력 하한을 유지하여 피해로 진행되지 않게 함
	if (Phase->Kind == EBossPhaseKind::Barrier)
	{
		for (int32 Index = CurrentPhaseIndex - 1; Index >= 0; --Index)
		{
			const FBossPhaseDefinition& PreviousPhase = Config->Phases[Index];
			if (!Config->IsLogOnlyPhase(Index) && PreviousPhase.Kind == EBossPhaseKind::Combat)
				return PreviousPhase.HealthFloorRatio * MaxHealth;
		}
		return 0.f;
	}
	return Phase->HealthFloorRatio * MaxHealth;
}

// 현재 전투 페이즈의 하한 체력 도달 여부 확인
void UEnemyPhaseComponent::EvaluateHealth(float Health, float MaxHealth)
{
	// 현재 활성 전투 페이즈에 대해서만 체력 조건 평가
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	if (IsActive() && Phase && !IsCurrentPhaseLogOnly() && Phase->Kind == EBossPhaseKind::Combat
		&& Phase->HealthFloorRatio > 0.f && Health > 0.f && MaxHealth > 0.f
		&& Health <= MaxHealth * Phase->HealthFloorRatio) RequestNextPhase();
}



// 다음 페이즈 전환을 중복 없이 요청하고 즉시 보호 처리 통지
void UEnemyPhaseComponent::RequestNextPhase()
{
	if (!IsActive() || !Config || !Config->Phases.IsValidIndex(CurrentPhaseIndex + 1))
	{
		return;
	}
	ActivePhaseExecutionId = 0;
	bPendingGimmickCompletion = false;
	PendingPhaseIndex = CurrentPhaseIndex + 1;
	ActiveTransitionId = ++NextTransitionId;
	TransitionResult = EEnemyPatternResult::Running;
	RunState = EBossPhaseRunState::TransitionPending;
	OnTransitionRequested.Broadcast();
}

// 대기 중인 요청의 전환 GA 시작
bool UEnemyPhaseComponent::BeginPendingTransition(int64& OutTransitionId)
{
	OutTransitionId = 0;
	if (!GetOwner()->HasAuthority() || !HasPendingTransition() || !ASC.IsValid())
	{
		return false;
	}
	RunState = EBossPhaseRunState::Transitioning;
	OutTransitionId = ActiveTransitionId;
	SequenceTransitionId = ActiveTransitionId;
	const FBossPhaseDefinition& Phase = Config->Phases[PendingPhaseIndex];
	if (Config->IsLogOnlyPhase(PendingPhaseIndex) || !Phase.bUseTransitionSequence)
	{
		FinishTransitionSequence(TEXT("Disabled"));
		return true;
	}
	// 미지정 연출과 잘못된 길이 또는 제한 시간도 전투 실패 대신 연출만 건너뜀
	if (!Phase.TransitionSequence || !Phase.TransitionSequence->GetMovieScene()
		|| Phase.TransitionSequence->GetMovieScene()->GetPlaybackRange().IsEmpty()
		|| !FMath::IsFinite(Phase.TransitionTimeout) || Phase.TransitionTimeout <= 0.f)
	{
		FinishTransitionSequence(TEXT("Invalid sequence or timeout"));
		return true;
	}
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bAutoPlay = false;
	Settings.LoopCount.Value = 0;
	Settings.bPauseAtEnd = false;
	Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;
	ALevelSequenceActor* SequenceActor = nullptr;
	TransitionSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Phase.TransitionSequence, Settings, SequenceActor);
	TransitionSequenceActor = SequenceActor;
	if (!IsValid(TransitionSequencePlayer) || !IsValid(TransitionSequenceActor))
	{
		FinishTransitionSequence(TEXT("Creation failed"));
		return true;
	}
	// 동적으로 생성한 보스는 시퀀스 바인딩 태그 Boss로 연결; 카메라만 사용하는 연출은 태그 생략 가능
	if (!Phase.TransitionSequence->FindBindingsByTag(TEXT("Boss")).IsEmpty())
		TransitionSequenceActor->SetBindingByTag(TEXT("Boss"), {GetOwner()}, false);
	TransitionSequencePlayer->OnFinished.AddDynamic(this, &UEnemyPhaseComponent::HandleTransitionSequenceFinished);
	TransitionSequencePlayer->OnStop.AddDynamic(this, &UEnemyPhaseComponent::HandleTransitionSequenceStopped);
	GetWorld()->GetTimerManager().SetTimer(TransitionTimeoutHandle, this,
		&UEnemyPhaseComponent::HandleTransitionTimeout, Phase.TransitionTimeout, false);
	SetComponentTickEnabled(true);
	TransitionSequencePlayer->Play();
	// Play 중 즉시 종료된 경우에는 이미 완료된 요청을 다시 처리하지 않음
	if (TransitionResult == EEnemyPatternResult::Running && IsValid(TransitionSequencePlayer)
		&& !TransitionSequencePlayer->IsPlaying() && !TransitionSequencePlayer->IsPaused())
		FinishTransitionSequence(TEXT("Playback failed"));
	return true;
}

// 현재 전환 번호와 일치하는 결과만 반환
EEnemyPatternResult UEnemyPhaseComponent::GetTransitionResult(int64 TransitionId) const
{
	return IsTransitioning() && TransitionId != 0 && TransitionId == ActiveTransitionId
		? TransitionResult : EEnemyPatternResult::Invalid;
}

// 정상 종료 시 다음 페이즈 적용 허용
void UEnemyPhaseComponent::HandleTransitionSequenceFinished()
{
	FinishTransitionSequence(TEXT("Finished"));
}

// 외부에서 시퀀스를 Stop해도 다음 페이즈 적용 허용
void UEnemyPhaseComponent::HandleTransitionSequenceStopped()
{
	FinishTransitionSequence(TEXT("Stopped"));
}

// 타임아웃은 전투 실패가 아니라 연출 생략으로 처리
void UEnemyPhaseComponent::HandleTransitionTimeout()
{
	FinishTransitionSequence(TEXT("Timeout"));
}

// 요청 번호가 일치하는 재생만 취소하고 다음 페이즈로 진행
bool UEnemyPhaseComponent::CancelTransitionSequence(int64 TransitionId)
{
	if (!GetOwner()->HasAuthority() || GetTransitionResult(TransitionId) != EEnemyPatternResult::Running)
	{
		return false;
	}
	FinishTransitionSequence(TEXT("Cancelled"));
	return true;
}

// 재생 상태를 먼저 완료로 바꾼 뒤 정리하여 Stop 콜백의 중복 완료 방지
void UEnemyPhaseComponent::FinishTransitionSequence(const TCHAR* Reason)
{
	if (!IsTransitioning() || TransitionResult != EEnemyPatternResult::Running
		|| ActiveTransitionId == 0 || SequenceTransitionId != ActiveTransitionId) return;
	TransitionResult = EEnemyPatternResult::Completed;
	UE_LOG(LogTemp, Log, TEXT("[BossPhase][TransitionSequence] %s: %s"),
		*Config->Phases[PendingPhaseIndex].PhaseId.ToString(), Reason);
	CleanupTransitionSequence();
}

// 구독 해제 및 소유 참조 초기화 후 Stop과 상태 복원; 전투 초기화는 완료 통지를 발생시키지 않음
void UEnemyPhaseComponent::CleanupTransitionSequence()
{
	SequenceTransitionId = 0;
	SetComponentTickEnabled(false);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TransitionTimeoutHandle);
	}
	ULevelSequencePlayer* Player = TransitionSequencePlayer;
	ALevelSequenceActor* Actor = TransitionSequenceActor;
	TransitionSequencePlayer = nullptr;
	TransitionSequenceActor = nullptr;
	if (IsValid(Player))
	{
		Player->OnFinished.RemoveDynamic(this, &UEnemyPhaseComponent::HandleTransitionSequenceFinished);
		Player->OnStop.RemoveDynamic(this, &UEnemyPhaseComponent::HandleTransitionSequenceStopped);
		Player->Stop();
		Player->RestoreState();
	}
	if (IsValid(Actor))
	{
		Actor->Destroy();
	}
}

// 정상 종료한 전환에 한해 다음 페이즈 설정 확정
bool UEnemyPhaseComponent::CompleteTransition(int64 TransitionId)
{
	if (GetTransitionResult(TransitionId) != EEnemyPatternResult::Completed || !ApplyPhase.IsBound())
	{
		return false;
	}
	CurrentPhaseIndex = PendingPhaseIndex;
	PendingPhaseIndex = INDEX_NONE;
	if (!ApplyCurrentPhase())
	{
		return false;
	}
	// 기믹 적용 중 정지 요청이 발생했다면 다시 활성화하지 않음
	if (!IsTransitioning() || ActiveTransitionId != TransitionId)
	{
		return false;
	}
	RunState = EBossPhaseRunState::Active;
	ActiveTransitionId = 0;
	if (bPendingGimmickCompletion)
	{
		RequestNextPhase();
	}
	return true;
}

// 요청을 먼저 무효화한 뒤 타이머, 구독 및 능력 실행 정리
void UEnemyPhaseComponent::StopAndInvalidate()
{
	ActivePhaseExecutionId = 0;
	bApplyingPhase = false;
	bPendingGimmickCompletion = false;
	RunState = EBossPhaseRunState::Stopped;
	ActiveTransitionId = 0;
	PendingPhaseIndex = INDEX_NONE;
	TransitionResult = EEnemyPatternResult::Invalid;
	CleanupTransitionSequence();
}

// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
void UEnemyPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAndInvalidate();
	ApplyPhase.Unbind();
	OnTransitionRequested.Clear();
	Super::EndPlay(EndPlayReason);
}

// 현재 페이즈가 다른 담당자의 로그 전용 항목인지 확인
bool UEnemyPhaseComponent::IsCurrentPhaseLogOnly() const
{
	return Config && Config->IsLogOnlyPhase(CurrentPhaseIndex);
}

// 현재 배리어와 진입 번호가 일치하는 기믹 완료만 다음 페이즈 요청으로 변환
bool UEnemyPhaseComponent::RequestNextPhaseFromGimmick(FName PhaseId, int64 PhaseExecutionId)
{
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	if (!GetOwner()->HasAuthority() || !Phase
		|| (!IsCurrentPhaseLogOnly() && Phase->Kind != EBossPhaseKind::Barrier) || Phase->PhaseId != PhaseId
		|| PhaseExecutionId == 0 || PhaseExecutionId != ActivePhaseExecutionId
		|| !Config->Phases.IsValidIndex(CurrentPhaseIndex + 1)) return false;
	// 진입 중 즉시 완료되는 기믹은 설정 적용을 마친 뒤 한 번만 처리
	if (bApplyingPhase && IsTransitioning())
	{
		if (bPendingGimmickCompletion)
		{
			return false;
		}
		bPendingGimmickCompletion = true;
		return true;
	}
	if (!IsActive())
	{
		return false;
	}
	RequestNextPhase();
	return true;
}

// 페이즈별 실행 번호를 부여하고 외부 적용 콜백의 정지 및 동기 완료 처리
bool UEnemyPhaseComponent::ApplyCurrentPhase()
{
	if (!Config || !Config->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		return false;
	}
	ActivePhaseExecutionId = ++NextPhaseExecutionId;
	const int64 ApplyingExecutionId = ActivePhaseExecutionId;
	bPendingGimmickCompletion = false;
	bApplyingPhase = true;
	if (IsCurrentPhaseLogOnly())
		UE_LOG(LogTemp, Log, TEXT("[BossPhase][LogOnly] %s waiting for manual completion"),
			*Config->Phases[CurrentPhaseIndex].PhaseId.ToString());
	const bool bApplied = IsCurrentPhaseLogOnly() || ApplyPhase.Execute(Config->Phases[CurrentPhaseIndex]);
	bApplyingPhase = false;
	return bApplied && IsTransitioning() && ActivePhaseExecutionId == ApplyingExecutionId;
}
