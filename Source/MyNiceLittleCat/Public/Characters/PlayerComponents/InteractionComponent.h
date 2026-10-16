/// 작성자: 최요환
/// 작성일자: 2026-10-16
///
/// 플레이어용 상호작용 컴포넌트입니다.
/// 주변의 IInteractableInterface 구현 액터 중 상호작용 대상 1개를 골라 포커스하고,
/// TryInteract() 호출 시 대상이 요청한 정렬(이동·회전)을 마친 뒤 Interact를 실행합니다.
/// 대상 선정: 반경 + 캐릭터 정면 각도 + (선택) 시야 트레이스를 통과한 후보 중 점수가 가장 높은 액터
/// InteractionVolume 컴포넌트가 있는 액터는 반경 대신 캐릭터 캡슐이 그 볼륨에 닿아 있어야 후보가 됩니다.
/// 사용법: 캐릭터에 컴포넌트를 추가하고, 상호작용 입력(E, IA_Interact)에서 TryInteract()만 호출하면 됩니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/InteractableInterface.h"
#include "InteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractFocusChanged, AActor*, NewFocus, AActor*, OldFocus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractExecuted, AActor*, Target);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 현재 포커스 대상과 상호작용 시도. 정렬이 필요하면 정렬 후 실행. 시작하지 못하면 false
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	// 정렬 중이면 취소 (Interact는 호출되지 않음)
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void CancelInteract();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	// 정렬 중(상호작용 실행 대기) 여부
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsAligning() const { return AlignTarget.IsValid(); }

public:
	// 포커스 대상이 바뀔 때 (안내 UI 등)
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractFocusChanged OnFocusChanged;

	// Interact를 실행한 직후
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractExecuted OnInteractExecuted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 주변 후보를 찾아 포커스 대상 갱신
	void RefreshFocus();
	AActor* FindBestCandidate() const;
	bool IsVisible(const AActor* Target, const FVector& TargetLocation) const;
	void SetFocusedActor(AActor* NewFocus);

	void BeginAlign(AActor* Target, const FInteractAlignment& Alignment);
	void FinishAlign(bool bExecute);
	void ExecuteInteract(AActor* Target);

	// 정렬 중 이동 입력 잠금/해제
	void SetMoveInputLocked(bool bLocked);

protected:
	// 대상 탐색 간격
	UPROPERTY(EditAnywhere, Category = "Interaction|Detect", meta = (ClampMin = "0.03"))
	float ScanInterval = 0.1f;

	// 상호작용 가능 반경 (InteractionVolume이 있는 액터는 볼륨 판정을 대신 사용, 탐색 범위로만 쓰임)
	UPROPERTY(EditAnywhere, Category = "Interaction|Detect", meta = (ClampMin = "0.0"))
	float InteractRadius = 200.f;

	// 캐릭터 정면 기준 허용 각도 (좌우 각각, 180이면 전방위)
	UPROPERTY(EditAnywhere, Category = "Interaction|Detect", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxFacingAngle = 90.f;

	// 점수 = 정면 가중치 * 정면도(-1~1) + 거리 가중치 * 가까움(0~1)
	UPROPERTY(EditAnywhere, Category = "Interaction|Detect", meta = (ClampMin = "0.0"))
	float FacingWeight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Interaction|Detect", meta = (ClampMin = "0.0"))
	float DistanceWeight = 1.f;

	// 시야 트레이스로 벽 너머 대상 제외
	UPROPERTY(EditAnywhere, Category = "Interaction|Detect")
	bool bUseVisibilityTrace = false;

	UPROPERTY(EditAnywhere, Category = "Interaction|Detect", meta = (EditCondition = "bUseVisibilityTrace"))
	TEnumAsByte<ECollisionChannel> VisibilityChannel = ECC_Visibility;

	// 이동 단계: 목표 위치에 이만큼 가까워지면 도착으로 보고 정확한 위치로 맞춤
	UPROPERTY(EditAnywhere, Category = "Interaction|Align", meta = (ClampMin = "1.0"))
	float MoveAcceptRadius = 5.f;

	// 이동 단계 입력 세기 (1 = 걷기 최고 속도, 낮추면 천천히 걸어감)
	UPROPERTY(EditAnywhere, Category = "Interaction|Align", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MoveInputScale = 0.6f;

	// 이동 단계: NavMesh 경로를 따라 장애물을 돌아감 (NavMesh가 없거나 경로가 끊기면 직선 이동)
	UPROPERTY(EditAnywhere, Category = "Interaction|Align")
	bool bUseNavPath = true;

	// 이동 단계: 경로 중간 지점에 이만큼 가까워지면 다음 지점으로 넘어감
	UPROPERTY(EditAnywhere, Category = "Interaction|Align", meta = (ClampMin = "1.0", EditCondition = "bUseNavPath"))
	float PathPointAcceptRadius = 30.f;

	// 이동 단계 제한 시간 = 경로 길이 / (최고 속도 × 입력 세기) + 이 여유 시간 (가속, 벽 미끄러짐 흡수)
	UPROPERTY(EditAnywhere, Category = "Interaction|Align", meta = (ClampMin = "0.0"))
	float MoveTimeoutPadding = 0.5f;

	// 제한 시간 안에 도착하지 못하면 상호작용 취소. 끄면 그 자리에서 회전 후 상호작용 실행
	UPROPERTY(EditAnywhere, Category = "Interaction|Align")
	bool bCancelOnMoveFail = true;

	// 회전 단계 회전 속도 (도/초)
	UPROPERTY(EditAnywhere, Category = "Interaction|Align", meta = (ClampMin = "1.0"))
	float RotateSpeed = 540.f;

	// 정렬 중 이동 입력 잠금
	UPROPERTY(EditAnywhere, Category = "Interaction|Align")
	bool bLockMoveInputWhileAligning = true;

	// 정렬 목표 위치·방향 디버그 표시
	UPROPERTY(EditAnywhere, Category = "Interaction|Debug")
	bool bDrawDebugAlign = false;

private:
	// 정렬 단계: 걸어서 이동 → 도착 후 제자리 회전
	enum class EAlignPhase : uint8
	{
		None,
		Move,
		Rotate
	};

	void TickMove(float DeltaTime);
	void TickRotate(float DeltaTime);

	// 이동 경로(경유 지점 + 목표)와 제한 시간 계산
	void BuildMovePath(const FVector& Start, const FVector& Goal);

private:
	TWeakObjectPtr<AActor> FocusedActor;

	// 정렬 상태 (AlignTarget이 유효하면 정렬 중)
	TWeakObjectPtr<AActor> AlignTarget;
	FInteractAlignment CurrentAlignment;
	EAlignPhase AlignPhase = EAlignPhase::None;
	float PhaseElapsed = 0.f;
	bool bMoveInputLocked = false;

	// 이동 단계 경로 (마지막 점이 정렬 목표 위치)
	TArray<FVector> MovePathPoints;
	int32 MovePathIndex = 0;
	float MoveTimeLimit = 0.f;

	FTimerHandle ScanTimerHandle;
};
