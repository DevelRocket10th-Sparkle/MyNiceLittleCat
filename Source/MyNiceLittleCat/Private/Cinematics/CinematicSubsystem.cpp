// Fill out your copyright notice in the Description page of Project Settings.


#include "Cinematics/CinematicSubsystem.h"
#include "Cinematics/CinematicLog.h"

#include "DefaultLevelSequenceInstanceData.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "LevelUtils.h"
#include "MovieScene.h"
#include "MovieSceneBindingOverrides.h"
#include "MovieSceneObjectBindingID.h"
#include "TimerManager.h"

#include "Systems/CameraOcclusionComponent.h"

namespace
{
	// 요청의 종료 상태 설정을 시퀀스 플레이어 설정 값으로 변환
	EMovieSceneCompletionModeOverride ToCompletionOverride(ECinematicEndState EndState)
	{
		switch (EndState)
		{
		case ECinematicEndState::RestoreState:
			return EMovieSceneCompletionModeOverride::ForceRestoreState;

		case ECinematicEndState::KeepState:
			return EMovieSceneCompletionModeOverride::ForceKeepState;

		default:
			return EMovieSceneCompletionModeOverride::None;
		}
	}

	// 재생 중이거나 일시 정지 상태인지 확인
	bool IsPlayerRunning(const ULevelSequencePlayer* Player)
	{
		return IsValid(Player) && (Player->IsPlaying() || Player->IsPaused());
	}
}

/* UCinematicPlaybackListener */

void UCinematicPlaybackListener::Initialize(UCinematicSubsystem* InSubsystem, FCinematicHandle InHandle)
{
	Subsystem = InSubsystem;
	Handle = InHandle;
}

void UCinematicPlaybackListener::HandleFinished()
{
	if (UCinematicSubsystem* Owner = Subsystem.Get())
	{
		Owner->NotifyPlayerEnded(Handle, true);
	}
}

void UCinematicPlaybackListener::HandleStopped()
{
	if (UCinematicSubsystem* Owner = Subsystem.Get())
	{
		Owner->NotifyPlayerEnded(Handle, false);
	}
}

/* UCinematicSubsystem: 재생 요청 */

FCinematicHandle UCinematicSubsystem::Play(const FCinematicRequest& Request, FOnCinematicMarker OnMarker, FOnCinematicEnded OnEnded)
{
	UWorld* World = GetWorld();
	ULevelSequence* Sequence = Request.Sequence;
	const UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;

	// 재생 범위가 없는 시퀀스는 시작하지 않음
	if (!World || !MovieScene || MovieScene->GetPlaybackRange().IsEmpty())
	{
		UE_LOG(LogCinematic, Warning, TEXT("[Cinematic] Invalid sequence: %s"), *GetNameSafe(Sequence));
		return FCinematicHandle();
	}

	/* 플레이어 생성 */

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bAutoPlay = false;
	Settings.LoopCount.Value = 0;
	Settings.bPauseAtEnd = false;
	Settings.bDisableMovementInput = Request.bCutscene;
	Settings.bDisableLookAtInput = Request.bCutscene;
	Settings.bHideHud = Request.bCutscene;
	Settings.FinishCompletionStateOverride = ToCompletionOverride(Request.EndState);

	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, Settings, SequenceActor);
	if (!IsValid(Player) || !IsValid(SequenceActor))
	{
		if (IsValid(SequenceActor))
		{
			SequenceActor->Destroy();
		}

		UE_LOG(LogCinematic, Warning, TEXT("[Cinematic] Failed to create player: %s"), *Sequence->GetName());
		return FCinematicHandle();
	}

	/* 좌표계와 바인딩 */

	if (Request.bUseTransformOrigin)
	{
		// 시퀀스 키를 작업한 좌표계(레벨 인스턴스 로컬)를 현재 배치 위치로 옮김
		if (UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(SequenceActor->DefaultInstanceData))
		{
			SequenceActor->bOverrideInstanceData = true;
			InstanceData->TransformOrigin = Request.TransformOrigin;
		}
	}

	ApplyBindings(*SequenceActor, *Sequence, Request);

	/* 재생 상태 등록 */

	FCinematicHandle Handle;
	Handle.Id = ++NextHandleId;

	UCinematicPlaybackListener* Listener = NewObject<UCinematicPlaybackListener>(this);
	Listener->Initialize(this, Handle);
	Player->OnFinished.AddDynamic(Listener, &UCinematicPlaybackListener::HandleFinished);
	Player->OnStop.AddDynamic(Listener, &UCinematicPlaybackListener::HandleStopped);

	FCinematicPlayback& Playback = Playbacks.AddDefaulted_GetRef();
	Playback.Handle = Handle;
	Playback.Player = Player;
	Playback.SequenceActor = SequenceActor;
	Playback.Listener = Listener;
	Playback.Markers = ReadMarkers(Sequence);
	Playback.PlaybackStartSeconds = MovieScene->GetTickResolution().AsSeconds(MovieScene->GetPlaybackRange().GetLowerBoundValue());
	Playback.StartWorldSeconds = World->GetTimeSeconds();
	Playback.TimeoutSeconds = Request.TimeoutSeconds;
	Playback.bCutscene = Request.bCutscene;
	Playback.bSkippable = Request.bCutscene && Request.bSkippable;
	Playback.OnMarker = MoveTemp(OnMarker);
	Playback.OnEnded = MoveTemp(OnEnded);

	if (Playback.bCutscene)
	{
		BeginCutsceneEffects();
	}

	// 주의: Play() 중 시퀀스 이벤트가 다른 재생을 요청하면 Playbacks 배열이 재할당될 수 있음
	// → 위의 Playback 참조는 여기서부터 사용하지 않음
	Player->Play();

	return Handle;
}

