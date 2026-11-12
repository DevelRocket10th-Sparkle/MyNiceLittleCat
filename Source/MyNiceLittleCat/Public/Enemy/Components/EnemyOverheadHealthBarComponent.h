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

	// 체력 변경 시 위젯을 한 번 다시 그리도록 요청 (수동 다시 그리기 모드)
	void HandleHealthChanged(float NewHealth, float MaxHealth);

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

	/** 내용이 바뀔 때 (체력 변경, 텍스트 표시 변경)만 위젯을 렌더 타깃에 다시 그림
	 *  월드 스페이스 위젯은 기본적으로 매 프레임 다시 그려 Enemy 수만큼 렌더 스레드 비용이 늘어남
	 *  체력바는 애니메이션이 없어 값이 바뀔 때만 그려도 화면이 같음. 회전 (빌보드)은 다시 그리기와 무관 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar|Performance")
	bool bRedrawOnlyOnChange = true;

private:

	TWeakObjectPtr<AEnemyCharacter> OwnerEnemy;

	FDelegateHandle DeathStartedHandle;

	// 체력 변경 구독 핸들 (수동 다시 그리기 요청용)
	FDelegateHandle HealthChangedHandle;

	// 첫 레이아웃 / 폰트 로드 이후 한 번 더 그리기 위한 타이머
	FTimerHandle InitialRedrawTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyOverheadHealthBarWidget> HealthBarWidget;

};
