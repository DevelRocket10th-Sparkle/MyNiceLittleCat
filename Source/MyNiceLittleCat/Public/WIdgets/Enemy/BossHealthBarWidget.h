// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHealthBarWidget.generated.h"

class ABossCharacter;
class UEnemyAttributeSet;
class UResourceBarWidget;

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UBossHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public :
	
	// 체력바 초기화: 보스에게 연결할 때 구독 갱신 및 체력바 갱신 수행
	void InitializeBossHealthBar(ABossCharacter* Boss);
	
	// 현재 연결된 보스 델리게이트 해제
	void UnbindBoss();

	UFUNCTION(BlueprintCallable, Category = "Health Bar")
	void SetShowHealthText(bool bShow);

protected :

	virtual void NativeDestruct() override;

protected :

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar", meta = (DisplayName = "Show Health Text"))
	bool bShowHealthText = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UResourceBarWidget> HealthBar = nullptr;

private :

	// 현재 연결된 보스의 스탯
	TWeakObjectPtr<UEnemyAttributeSet> ObservedStats;
	
};