void UCinematicSubsystem::Cancel(FCinematicHandle Handle)
{
	const int32 Index = Playbacks.IndexOfByPredicate(
		[Handle](const FCinematicPlayback& Playback)
		{
			return Playback.Handle == Handle;
		});
	if (Index == INDEX_NONE)
	{
		return;
	}

	// 목록에서 먼저 제거하여 정리 중 발생한 플레이어 이벤트가 이 재생을 다시 찾지 못하게 함
	FCinematicPlayback Playback = MoveTemp(Playbacks[Index]);
	Playbacks.RemoveAt(Index);

	ReleasePlayback(Playback);
}

bool UCinematicSubsystem::TrySkipActiveCutscene()
{
	// 가장 최근에 시작한 재생부터 확인
	for (int32 Index = Playbacks.Num() - 1; Index >= 0; --Index)
	{
		FCinematicPlayback& Playback = Playbacks[Index];
		if (!Playback.bSkippable || Playback.PendingEndReason.IsSet() || !IsPlayerRunning(Playback.Player))
		{
			continue;
		}

		const FCinematicHandle Handle = Playback.Handle;

		// GoToEndAndStop이 발생시키는 OnStop을 중단으로 기록하지 않도록 리스너를 먼저 해제
		if (Playback.Listener)
		{
			Playback.Player->OnFinished.RemoveAll(Playback.Listener);
			Playback.Player->OnStop.RemoveAll(Playback.Listener);
		}

		// 마지막 프레임을 평가한 뒤 정지해야 KeepState 트랙이 연출 최종 상태로 남음
		Playback.Player->GoToEndAndStop();

		// 스킵은 입력 처리 중 호출되므로 다음 Tick을 기다리지 않고 바로 종료 처리
		EndPlayback(Handle, ECinematicEndReason::Skipped);
		return true;
	}

	return false;
}

/* 상태 조회 */

bool UCinematicSubsystem::IsPlaying(FCinematicHandle Handle) const
{
	const FCinematicPlayback* Playback = FindPlayback(Handle);
	return Playback && !Playback->PendingEndReason.IsSet();
}

/* 유틸리티 */

