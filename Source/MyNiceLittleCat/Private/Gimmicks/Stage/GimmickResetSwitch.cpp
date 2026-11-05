/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 되돌리기 스위치 구현

#include "Gimmicks/Stage/GimmickResetSwitch.h"
#include "Interfaces/GimmickResettableInterface.h"
#include "TimerManager.h"

AGimmickResetSwitch::AGimmickResetSwitch()
{
	// 켜짐 상태·신호를 쓰지 않음
	bShowToggleableOption = false;
	bReceiveSignals = false;
	bSaveState = false;
}

void AGimmickResetSwitch::Interact_Implementation(AActor* InteractInstigator)
{
	if (!IInteractableInterface::Execute_CanInteract(this, InteractInstigator))
	{
		return;
	}

	ResetTargets(InteractInstigator);

	if (ResetCooldown > 0.f)
	{
		GetWorldTimerManager().SetTimer(CooldownTimer, ResetCooldown, false);
	}
}

bool AGimmickResetSwitch::CanInteract_Implementation(AActor* InteractInstigator) const
{
	return IsInteractionEnabled() && !GetWorldTimerManager().IsTimerActive(CooldownTimer);
}

void AGimmickResetSwitch::ResetTargets(AActor* ResetInstigator)
{
	for (AActor* Target : Targets)
	{
		if (IsValid(Target) && Target->Implements<UGimmickResettableInterface>())
		{
			IGimmickResettableInterface::Execute_ResetGimmick(Target, ResetInstigator);
		}
	}

	K2_OnResetTriggered(ResetInstigator);
}

void AGimmickResetSwitch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CooldownTimer);
	Super::EndPlay(EndPlayReason);
}
