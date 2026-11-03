// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "EnemyOverheadHealthBarComponent.generated.h"

class AEnemyCharacter;
class UEnemyOverheadHealthBarWidget;

/**
 * 
 */
UCLASS(
	ClassGroup = (Enemy),
	meta = (
		BlueprintSpawnableComponent,
		DisplayName = "Overhead Health Bar Widget Component"
	)
)
class MYNICELITTLECAT_API UEnemyOverheadHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()
	
public :
	
	UEnemyOverheadHealthBarComponent();
	
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetShowHealthText(bool bShow);

protected :

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private :

	void HandleOwnerDeathStarted(AEnemyCharacter* Enemy);

	void UpdateWidgetRotation();

protected :

	// 위젯 BP의 기본값보다 컴포넌트 설정을 우선 적용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar", meta = (DisplayName = "Show Health Text"))
	bool bShowHealthText = true;

	/** 위젯이 항상 카메라를 바라보도록 회전할지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar")
	bool bFaceCamera = true;

	/** 빌보드 회전 시 상하 기울기(Pitch)를 고정(0)할지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar")
	bool bLockWidgetPitch = false;

	/** 빌보드 회전 시 좌우 기울기(Roll)를 고정(0)할지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar")
	bool bLockWidgetRoll = true;

private:

	TWeakObjectPtr<AEnemyCharacter> OwnerEnemy;

	FDelegateHandle DeathStartedHandle;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyOverheadHealthBarWidget> HealthBarWidget;

};