TArray<FCinematicMarker> UCinematicSubsystem::ReadMarkers(const UMovieSceneSequence* Sequence)
{
	TArray<FCinematicMarker> Markers;

	const UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return Markers;
	}

	// 마커 프레임은 틱 해상도 단위이므로 초로 바꾼 뒤 재생 범위 시작 시각을 뺌
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const double StartSeconds = TickResolution.AsSeconds(MovieScene->GetPlaybackRange().GetLowerBoundValue());

	for (const FMovieSceneMarkedFrame& MarkedFrame : MovieScene->GetMarkedFrames())
	{
		FCinematicMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.Label = MarkedFrame.Label;
		Marker.Seconds = TickResolution.AsSeconds(MarkedFrame.FrameNumber) - StartSeconds;
	}

	// 같은 시각의 마커는 시퀀서에 추가한 순서를 유지
	Markers.StableSort(
		[](const FCinematicMarker& A, const FCinematicMarker& B)
		{
			return A.Seconds < B.Seconds;
		});

	return Markers;
}

FTransform UCinematicSubsystem::GetLevelTransform(const AActor& Actor)
{
	const ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(Actor.GetLevel());
	return StreamingLevel ? StreamingLevel->LevelTransform : FTransform::Identity;
}

/* 리스너 전용 */

void UCinematicSubsystem::NotifyPlayerEnded(FCinematicHandle Handle, bool bFinishedNaturally)
{
	FCinematicPlayback* Playback = FindPlayback(Handle);
	if (!Playback)
	{
		return;
	}

	// 정상 완료 시 OnStop → OnFinished 순서로 둘 다 오므로 Finished가 Interrupted를 덮어씀
	if (bFinishedNaturally)
	{
		Playback->PendingEndReason = ECinematicEndReason::Finished;
	}
	else if (!Playback->PendingEndReason.IsSet())
	{
		Playback->PendingEndReason = ECinematicEndReason::Interrupted;
	}
}

/* UWorldSubsystem */

bool UCinematicSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCinematicSubsystem::Deinitialize()
{
	// 월드 종료 중에는 콜백을 호출하지 않고 정리만 수행
	OnCutsceneStateChanged.Clear();

	TArray<FCinematicPlayback> Remaining = MoveTemp(Playbacks);
	Playbacks.Reset();

	for (FCinematicPlayback& Playback : Remaining)
	{
		ReleasePlayback(Playback);
	}

	ActiveCutsceneCount = 0;
	SuspendedOcclusion.Reset();

	Super::Deinitialize();
}

/* FTickableGameObject */

void UCinematicSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 콜백 안에서 재생이 추가/제거될 수 있으므로 이번 Tick에 처리할 핸들을 먼저 복사
	TArray<FCinematicHandle, TInlineAllocator<4>> Handles;
	for (const FCinematicPlayback& Playback : Playbacks)
	{
		Handles.Add(Playback.Handle);
	}

	const double Now = GetWorld()->GetTimeSeconds();

	for (const FCinematicHandle Handle : Handles)
	{
		// 1) 정상 진행 중이면 지난 마커 전달
		if (const FCinematicPlayback* Playback = FindPlayback(Handle); Playback && !Playback->PendingEndReason.IsSet())
		{
			DispatchReachedMarkers(Handle);
		}

		// 마커 콜백에서 취소됐을 수 있으므로 다시 검색
		FCinematicPlayback* Playback = FindPlayback(Handle);
		if (!Playback)
		{
			continue;
		}

		// 2) 플레이어나 임시 액터가 외부에서 제거됨
		if (!Playback->PendingEndReason.IsSet()
			&& (!IsValid(Playback->Player) || !IsValid(Playback->SequenceActor)))
		{
			Playback->PendingEndReason = ECinematicEndReason::Interrupted;
		}

		// 3) 제한 시간 초과
		if (!Playback->PendingEndReason.IsSet()
			&& Playback->TimeoutSeconds > 0.f
			&& Now - Playback->StartWorldSeconds >= Playback->TimeoutSeconds)
		{
			UE_LOG(LogCinematic, Warning, TEXT("[Cinematic] Timed out after %.1fs: %s"),
				Playback->TimeoutSeconds, *GetNameSafe(Playback->Player ? Playback->Player->GetSequence() : nullptr));
			Playback->PendingEndReason = ECinematicEndReason::TimedOut;
		}

		// 4) 종료 이유가 기록됐으면 종료 처리
		if (Playback->PendingEndReason.IsSet())
		{
			EndPlayback(Handle, Playback->PendingEndReason.GetValue());
		}
	}
}

