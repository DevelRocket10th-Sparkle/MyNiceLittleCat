/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 기믹 신호 연결 표시 컴포넌트 구현

#include "Gimmicks/GimmickLinkVisualizerComponent.h"
#include "Gimmicks/GimmickActorBase.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace GimmickLinkVisualizer
{
	const FColor OutgoingColor(60, 200, 255);
	const FColor IncomingColor(255, 210, 40);
}

UGimmickLinkVisualizerComponent::UGimmickLinkVisualizerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
	bIsEditorOnly = true;
}

void UGimmickLinkVisualizerComponent::BeginPlay()
{
	Super::BeginPlay();

	// 게임 월드(PIE)에서는 그리지 않음
	SetComponentTickEnabled(false);
}

void UGimmickLinkVisualizerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_EDITOR
	UWorld* World = GetWorld();
	AGimmickActorBase* Gimmick = Cast<AGimmickActorBase>(GetOwner());
	if (!World || World->IsGameWorld() || !Gimmick)
	{
		return;
	}

	const bool bSelected = Gimmick->IsSelectedInEditor();
	if (bSelected)
	{
		// 디테일 패널의 읽기 전용 신호 정보 갱신 (다른 기믹에서 연결을 바꿔도 반영)
		Gimmick->RefreshSignalInfo();
	}
	else if (!bAlwaysShowLinks)
	{
		return;
	}

	// 보내는 연결
	for (const AActor* Linked : Gimmick->GetLinkedActors())
	{
		if (IsValid(Linked))
		{
			DrawLink(Gimmick, Linked, GimmickLinkVisualizer::OutgoingColor);
		}
	}

	// 받는 연결 (선택했을 때만, 항상 표시 모드에서는 보내는 쪽이 그리므로 중복 생략)
	if (bSelected && bShowIncomingLinks)
	{
		TArray<AGimmickActorBase*> Senders;
		Gimmick->GetLinkedSenders(Senders);
		for (const AGimmickActorBase* Sender : Senders)
		{
			DrawLink(Sender, Gimmick, GimmickLinkVisualizer::IncomingColor);
		}
	}
#endif
}

void UGimmickLinkVisualizerComponent::DrawLink(const AActor* From, const AActor* To, const FColor& Color) const
{
	const FVector Offset(0.f, 0.f, LineHeightOffset);
	const FVector Start = From->GetActorLocation() + Offset;
	const FVector End = To->GetActorLocation() + Offset;

	// 한 프레임만 그림 (매 틱 다시 그림), 맨 앞(SDPG_Foreground = 1)
	DrawDebugDirectionalArrow(GetWorld(), Start, End, 40.f, Color, false, -1.f, SDPG_Foreground, 3.f);
	DrawDebugSphere(GetWorld(), Start, 12.f, 8, Color, false, -1.f, SDPG_Foreground, 2.f);
}
