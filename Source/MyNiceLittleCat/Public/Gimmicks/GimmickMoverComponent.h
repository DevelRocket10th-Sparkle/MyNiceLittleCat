/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 기믹의 움직이는 부분(문짝, 레버 손잡이, 버튼, 발판)을 닫힘/열림 두 위치 사이로 움직이는 컴포넌트입니다.
/// 에디터에서 배치한 상대 위치·회전이 닫힘 상태이고, 열림 상태는 OpenOffset·OpenRotation만큼 더한 위치입니다.
/// 이 컴포넌트 아래에 메시·이펙트를 붙이면 같이 움직입니다 (스테이지마다 BP에서 메시만 교체).
/// bFollowActivation이면 AGimmickActorBase가 켜짐 상태에 맞춰 자동으로 SetOpen을 호출합니다.
/// 움직임 시작·끝 이벤트(OnMoveStarted/OnMoveFinished)에 소리·이펙트를 연결합니다. 즉시 이동(시작 상태 복원)은 이벤트를 보내지 않습니다.
/// 에디터 뷰포트에는 기준점 표시가 붙습니다 (게임에서는 만들지 않음):
///   주황 원·세로선 = 회전 중심(이 컴포넌트 위치)과 위(Z = 문 경첩 축) / 흰 화살표 = 닫힘 정면(X) / 초록 화살표 = 열렸을 때 정면(X) 방향과 위치
///   메시 안에 있어도 보이도록 맨 앞에 그립니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GimmickMoverComponent.generated.h"

class UCurveFloat;
class UGimmickMoverComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGimmickMoverEvent, UGimmickMoverComponent*, Mover, bool, bOpen);

// UGimmickMoverComponent의 기준점을 에디터 뷰포트에 그리는 표시 전용 컴포넌트 (메시 안에 있어도 보이게 맨 앞에 그림)
// Mover가 에디터 월드에서만 만들어 붙이며, 직접 추가해서 쓰지 않습니다.
UCLASS(NotBlueprintable, Transient, HideCategories = (Object, Collision, Physics, Rendering, Lighting, Navigation))
class MYNICELITTLECAT_API UGimmickPivotVisualizerComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UGimmickPivotVisualizerComponent();

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	// 열림 위치·회전 (이 컴포넌트 기준)
	FTransform OpenRelativeTransform;
	float DrawScale = 1.f;
};

UCLASS(ClassGroup = (Gimmick), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UGimmickMoverComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UGimmickMoverComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 열림/닫힘 위치로 이동 (bInstant면 바로 이동, 이벤트 없음). 움직이는 중에 반대로 바꾸면 그 자리에서 되돌아감
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Mover")
	void SetOpen(bool bOpen, bool bInstant = false);

	// 목표 상태 (움직이는 중이면 향하는 쪽)
	UFUNCTION(BlueprintPure, Category = "Gimmick|Mover")
	bool IsOpen() const { return bTargetOpen; }

	UFUNCTION(BlueprintPure, Category = "Gimmick|Mover")
	bool IsMoving() const { return bMoving; }

	// 0 = 닫힘, 1 = 열림
	UFUNCTION(BlueprintPure, Category = "Gimmick|Mover")
	float GetProgress() const { return Progress; }

public:
	// bOpen = 향하는 쪽
	UPROPERTY(BlueprintAssignable, Category = "Gimmick|Mover")
	FOnGimmickMoverEvent OnMoveStarted;

	// bOpen = 도착한 상태
	UPROPERTY(BlueprintAssignable, Category = "Gimmick|Mover")
	FOnGimmickMoverEvent OnMoveFinished;

	// 소유 기믹의 켜짐 상태를 따라 움직임
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gimmick|Mover")
	bool bFollowActivation = true;

	// 열림 위치 = 닫힘 위치 + 이 값 (부모 컴포넌트 기준)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gimmick|Mover")
	FVector OpenOffset = FVector::ZeroVector;

	// 열림 회전 = 닫힘 회전에 이 값을 더함 (컴포넌트 자신 기준, 회전 중심 = 이 컴포넌트 위치)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gimmick|Mover")
	FRotator OpenRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gimmick|Mover", meta = (ClampMin = "0.0", Units = "s"))
	float OpenDuration = 0.5f;

	// 음수면 OpenDuration과 같음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gimmick|Mover", meta = (Units = "s"))
	float CloseDuration = -1.f;

	// 진행도(0~1) → 이동 비율. 비면 부드럽게 시작·정지 (SmoothStep)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gimmick|Mover")
	TObjectPtr<UCurveFloat> MoveCurve;

	// 에디터 뷰포트에 회전 중심·닫힘/열림 방향 표시
	UPROPERTY(EditAnywhere, Category = "Gimmick|Mover|Editor")
	bool bShowPivotInEditor = true;

	UPROPERTY(EditAnywhere, Category = "Gimmick|Mover|Editor", meta = (ClampMin = "0.1", EditCondition = "bShowPivotInEditor"))
	float PivotArrowSize = 1.f;

protected:
	virtual void BeginPlay() override;
	virtual void OnRegister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void CaptureClosedTransform();
	void ApplyProgress();

#if WITH_EDITORONLY_DATA
	// 에디터 표시용 컴포넌트 생성·갱신·제거
	void UpdatePivotVisualization();
	void DestroyPivotVisualization();

	UPROPERTY(Transient)
	TObjectPtr<UGimmickPivotVisualizerComponent> PivotVisualizer;
#endif

private:
	FTransform ClosedTransform;
	bool bHasClosedTransform = false;

	float Progress = 0.f;
	bool bTargetOpen = false;
	bool bMoving = false;
};
