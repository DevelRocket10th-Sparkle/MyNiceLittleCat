/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 보스 레이저 테스트용 발사기입니다.
/// 보스와 보스 공격 GA가 준비되기 전에 ULaserBeamComponent를 대신 실행합니다.
/// 목표는 TargetActor(레벨 인스턴스에서 지정), 비어 있으면 플레이어 폰의 현재 위치입니다.
/// bTrackTarget이면 발사 중 매 틱 SetTargetLocation으로 목표를 따라갑니다 (GA의 추적 조준 흉내).
/// 테스트 키 (기본): F 발사/정지, G 현재 목표로 다시 조준, H 추적 켜기/끄기. 맞은 액터·누적 피해는 화면 디버그 메시지로 표시합니다.
/// 레이저 수치(사거리·굵기·피해·회전 따라가기)는 Laser 컴포넌트(ULaserBeamComponent) 디테일에서 설정합니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Test_LaserEmitter.generated.h"

class UStaticMeshComponent;
class ULaserBeamComponent;

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

protected:
	virtual void BeginPlay() override;

	AActor* GetTargetActor() const;

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

private:
	float TotalDamage = 0.f;
};
