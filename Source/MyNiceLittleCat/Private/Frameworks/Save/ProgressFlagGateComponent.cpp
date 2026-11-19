#include "Frameworks/Save/ProgressFlagGateComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "GameFramework/Actor.h"

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

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UGameProgressSubsystem* Progress = GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr;
	if (!Progress)
		return;

	Progress->OnProgressFlagChanged.AddUniqueDynamic(this, &UProgressFlagGateComponent::HandleProgressFlagChanged);
	Evaluate();
}

void UProgressFlagGateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UGameProgressSubsystem* Progress = GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr)
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

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UGameProgressSubsystem* Progress = GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr;
	if (!Progress)
		return;

	if (Progress->HasProgressFlag(Flag) != bActWhenFlagSet)
	{
		// 등장형: 조건 전에는 숨겨 둠
		if (Action == EProgressFlagGateAction::RevealOwner)
		{
			if (AActor* Owner = GetOwner())
			{
				Owner->SetActorHiddenInGame(true);
				Owner->SetActorEnableCollision(false);
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
		Owner->SetActorHiddenInGame(true);
		Owner->SetActorEnableCollision(false);
		Owner->SetActorTickEnabled(false);
		break;
	case EProgressFlagGateAction::RevealOwner:
		Owner->SetActorHiddenInGame(false);
		Owner->SetActorEnableCollision(true);
		break;
	}
}
