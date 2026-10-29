/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 버튼 구현

#include "Gimmicks/Stage/GimmickButton.h"
#include "Gimmicks/GimmickMoverComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "TimerManager.h"

AGimmickButton::AGimmickButton()
{
	// 버튼 디테일에서는 베이스 Toggleable을 숨기고 ButtonMode로 정함
	bShowToggleableOption = false;
	ApplyButtonMode();

	ButtonMover = CreateDefaultSubobject<UGimmickMoverComponent>(TEXT("ButtonMover"));
	ButtonMover->SetupAttachment(Mesh);
	// 정면(+X)을 향한 버튼이 안쪽으로 눌림
	ButtonMover->OpenOffset = FVector(-4.f, 0.f, 0.f);
	ButtonMover->OpenDuration = 0.1f;

	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	ButtonMesh->SetupAttachment(ButtonMover);
	ButtonMesh->SetMobility(EComponentMobility::Movable);
	ButtonMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void AGimmickButton::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyButtonMode();
}

void AGimmickButton::BeginPlay()
{
	ApplyButtonMode();
	Super::BeginPlay();
}

void AGimmickButton::ApplyButtonMode()
{
	// Once: 켜지면 잠김 / Toggle: 다시 눌러 끄기 / Timed: CanInteract에서 따로 처리
	bToggleable = ButtonMode == EGimmickButtonMode::Toggle;
}

void AGimmickButton::Interact_Implementation(AActor* InteractInstigator)
{
	if (ButtonMode != EGimmickButtonMode::Timed)
	{
		Super::Interact_Implementation(InteractInstigator);
		return;
	}

	if (!IInteractableInterface::Execute_CanInteract(this, InteractInstigator))
	{
		return;
	}

	SetActivated(true, InteractInstigator);
}

bool AGimmickButton::CanInteract_Implementation(AActor* InteractInstigator) const
{
	if (ButtonMode == EGimmickButtonMode::Timed)
	{
		// 시간 제한 버튼은 꺼진 뒤에만 다시 누름
		return IsInteractionEnabled() && !IsActivated();
	}
	return Super::CanInteract_Implementation(InteractInstigator);
}

float AGimmickButton::GetRemainingActiveTime() const
{
	return GetWorldTimerManager().IsTimerActive(ActiveTimer) ? GetWorldTimerManager().GetTimerRemaining(ActiveTimer) : 0.f;
}

void AGimmickButton::HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator)
{
	Super::HandleActivationChanged(bNewActivated, ChangeInstigator);

	GetWorldTimerManager().ClearTimer(ActiveTimer);
	if (bNewActivated && ButtonMode == EGimmickButtonMode::Timed && ActiveDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(ActiveTimer, this, &AGimmickButton::HandleActiveTimeExpired, ActiveDuration, false);
	}
}

void AGimmickButton::HandleActiveTimeExpired()
{
	SetActivated(false, this);
}
