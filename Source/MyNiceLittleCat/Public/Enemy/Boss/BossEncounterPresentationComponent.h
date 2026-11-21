// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20. (ABossEncounterManager에서 분리)
* 보스방 전투의 화면·소리 표시 담당 (보스방 매니저의 기본 컴포넌트)
* - 보스 체력바: 플레이어 HUD(AMainHUD)에 표시/숨김
* - 보스 BGM: BGM 서브시스템의 이벤트 스택에 Push/Pop (Owner = 보스방 매니저)
* - 매니저는 전투 진행 상태만 관리하고, HUD와 BGM 시스템은 이 컴포넌트만 앎
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossEncounterPresentationComponent.generated.h"

class ABossCharacter;
class AMainHUD;
class APlayerCharacter;
class USoundBase;

UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UBossEncounterPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public :

	UBossEncounterPresentationComponent();

	/* 체력바 */

	// 플레이어 HUD에 보스 체력바 표시 (이전 전투의 체력바가 남아 있으면 먼저 숨김)
	void ShowHealthBar(const APlayerCharacter* Player, ABossCharacter* Boss);

	// 표시한 체력바 숨김
	void HideHealthBar();

	/* BGM */

	// 보스 곡으로 전환 (같은 Owner로 다시 Push해도 중복 없이 교체됨)
	void PushBGM(USoundBase* Sound);

	// 보스 곡 종료 → 존 곡으로 복귀 (스택에 없으면 아무 일도 없어 여러 번 불러도 안전)
	void PopBGM();

public :

	// 보스 곡 전환 크로스페이드 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|BGM", meta = (ClampMin = "0.0", Units = "s"))
	float BGMFadeSeconds = 0.5f;

private :

	// 체력바를 표시한 HUD (숨길 때 사용)
	TWeakObjectPtr<AMainHUD> EncounterHUD;
};
