// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.16.
* 열쇠로 여는 자물쇠 기믹 (레버와 같은 상호작용 기믹, AInteractableBase 파생)
* - 열쇠 아이템을 가진 플레이어가 상호작용하면 켜짐(풀림) → LinkedActors에 신호 (레버로 문을 여는 것과 같은 구조)
* - 열쇠가 없으면 덜컹 소리만 냄. 한 번 풀리면 다시 잠기지 않음
* - 풀리는 순간 연출이 필요하면 LinkedActors를 비우고 OnActivationChanged를 구독 (예: Stage3 대문 열림 컷)
*
* 설정
*   RequiredKey      풀 때 필요한 열쇠 아이템 (소모하지 않음)
*   LockedSound      열쇠 없이 상호작용할 때 소리 / 풀릴 때 소리는 베이스의 Activate Sound
*   안내 문구        열쇠가 있을 때 UnlockPromptText, 없을 때 LockedPromptText
*   감지 영역·위치    InteractionVolume, InteractPromptAnchor 컴포넌트 (다른 상호작용 기믹과 같은 규칙)
*/

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"

#include "GimmickKeyLock.generated.h"

class UItemData;
class USoundBase;

UCLASS()
class MYNICELITTLECAT_API AGimmickKeyLock : public AInteractableBase
{
	GENERATED_BODY()

public :

	// 한 번만 켜지는 상호작용 기믹으로 설정
	AGimmickKeyLock();

	/* IInteractableInterface */

	// 잠겨 있는 동안만 상호작용 가능 (열쇠가 없어도 확인은 가능)
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;

	// 열쇠가 있으면 풀림(켜짐 + 신호), 없으면 덜컹 소리
	virtual void Interact_Implementation(AActor* InteractInstigator) override;

	// 열쇠 보유 여부에 따라 안내 문구 선택 (위치는 베이스 규칙)
	virtual FInteractPromptInfo GetInteractPrompt_Implementation(AActor* InteractInstigator) const override;

	// Getter: 풀렸는지 (켜짐 = 풀림)
	UFUNCTION(BlueprintPure, Category = "Gimmick|KeyLock")
	bool IsUnlocked() const { return IsActivated(); }

protected :

	// 상호작용한 플레이어가 열쇠를 가지고 있는지 (열쇠가 지정되지 않았으면 false)
	bool HasRequiredKey() const;

protected :

	// 풀 때 필요한 열쇠 아이템 (소모하지 않음: 다시 들어와도 같은 열쇠로 풀 수 있음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|KeyLock")
	TObjectPtr<UItemData> RequiredKey;

	// 풀 때 필요한 RequiredKey 개수 (예: Stage1·Stage2 보상 열쇠 2개)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|KeyLock", meta = (ClampMin = "1"))
	int32 RequiredKeyCount = 1;

	// 열쇠 없이 상호작용할 때 소리 (덜컹)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|KeyLock")
	TObjectPtr<USoundBase> LockedSound;

	// 안내 문구: 열쇠가 없을 때 / 열쇠가 있을 때
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|KeyLock")
	FText LockedPromptText = NSLOCTEXT("GimmickKeyLock", "Locked", "잠겨 있음");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|KeyLock")
	FText UnlockPromptText = NSLOCTEXT("GimmickKeyLock", "Unlock", "열쇠 사용");
};
