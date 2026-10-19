/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 체크포인트 액터 구현

#include "Gimmicks/Checkpoint/Checkpoint.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"

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
}

bool ACheckpoint::CanInteract_Implementation(AActor* InteractInstigator) const
{
	// 활성 상태와 관계없이 다시 저장할 수 있게 토글 규칙은 쓰지 않음
	return IsInteractionEnabled();
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