TStatId UCinematicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCinematicSubsystem, STATGROUP_Tickables);
}

/* 내부 처리 */

FCinematicPlayback* UCinematicSubsystem::FindPlayback(FCinematicHandle Handle)
{
	return Playbacks.FindByPredicate(
		[Handle](const FCinematicPlayback& Playback)
		{
			return Playback.Handle == Handle;
		});
}

const FCinematicPlayback* UCinematicSubsystem::FindPlayback(FCinematicHandle Handle) const
{
	return Playbacks.FindByPredicate(
		[Handle](const FCinematicPlayback& Playback)
		{
			return Playback.Handle == Handle;
		});
}

void UCinematicSubsystem::DispatchReachedMarkers(FCinematicHandle Handle)
{
	while (true)
	{
		// 콜백이 재생을 취소하거나 배열을 바꿀 수 있으므로 매번 다시 검색
		FCinematicPlayback* Playback = FindPlayback(Handle);
		if (!Playback || !IsValid(Playback->Player) || !Playback->Markers.IsValidIndex(Playback->NextMarkerIndex))
		{
			return;
		}

		// 플레이어 현재 시각을 재생 범위 시작 기준으로 변환
		const double ElapsedSeconds = Playback->Player->GetCurrentTime().AsSeconds() - Playback->PlaybackStartSeconds;

		const FCinematicMarker& NextMarker = Playback->Markers[Playback->NextMarkerIndex];
		if (NextMarker.Seconds > ElapsedSeconds + KINDA_SMALL_NUMBER)
		{
			return;
		}

		// 콜백 중 재생이 제거될 수 있으므로 마커와 콜백을 복사한 뒤 호출
		FCinematicMarker Marker = NextMarker;
		Marker.bReachedByPlayback = true;
		++Playback->NextMarkerIndex;

		const FOnCinematicMarker Callback = Playback->OnMarker;
		Callback.ExecuteIfBound(Handle, Marker);
	}
}

void UCinematicSubsystem::EndPlayback(FCinematicHandle Handle, ECinematicEndReason Reason)
{
	const int32 Index = Playbacks.IndexOfByPredicate(
		[Handle](const FCinematicPlayback& Playback)
		{
			return Playback.Handle == Handle;
		});
	if (Index == INDEX_NONE)
	{
		return;
	}

	// 목록에서 먼저 제거: 아래 콜백에서 Cancel/Play가 다시 불려도 이 재생을 건드리지 않음
	FCinematicPlayback Playback = MoveTemp(Playbacks[Index]);
	Playbacks.RemoveAt(Index);

	ReleasePlayback(Playback);

	// 지나치지 못한 마커는 순서대로 한꺼번에 전달
	// → 스킵해도 문 열림, 계단 붕괴 같은 연출의 최종 상태가 항상 보장됨
	for (int32 MarkerIndex = Playback.NextMarkerIndex; MarkerIndex < Playback.Markers.Num(); ++MarkerIndex)
	{
		FCinematicMarker Marker = Playback.Markers[MarkerIndex];
		Marker.bReachedByPlayback = false;
		Playback.OnMarker.ExecuteIfBound(Handle, Marker);
	}

	UE_LOG(LogCinematic, Log, TEXT("[Cinematic] Ended (%s)"), *UEnum::GetValueAsString(Reason));

	Playback.OnEnded.ExecuteIfBound(Handle, Reason);
}

void UCinematicSubsystem::ReleasePlayback(FCinematicPlayback& Playback)
{
	// 정지 콜백이 다시 들어오지 않도록 리스너를 먼저 해제
	if (IsValid(Playback.Player))
	{
		if (Playback.Listener)
		{
			Playback.Player->OnFinished.RemoveAll(Playback.Listener);
			Playback.Player->OnStop.RemoveAll(Playback.Listener);
		}

		// 제한 시간 초과, 취소처럼 아직 재생 중인 경우에만 정지 (완료 모드 설정에 따라 상태 복원)
		if (IsPlayerRunning(Playback.Player))
		{
			Playback.Player->Stop();
		}
	}

	// 시퀀스 콜백 안에서 액터를 바로 지우지 않도록 다음 Tick에 제거
	if (IsValid(Playback.SequenceActor))
	{
		const TWeakObjectPtr<ALevelSequenceActor> WeakActor = Playback.SequenceActor.Get();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(
				[WeakActor]()
				{
					if (WeakActor.IsValid())
					{
						WeakActor->Destroy();
					}
				});
		}
	}

	if (Playback.bCutscene)
	{
		Playback.bCutscene = false;
		EndCutsceneEffects();
	}

	Playback.Player = nullptr;
	Playback.SequenceActor = nullptr;
	Playback.Listener = nullptr;
}

