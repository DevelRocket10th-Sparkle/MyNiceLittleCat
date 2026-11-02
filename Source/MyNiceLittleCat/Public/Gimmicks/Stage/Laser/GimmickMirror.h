/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 레벨에 배치하는 거울입니다 (스테이지 2 반사 퍼즐). 반사형 레이저(ULaserBeamComponent bEnableReflection)를 반사합니다.
/// 반사는 MirrorSurface 박스의 정면(+X)으로 들어온 빛만 하고(펫 반사판과 같은 규칙), 뒷면은 빛을 막습니다. bDoubleSided면 양면 반사.
/// 회전 방식(RotateMode):
///   Fixed    = 고정
///   Interact = E로 상호작용할 때마다 RotateStep만큼 한 방향으로 돎 (기본 45°, 8번이면 한 바퀴 → 언제든 정답 각도에 도달)
///   Signal   = 다른 기믹의 신호가 켜질 때마다 RotateStep만큼 돎 (레버로 돌리는 거울 등)
/// 도는 동안(RotateDuration)에도 반사는 계속되며, 다시 상호작용할 수 없습니다.
/// 거울의 켜짐 상태는 쓰지 않습니다 (회전 각도는 세이브에 저장하지 않음, 퍼즐 결과는 Latch 수신기가 저장).
/// Mesh(루트) = 받침, MirrorPivot(회전, 받침 스케일 무시) > MirrorSurface(반사 판정 박스) > MirrorMesh(보이는 거울).
/// 반사 판정 박스는 스테이지 레이저 높이(60cm)를 덮도록 기본 높이 0~120cm. 메시는 BP에서 지정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "Interfaces/ReflectiveSurfaceInterface.h"
#include "GimmickMirror.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EGimmickMirrorRotateMode : uint8
{
	// 고정
	Fixed,
	// E로 상호작용할 때마다 회전
	Interact,
	// 신호가 켜질 때마다 회전
	Signal
};

UCLASS()
class MYNICELITTLECAT_API AGimmickMirror : public AInteractableBase, public IReflectiveSurfaceInterface
{
	GENERATED_BODY()

public:
	AGimmickMirror();

	virtual void Tick(float DeltaTime) override;

	// IInteractableInterface
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;

	// IReflectiveSurfaceInterface
	virtual bool TryGetReflection_Implementation(const FVector& IncomingDirection, const FHitResult& Hit, FVector& OutDirection) const override;

	// RotateStep만큼 회전 시작 (도는 중이면 무시)
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Mirror")
	void RotateOnce();

	UFUNCTION(BlueprintPure, Category = "Gimmick|Mirror")
	bool IsRotating() const { return bRotating; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void HandleSignalChanged(AActor* Source, bool bSignalOn) override;

	void ApplyRotateMode();

	// 회전 시작 / 끝 (BP 연출용: 소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Mirror", meta = (DisplayName = "On Rotate Started"))
	void K2_OnRotateStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Mirror", meta = (DisplayName = "On Rotate Finished"))
	void K2_OnRotateFinished();

protected:
	// 회전 중심 (받침 위에서 Z축으로 돎)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> MirrorPivot;

	// 반사 판정 박스 (정면 +X가 반사면). 빔을 막도록 BlockAllDynamic
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> MirrorSurface;

	// 보이는 거울 메시 (콜리전 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MirrorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Mirror")
	EGimmickMirrorRotateMode RotateMode = EGimmickMirrorRotateMode::Interact;

	// 한 번에 도는 각도 (양수 = 위에서 볼 때 시계 방향)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Mirror", meta = (Units = "deg", EditCondition = "RotateMode != EGimmickMirrorRotateMode::Fixed"))
	float RotateStep = 45.f;

	// 한 번 도는 데 걸리는 시간 (0이면 즉시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Mirror", meta = (ClampMin = "0.0", Units = "s", EditCondition = "RotateMode != EGimmickMirrorRotateMode::Fixed"))
	float RotateDuration = 0.25f;

	// 양면 반사 (꺼져 있으면 뒷면은 빛을 막음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Mirror")
	bool bDoubleSided = false;

private:
	bool bRotating = false;

	float RotateElapsed = 0.f;

	float RotateStartYaw = 0.f;

	float RotateTargetYaw = 0.f;
};
