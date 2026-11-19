// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmicks/Stage/GimmickKeyLock.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

#include "Items/InventorySubsystem.h"

AGimmickKeyLock::AGimmickKeyLock()
{
	// 한 번 풀리면 다시 잠기지 않음: 토글 옵션은 디테일에서 숨김
	bToggleable = false;
	bShowToggleableOption = false;

	// 받은 신호로 풀리지 않음: 열쇠로만 풀림 (연출이 강제로 풀어야 하면 SetActivated 사용)
	bReceiveSignals = false;
}

/* IInteractableInterface */

bool AGimmickKeyLock::CanInteract_Implementation(AActor* InteractInstigator) const
{
	// 플레이어가 조종하는 Pawn만, 잠겨 있는 동안만
	const APawn* Pawn = Cast<APawn>(InteractInstigator);
	return Pawn && Pawn->IsPlayerControlled()
		&& Super::CanInteract_Implementation(InteractInstigator);
}

void AGimmickKeyLock::Interact_Implementation(AActor* InteractInstigator)
{
	// BP에서 CanInteract를 덮어쓴 경우도 따르도록 Execute로 확인
	if (!IInteractableInterface::Execute_CanInteract(this, InteractInstigator))
	{
		return;
	}

	// 열쇠가 없으면 확인만: 덜컹 소리
	if (!HasRequiredKey())
	{
		PlayGimmickSound(LockedSound);
		return;
	}

	// 풀림: 켜짐 상태로 바꾸면 베이스가 Activate Sound 재생, OnActivationChanged 방송, LinkedActors에 신호 전달
	SetActivated(true, InteractInstigator);
}

FInteractPromptInfo AGimmickKeyLock::GetInteractPrompt_Implementation(AActor* InteractInstigator) const
{
	// 위치와 표시 여부는 베이스 규칙 (InteractPromptAnchor, PromptOffset, bShowPrompt)
	FInteractPromptInfo Prompt = Super::GetInteractPrompt_Implementation(InteractInstigator);
	Prompt.ActionText = HasRequiredKey() ? UnlockPromptText : LockedPromptText;
	return Prompt;
}

/* 내부 처리 */

bool AGimmickKeyLock::HasRequiredKey() const
{
	if (!RequiredKey)
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UInventorySubsystem* Inventory = GameInstance ? GameInstance->GetSubsystem<UInventorySubsystem>() : nullptr;
	return Inventory && Inventory->HasItem(RequiredKey, FMath::Max(1, RequiredKeyCount));
}
