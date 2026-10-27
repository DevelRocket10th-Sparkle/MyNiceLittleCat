// 작성자: 우진
// 작성일자: 2026-10-27
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ActivatableInterface.h"
#include "TutorialGate.generated.h"

UCLASS()
class MYNICELITTLECAT_API ATutorialGate : public AActor, public IActivatableInterface
{
	GENERATED_BODY()
	
public:	
	ATutorialGate();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	// =======================================================================
public:
	// 문 열기 / 닫기 (OpenDuration 동안 부드럽게 회전)
	UFUNCTION(BlueprintCallable, Category = "Tutorial|Gate")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "Tutorial|Gate")
	void Close();

	// 열려 있거나 열리는 중이면 true
	UFUNCTION(BlueprintPure, Category = "Tutorial|Gate")
	bool IsOpen() const { return bWantOpen; }

	// IActivatableInterface
	virtual void Activate_Implementation(AActor* Activator) override { Open(); }
	virtual void Deactivate_Implementation(AActor* Activator) override { Close(); }

protected:
	// 열린 정도(0-1)를 문짝 회전에 반영
	void ApplyDoorRotation();
	// =======================================================================
protected:
	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Gate")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Gate")
	TObjectPtr<class UStaticMeshComponent> FrameMesh; // 문틀 (움직이지 않음)

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Gate")
	TObjectPtr<UStaticMeshComponent> LeftDoorMesh; // 왼쪽 문짝 (피벗 = 경첩)

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Gate")
	TObjectPtr<UStaticMeshComponent> RightDoorMesh; // 오른쪽 문짝 (피벗 = 경첩)

	UPROPERTY(EditAnywhere, Category = "Tutorial|Gate")
	bool bStartOpen = false; // 게임 시작 시 열려 있을지

	UPROPERTY(EditAnywhere, Category = "Tutorial|Gate", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float OpenAngle = 90.f; // 다 열렸을 때 문짝 회전 각도. 반대로 열리면 음수 대신 아래 bOpenInverted 사용

	UPROPERTY(EditAnywhere, Category = "Tutorial|Gate")
	bool bOpenInverted = false; // 문이 반대 방향으로 열리면 체크

	UPROPERTY(EditAnywhere, Category = "Tutorial|Gate", meta = (ClampMin = "0.01"))
	float OpenDuration = 1.f; // 열리거나 닫히는 데 걸리는 시간(초)

private:
	bool bWantOpen = false; // 목표 상태 (열기 요청 = true)
	float OpenAlpha = 0.f;  // 현재 열린 정도. 0 = 닫힘, 1 = 다 열림
};
