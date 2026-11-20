// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Boss/BossEncounterPresentationComponent.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Characters/PlayerCharacter.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Frameworks/Sound/BGMSubSystem.h"
#include "Widgets/MainHUD.h"

UBossEncounterPresentationComponent::UBossEncounterPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* 체력바 */

void UBossEncounterPresentationComponent::ShowHealthBar(const APlayerCharacter* Player, ABossCharacter* Boss)
{
	// 이전 전투의 UI가 남아 있으면 먼저 정리
	HideHealthBar();

	if (!IsValid(Player) || !IsValid(Boss))
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Player->GetController());
	AMainHUD* HUD = PlayerController && PlayerController->IsLocalController() ? Cast<AMainHUD>(PlayerController->GetHUD()) : nullptr;
	if (!IsValid(HUD))
	{
		return;
	}

	EncounterHUD = HUD;
	HUD->ShowBossHealthBar(Boss);
}

void UBossEncounterPresentationComponent::HideHealthBar()
{
	if (AMainHUD* HUD = EncounterHUD.Get())
	{
		HUD->HideBossHealthBar();
	}

	EncounterHUD.Reset();
}

/* BGM */

void UBossEncounterPresentationComponent::PushBGM(USoundBase* Sound)
{
	if (nullptr == Sound)
	{
		return;
	}

	const UGameInstance* GI = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
	if (UBGMSubSystem* BGM = GI ? GI->GetSubsystem<UBGMSubSystem>() : nullptr)
	{
		// Owner = 보스방 매니저: 같은 Owner로 다시 Push해도 중복 없이 교체됨
		BGM->PushEventBGM(GetOwner(), Sound, BGMFadeSeconds);
	}
}

void UBossEncounterPresentationComponent::PopBGM()
{
	const UGameInstance* GI = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
	if (UBGMSubSystem* BGM = GI ? GI->GetSubsystem<UBGMSubSystem>() : nullptr)
	{
		// 스택에 없으면 아무 일도 안 일어나므로 여러 번 불러도 안전
		BGM->PopEventBGM(GetOwner(), BGMFadeSeconds);
	}
}
