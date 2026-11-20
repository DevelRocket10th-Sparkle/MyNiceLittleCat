// Fill out your copyright notice in the Description page of Project Settings.


#include "Cinematics/CinematicMarkerEffectsComponent.h"
#include "Cinematics/CinematicLog.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Cinematics/CinematicSubsystem.h"
#include "Interfaces/ActivatableInterface.h"

UCinematicMarkerEffectsComponent::UCinematicMarkerEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCinematicMarkerEffectsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAll();
	Super::EndPlay(EndPlayReason);
}

bool UCinematicMarkerEffectsComponent::HandleMarker(const FCinematicMarker& Marker, const FCinematicCueAnchors& Anchors)
{
	const FCinematicMarkerArgs Args(Marker.Label);

	if (Args.IsCommand(TEXT("Event")))
	{
		HandleEventMarker(Args);
		return true;
	}

	if (Args.IsCommand(TEXT("Cue")))
	{
		HandleCueMarker(Args, Marker.bReachedByPlayback, Anchors);
		return true;
	}

	return false;
}

bool UCinematicMarkerEffectsComponent::HandleMarkerForOwner(const FCinematicMarker& Marker)
{
	FCinematicCueAnchors Anchors;
	if (const AActor* Owner = GetOwner())
	{
		Anchors.StageTransform = UCinematicSubsystem::GetLevelTransform(*Owner);
	}
	if (const ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		Anchors.PlayerMesh = Player->GetMesh();
	}
	return HandleMarker(Marker, Anchors);
}

void UCinematicMarkerEffectsComponent::StopAll()
{
	for (FEnemyFeedbackInstance& Instance : ActiveCueInstances)
	{
		Instance.Stop();
	}
	ActiveCueInstances.Reset();
}

// Event:<이름>[:Off] → 등록된 액터에 기믹 신호(IActivatableInterface)로 전달
void UCinematicMarkerEffectsComponent::HandleEventMarker(const FCinematicMarkerArgs& Args)
{
	const FName EventName(*Args.GetArg(0));
	const bool bDeactivate = Args.ArgEquals(1, TEXT("Off"));

	AActor* Receiver = EventReceivers.FindRef(EventName);
	if (!Receiver || !Receiver->Implements<UActivatableInterface>())
	{
		UE_LOG(LogCinematic, Warning, TEXT("[MarkerEffects] %s: no IActivatableInterface receiver for cinematic event '%s'"),
			*GetNameSafe(GetOwner()), *EventName.ToString());
		return;
	}

	// 신호를 보낸 쪽은 소유 액터 (연출 담당자 등)
	if (bDeactivate)
	{
		IActivatableInterface::Execute_Deactivate(Receiver, GetOwner());
	}
	else
	{
		IActivatableInterface::Execute_Activate(Receiver, GetOwner());
	}
}

void UCinematicMarkerEffectsComponent::HandleCueMarker(const FCinematicMarkerArgs& Args, bool bReachedByPlayback, const FCinematicCueAnchors& Anchors)
{
	const FName CueName(*Args.GetArg(0));
	const FStageCinematicCue* Cue = CinematicCues.Find(CueName);

	// 등록하지 않은 이름: 자리만 만들어 둔 마커이므로 조용히 무시
	if (!Cue)
	{
		return;
	}

	// 스킵으로 지나간 연출은 기본적으로 재생하지 않음 (스킵 직후 소리가 몰려 나는 것 방지)
	if (!bReachedByPlayback && !Cue->bPlayWhenSkipped)
	{
		return;
	}

	/* Stage 기준: 스테이지 로컬 좌표 → 월드 */

	if (Cue->Target == EStageCueTarget::Stage)
	{
		EnemyFeedback::PlayAtLocation(this, Cue->Feedback, Anchors.StageTransform.TransformPosition(Cue->Offset), Anchors.StageTransform.Rotator());
		return;
	}

	/* 기준 컴포넌트 */

	USceneComponent* Anchor = nullptr;
	switch (Cue->Target)
	{
	case EStageCueTarget::Boss:		Anchor = Anchors.BossMesh; break;
	case EStageCueTarget::Player:	Anchor = Anchors.PlayerMesh; break;
	case EStageCueTarget::Actor:	Anchor = IsValid(Cue->TargetActor) ? Cue->TargetActor->GetRootComponent() : nullptr; break;
	default:						break;
	}

	if (!Anchor)
	{
		UE_LOG(LogCinematic, Warning, TEXT("[MarkerEffects] %s: cinematic cue '%s' has no target"), *GetNameSafe(GetOwner()), *CueName.ToString());
		return;
	}

	/* 붙여서 재생: 기준을 따라 움직임, 반복 연출은 시간 또는 초기화로 정지 */

	if (Cue->bAttach)
	{
		FEnemyFeedbackInstance Instance = EnemyFeedback::PlayAttached(Cue->Feedback, Anchor, Cue->Socket, Cue->Offset);

		if (Cue->StopAfterSeconds > 0.f)
		{
			FTimerHandle StopTimer;
			GetWorld()->GetTimerManager().SetTimer(StopTimer, FTimerDelegate::CreateWeakLambda(this, [Instance]() mutable
			{
				Instance.Stop();
			}), Cue->StopAfterSeconds, false);
		}

		ActiveCueInstances.Add(Instance);
		return;
	}

	/* 그 자리에서 한 번 재생 */

	const FTransform Base = Cue->Socket.IsNone() ? Anchor->GetComponentTransform() : Anchor->GetSocketTransform(Cue->Socket);
	EnemyFeedback::PlayAtLocation(this, Cue->Feedback, Base.TransformPosition(Cue->Offset), Base.Rotator());
}
