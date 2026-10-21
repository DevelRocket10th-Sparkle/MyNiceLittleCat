/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 보스 레이저 테스트용 발사기입니다.
/// 보스와 보스 공격 GA가 준비되기 전에 ULaserBeamComponent를 대신 실행합니다.
/// 목표는 TargetActor(레벨 인스턴스에서 지정), 비어 있으면 플레이어 폰의 현재 위치입니다.
/// bTrackTarget이면 발사 중 매 틱 SetTargetLocation으로 목표를 따라갑니다 (GA의 추적 조준 흉내).
/// 테스트 키 (기본): F 발사/정지, G 현재 목표로 다시 조준, H 추적 켜기/끄기, J 바닥 일격. 맞은 액터·누적 피해는 화면 디버그 메시지로 표시합니다.
/// 레이저 수치(사거리·굵기·피해·회전 따라가기)는 Laser 컴포넌트(ULaserBeamComponent) 디테일에서 설정합니다.
/// 바닥 일격(FloorStrike, 보스 레이저 GA 흉내): 목표 발밑 바닥 지점을 그 순간 고정 → 경고 데칼(AAttackWarningDecal) 표시
/// → StrikeWarningTime 뒤 그 지점으로 StrikeDuration 동안 발사. 내리쏘려면 발사기를 높이 두고 Laser의 bPlanarAim을 꺼야 합니다.
/// bAutoStrike면 AutoStrikeInterval마다 자동으로 일격합니다 (키 입력 없이 확인용).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Test_LaserEmitter.generated.h"

class UStaticMeshComponent;
class ULaserBeamComponent;
class AAttackWarningDecal;

UCLASS()
class MYNICELITTLECAT_API ATest_LaserEmitter : public AActor
{
	GENERATED_BODY()

public:
	ATest_LaserEmitter();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "LaserBeam|Test")
	void ToggleLaser();

	// 현재 목표 위치로 다시 발사 (발사 중이 아니어도 발사)
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "LaserBeam|Test")
	void Retarget();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "LaserBeam|Test")
	void ToggleTracking();

	// 목표 발밑 바닥에 예고 후 발사 (진행 중인 일격은 취소하고 새로 시작)
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "LaserBeam|Test")
	void FloorStrike();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	AActor* GetTargetActor() const;

	// Target 위치에서 아래로 트레이스해 바닥 지점을 찾음
	bool FindFloorUnder(const AActor* Target, FVector& OutLocation) const;

	// 예고가 끝나면 고정된 지점으로 발사
	void FireStrike();

	void HideStrikeWarning();

	UFUNCTION()
	void HandleLaserDamaged(ULaserBeamComponent* InLaser, AActor* DamagedActor, float Damage);

protected:
	// 발사기 몸체 표시용 (콜리전 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	// 발사 위치 (위치는 BP에서 조정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserBeamComponent> Laser;

	// 조준할 액터 (비어 있으면 플레이어 폰)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LaserBeam|Test")
	TObjectPtr<AActor> TargetActor;

	// true면 BeginPlay에서 바로 발사
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test")
	bool bFireOnBeginPlay = false;

	// true면 발사 중 매 틱 목표를 다시 조준
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test")
	bool bTrackTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|Input")
	bool bBindTestKeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey ToggleLaserKey = EKeys::F;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey RetargetKey = EKeys::G;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey ToggleTrackingKey = EKeys::H;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey FloorStrikeKey = EKeys::J;

	// 바닥 일격 경고 데칼 (비어 있으면 경고 없이 대기 후 발사)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|FloorStrike")
	TSubclassOf<AAttackWarningDecal> StrikeWarningDecalClass;

	// 경고 표시 후 발사까지 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|FloorStrike", meta = (ClampMin = "0.0", Units = "s"))
	float StrikeWarningTime = 1.f;

	// 발사 지속 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|FloorStrike", meta = (ClampMin = "0.05", Units = "s"))
	float StrikeDuration = 1.5f;

	// 경고 반경 (빔 굵기 + 플레이어 캡슐 반경 정도면 원 안 = 맞는 범위)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|FloorStrike", meta = (ClampMin = "1.0", Units = "cm"))
	float StrikeWarningRadius = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|FloorStrike")
	FLinearColor StrikeWarningColor = FLinearColor(1.f, 0.05f, 0.f);

	// 바닥을 찾는 아래 방향 트레이스 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|FloorStrike", meta = (ClampMin = "1.0", Units = "cm"))
	float StrikeFloorTraceDistance = 2000.f;

	// true면 BeginPlay부터 AutoStrikeInterval마다 자동 일격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|FloorStrike")
	bool bAutoStrike = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserBeam|Test|FloorStrike", meta = (ClampMin = "0.5", Units = "s", EditCondition = "bAutoStrike"))
	float AutoStrikeInterval = 4.f;

private:
	float TotalDamage = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<AAttackWarningDecal> StrikeWarning;

	// 예고 시점에 고정한 발사 지점
	FVector StrikeLocation = FVector::ZeroVector;

	bool bStrikePending = false;

	FTimerHandle StrikeTimerHandle;

	FTimerHandle AutoStrikeTimerHandle;
};
