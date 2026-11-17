// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.30.
* 수정 일자: 2026.11.17. (주석 정리)
* Enemy 머리 위 체력바 위젯 컴포넌트
* - 위젯(UEnemyOverheadHealthBarWidget)에 소유 Enemy의 체력을 연결하고, 카메라를 향해 회전
* - 사망 시작 시 숨김, 체력이 바뀔 때만 다시 그림 (bRedrawOnlyOnChange)
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "EnemyOverheadHealthBarComponent.generated.h"

class AEnemyCharacter;
class UEnemyOverheadHealthBarWidget;

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

	// 체력 숫자 표시 여부 변경 후 다시 그리기 요청
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetShowHealthText(bool bShow);

protected :

	// 소유 Enemy의 사망·체력 변경 구독, 위젯 초기화, 다시 그리기 모드 설정
	virtual void BeginPlay() override;

	// 구독 해제와 타이머 정리
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 보이는 동안 카메라를 향해 회전 (bFaceCamera)
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private :

	// 사망 시작: 체력바 숨김, Tick 중지
	void HandleOwnerDeathStarted(AEnemyCharacter* Enemy);

	// 플레이어 카메라 방향으로 회전 (Pitch/Roll 고정 옵션 반영)
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

	// 체력바를 표시할 Enemy
	TWeakObjectPtr<AEnemyCharacter> OwnerEnemy;

	// 사망 시작 구독 핸들
	FDelegateHandle DeathStartedHandle;

	// 체력 변경 구독 핸들 (수동 다시 그리기 요청용)
	FDelegateHandle HealthChangedHandle;

	// 첫 레이아웃 / 폰트 로드 이후 한 번 더 그리기 위한 타이머
	FTimerHandle InitialRedrawTimerHandle;

	// 생성된 체력바 위젯
	UPROPERTY(Transient)
	TObjectPtr<UEnemyOverheadHealthBarWidget> HealthBarWidget;

};
