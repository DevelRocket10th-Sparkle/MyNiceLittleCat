/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 체크포인트 액터 구현

#include "Gimmicks/Checkpoint/Checkpoint.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Frameworks/Transition/ScreenTransitionSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ACheckpoint::ACheckpoint()
{
	RespawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(Mesh);
	RespawnPoint->SetRelativeLocation(FVector(150.f, 0.f, 0.f));
	RespawnPoint->ArrowColor = FColor::Green;
}

void ACheckpoint::BeginPlay()
{
	Super::BeginPlay();

	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		Progress->OnCheckpointChanged.AddDynamic(this, &ACheckpoint::HandleCheckpointChanged);

		// 로드된 세이브의 체크포인트면 시작부터 활성 표시
		SetActivated(Progress->IsCurrentCheckpoint(this, GetCheckpointId()), this);
	}
}

void ACheckpoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGameProgressSubsystem* Progress = GameInstance->GetSubsystem<UGameProgressSubsystem>())
		{
			Progress->OnCheckpointChanged.RemoveDynamic(this, &ACheckpoint::HandleCheckpointChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ACheckpoint::Interact_Implementation(AActor* InteractInstigator)
{
	if (!IInteractableInterface::Execute_CanInteract(this, InteractInstigator))
	{
		return;
	}

	UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	if (!Progress)
	{
		return;
	}

	// 등록하면 OnCheckpointChanged로 이 체크포인트는 켜지고 이전 체크포인트는 꺼짐
	Progress->SetCheckpoint(this, GetCheckpointId(), GetRespawnTransform(), bSaveOnInteract);
	K2_OnCheckpointInteracted(InteractInstigator);

	if (bPlayRestTransition)
	{
		PlayRestTransition(InteractInstigator);
	}
}

void ACheckpoint::PlayRestTransition(AActor* InteractInstigator)
{
	const APawn* InstigatorPawn = Cast<APawn>(InteractInstigator);
	APlayerController* PlayerController = InstigatorPawn ? Cast<APlayerController>(InstigatorPawn->GetController()) : nullptr;
	UScreenTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UScreenTransitionSubsystem>();
	if (!PlayerController || !Transition)
	{
		return;
	}

	// 연출 도중 체크포인트나 플레이어가 사라질 수 있어 약한 참조로 넘김
	TWeakObjectPtr<ACheckpoint> WeakThis(this);
	TWeakObjectPtr<AActor> WeakInstigator(InteractInstigator);

	Transition->PlayTransition(PlayerController, RestTransition,
		[WeakThis, WeakInstigator]()
		{
			if (ACheckpoint* Checkpoint = WeakThis.Get())
			{
				Checkpoint->K2_OnRestScreenDark(WeakInstigator.Get());
			}
		},
		[WeakThis, WeakInstigator]()
		{
			if (ACheckpoint* Checkpoint = WeakThis.Get())
			{
				Checkpoint->K2_OnRestFinished(WeakInstigator.Get());
			}
		});
}

bool ACheckpoint::CanInteract_Implementation(AActor* InteractInstigator) const
{
	// 활성 상태와 관계없이 다시 저장할 수 있게 토글 규칙은 쓰지 않음
	if (!IsInteractionEnabled())
	{
		return false;
	}

	// 휴식 연출 등 화면 전환 중에는 다시 상호작용하지 않음
	const UScreenTransitionSubsystem* Transition = GetWorld() ? GetWorld()->GetSubsystem<UScreenTransitionSubsystem>() : nullptr;
	return !Transition || !Transition->IsTransitionPlaying();
}

void ACheckpoint::HandleCheckpointChanged(FName NewCheckpointId)
{
	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		SetActivated(Progress->IsCurrentCheckpoint(this, GetCheckpointId()), this);
	}
}

FName ACheckpoint::GetCheckpointId() const
{
	return CheckpointId.IsNone() ? GetFName() : CheckpointId;
}

FTransform ACheckpoint::GetRespawnTransform() const
{
	return FTransform(FRotator(0.f, RespawnPoint->GetComponentRotation().Yaw, 0.f), RespawnPoint->GetComponentLocation());
}
