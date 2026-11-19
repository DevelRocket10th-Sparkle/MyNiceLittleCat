// Fill out your copyright notice in the Description page of Project Settings.


#include "Cinematics/PlayCinematicAsyncAction.h"
#include "Cinematics/CinematicLog.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LevelSequence.h"

#include "Cinematics/CinematicSubsystem.h"

UPlayCinematicAsyncAction* UPlayCinematicAsyncAction::PlayCinematic(UObject* WorldContextObject, const FCinematicRequest& Request)
{
	UPlayCinematicAsyncAction* Action = NewObject<UPlayCinematicAsyncAction>();
	Action->Request = Request;
	Action->World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;

	// 재생이 끝날 때까지 GC되지 않도록 게임 인스턴스에 등록 (Finish에서 해제)
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UPlayCinematicAsyncAction::Activate()
{
	UWorld* PlayWorld = World.Get();
	UCinematicSubsystem* Cinematics = PlayWorld ? PlayWorld->GetSubsystem<UCinematicSubsystem>() : nullptr;

	if (Cinematics)
	{
		// 콜백은 Play() 안에서 호출되지 않으므로 핸들을 먼저 받아 둘 수 있음
		Handle = Cinematics->Play(Request,
			FOnCinematicMarker::CreateUObject(this, &UPlayCinematicAsyncAction::HandleMarker),
			FOnCinematicEnded::CreateUObject(this, &UPlayCinematicAsyncAction::HandleEnded));
	}

	if (!Handle.IsValid())
	{
		UE_LOG(LogCinematic, Warning, TEXT("[PlayCinematic] 시퀀스 %s 재생 시작 실패"), *GetNameSafe(Request.Sequence));
		OnFailed.Broadcast();
		Finish();
		return;
	}

	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UPlayCinematicAsyncAction::HandleWorldCleanup);
}

void UPlayCinematicAsyncAction::Cancel()
{
	if (!Handle.IsValid())
	{
		return;
	}

	const FCinematicHandle CancelledHandle = Handle;
	Handle.Reset();

	if (UCinematicSubsystem* Cinematics = World.IsValid() ? World->GetSubsystem<UCinematicSubsystem>() : nullptr)
	{
		Cinematics->Cancel(CancelledHandle);
	}

	Finish();
}

bool UPlayCinematicAsyncAction::IsPlaying() const
{
	const UCinematicSubsystem* Cinematics = World.IsValid() ? World->GetSubsystem<UCinematicSubsystem>() : nullptr;
	return Cinematics && Handle.IsValid() && Cinematics->IsPlaying(Handle);
}

void UPlayCinematicAsyncAction::HandleMarker(FCinematicHandle InHandle, const FCinematicMarker& Marker)
{
	if (InHandle == Handle)
	{
		OnMarker.Broadcast(Marker);
	}
}

void UPlayCinematicAsyncAction::HandleEnded(FCinematicHandle InHandle, ECinematicEndReason Reason)
{
	if (InHandle != Handle)
	{
		return;
	}

	// 종료 핀에서 다시 Cancel을 불러도 아무 일도 하지 않도록 먼저 비움
	Handle.Reset();
	OnEnded.Broadcast(Reason);
	Finish();
}

void UPlayCinematicAsyncAction::HandleWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	if (InWorld == World.Get())
	{
		Handle.Reset();
		Finish();
	}
}

void UPlayCinematicAsyncAction::Finish()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldCleanupHandle.Reset();
	SetReadyToDestroy();
}