void UCinematicSubsystem::ApplyBindings(ALevelSequenceActor& SequenceActor, ULevelSequence& Sequence, const FCinematicRequest& Request)
{
	UMovieScene* MovieScene = Sequence.GetMovieScene();

	for (const FCinematicBinding& Binding : Request.Bindings)
	{
		UObject* Object = Binding.Object;
		if (Binding.Tag.IsNone() || !IsValid(Object))
		{
			continue;
		}

		// 시퀀스에 없는 태그는 무시: 같은 요청 형식을 카메라만 쓰는 연출에도 사용할 수 있게 함
		const TArray<FMovieSceneObjectBindingID>& BindingIds = Sequence.FindBindingsByTag(Binding.Tag);
		if (BindingIds.IsEmpty())
		{
			continue;
		}

		/* 액터: 엔진의 태그 바인딩 사용 */

		if (AActor* Actor = Cast<AActor>(Object))
		{
			SequenceActor.SetBindingByTag(Binding.Tag, { Actor }, false);
			continue;
		}

		/* 컴포넌트: 태그가 붙은 바인딩마다 직접 덮어쓰기 */

		UActorComponent* Component = Cast<UActorComponent>(Object);
		if (!Component || !SequenceActor.BindingOverrides)
		{
			UE_LOG(LogCinematic, Warning, TEXT("[Cinematic] %s: tag '%s' supports only actors and components"),
				*Sequence.GetName(), *Binding.Tag.ToString());
			continue;
		}

		for (const FMovieSceneObjectBindingID& BindingId : BindingIds)
		{
			// 컴포넌트 바인딩은 부모(액터) 바인딩을 컴포넌트의 소유 액터로 먼저 연결
			// → 다른 레벨을 기준으로 작업한 시퀀스도 현재 레벨의 액터/컴포넌트로 재생됨
			const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(BindingId.GetGuid());
			if (Possessable && Possessable->GetParent().IsValid() && Component->GetOwner())
			{
				SequenceActor.SetBinding(UE::MovieScene::FFixedObjectBindingID(Possessable->GetParent(), MovieSceneSequenceID::Root),
					{ Component->GetOwner() });
			}

			SequenceActor.BindingOverrides->SetBinding(BindingId, { Component });
		}
	}
}

void UCinematicSubsystem::BeginCutsceneEffects()
{
	// 이미 다른 컷신이 처리 중이면 횟수만 증가
	if (ActiveCutsceneCount++ > 0)
	{
		return;
	}

	// 컷신 카메라가 벽 너머를 비출 때 플레이어 기준 가림 처리가 화면을 가리지 않도록 일시 정지
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UCameraOcclusionComponent* Occlusion = PlayerPawn ? PlayerPawn->FindComponentByClass<UCameraOcclusionComponent>() : nullptr;
	if (Occlusion)
	{
		SuspendedOcclusion = Occlusion;
		Occlusion->SetOcclusionSuspended(true);
	}

	OnCutsceneStateChanged.Broadcast(true);
}

void UCinematicSubsystem::EndCutsceneEffects()
{
	if (ActiveCutsceneCount <= 0)
	{
		return;
	}

	// 마지막 컷신이 끝날 때만 복구
	if (--ActiveCutsceneCount > 0)
	{
		return;
	}

	if (UCameraOcclusionComponent* Occlusion = SuspendedOcclusion.Get())
	{
		Occlusion->SetOcclusionSuspended(false);
	}
	SuspendedOcclusion.Reset();

	OnCutsceneStateChanged.Broadcast(false);
}
