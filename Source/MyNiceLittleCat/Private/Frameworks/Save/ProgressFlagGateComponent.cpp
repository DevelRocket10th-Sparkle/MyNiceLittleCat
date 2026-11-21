#include "Frameworks/Save/ProgressFlagGateComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "GameFramework/Actor.h"

namespace
{
	// 진행 플래그를 들고 있는 게임 인스턴스 서브시스템
	UGameProgressSubsystem* FindProgress(const UActorComponent& Component)
	{
		const UGameInstance* GameInstance = Component.GetWorld() ? Component.GetWorld()->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr;
	}

	// 숨김과 충돌을 함께 바꿈
	void SetActorShown(AActor& Actor, bool bShown)
	{
		Actor.SetActorHiddenInGame(!bShown);
		Actor.SetActorEnableCollision(bShown);
	}
}

UProgressFlagGateComponent::UProgressFlagGateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 현재 플래그 상태 반영 후 변경 이벤트 구독
void UProgressFlagGateComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Flag.IsNone())
		return;

	UGameProgressSubsystem* Progress = FindProgress(*this);
	if (!Progress)
		return;

	Progress->OnProgressFlagChanged.AddUniqueDynamic(this, &UProgressFlagGateComponent::HandleProgressFlagChanged);
	Evaluate();
}

void UProgressFlagGateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameProgressSubsystem* Progress = FindProgress(*this))
	{
		Progress->OnProgressFlagChanged.RemoveDynamic(this, &UProgressFlagGateComponent::HandleProgressFlagChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UProgressFlagGateComponent::HandleProgressFlagChanged(FName ChangedFlag, bool bEnabled)
{
	if (ChangedFlag == Flag)
	{
		Evaluate();
	}
}

// 플래그 상태가 조건에 맞으면 Action 실행
void UProgressFlagGateComponent::Evaluate()
{
	if (bApplied)
		return;

	const UGameProgressSubsystem* Progress = FindProgress(*this);
	if (!Progress)
		return;

	if (Progress->HasProgressFlag(Flag) != bActWhenFlagSet)
	{
		// 등장형: 조건 전에는 숨겨 둠
		if (Action == EProgressFlagGateAction::RevealOwner)
		{
			if (AActor* Owner = GetOwner())
			{
				SetActorShown(*Owner, false);
			}
		}
		return;
	}

	ApplyAction();
}

void UProgressFlagGateComponent::ApplyAction()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
		return;

	bApplied = true;
	UE_LOG(LogTemp, Log, TEXT("[ProgressFlagGate] %s -> %s (Flag %s)"), *Owner->GetName(),
		Action == EProgressFlagGateAction::DestroyOwner ? TEXT("Destroy")
		: Action == EProgressFlagGateAction::DisableOwner ? TEXT("Disable") : TEXT("Reveal"), *Flag.ToString());

	switch (Action)
	{
	case EProgressFlagGateAction::DestroyOwner:
		Owner->Destroy();
		break;
	case EProgressFlagGateAction::DisableOwner:
		SetActorShown(*Owner, false);
		Owner->SetActorTickEnabled(false);
		break;
	case EProgressFlagGateAction::RevealOwner:
		SetActorShown(*Owner, true);
		break;
	}
}
