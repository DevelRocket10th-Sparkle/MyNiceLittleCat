#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Systems/OcclusionGroupSubsystem.h"
#include "CameraOcclusionComponent.generated.h"

class UCameraComponent;
class ULockOnComponent;
class UPrimitiveComponent;

// 카메라와 플레이어 (및 락온 대상) 사이를 가리는 벽 / 천장을 면 단위로 숨기는 컴포넌트
// 판정은 일정 간격 Sphere Sweep, 숨김은 메인 / 깊이 패스 렌더링만 끄는 방식이라 그림자와 콜리전은 유지
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UCameraOcclusionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Tick 비활성화: 판정은 타이머로만 실행
	UCameraOcclusionComponent();

	// 숨긴 조각의 복구 정보
	struct FHiddenPiece
	{
		// 숨기기 전 메인 패스 렌더링 여부: 원래 꺼져 있던 조각을 켜지 않기 위함
		bool bWasRenderInMainPass = true;
		// 숨기기 전 깊이 패스 렌더링 여부: 메인 패스만 끄면 깊이는 남아 검은 면으로 보이므로 같이 끔
		bool bWasRenderInDepthPass = true;
		// 마지막으로 가림 판정된 시각
		float LastOccludedTime = 0.f;
	};

	// 가림 판정 간격 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "0.016"))
	float UpdateInterval = 0.05f;
	// 카메라에서 대상까지 Sweep하는 구의 반지름: 몸 일부만 가려도 잡히도록 캐릭터 폭 정도로 설정 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "1.0"))
	float SweepRadius = 40.f;
	// 대상 바로 앞에서 Sweep을 멈추는 거리: 대상 등 뒤에 붙은 벽이 잡히는 것 방지 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "0.0"))
	float TargetEndOffset = 40.f;
	// 대상 발 기준 높이: 바닥 / 천장 조각의 아랫면이 이보다 낮으면 숨기지 않음 (cm)
	// 같은 판이 아래층에서는 천장, 위층에서는 바닥이 되므로 대상보다 충분히 위에 있을 때만 천장으로 취급
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "0.0"))
	float CeilingMinHeight = 150.f;
	// 더 이상 가리지 않아도 이 시간 동안은 숨김 유지: 벽 경계에서 깜빡임 방지 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "0.0"))
	float RestoreDelay = 0.25f;
	// 락온 중인 대상 (보스 등)을 가리는 벽도 숨김
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion")
	bool bIncludeLockedTarget = true;
	// 조각을 같은 면으로 묶는 기준 및 가림 채널
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion")
	FOcclusionGroupSettings GroupSettings;
	// Sweep 경로 (노랑)와 맞은 조각 위치 (빨강) 표시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion|Debug")
	bool bDrawDebug = false;

	// Occlusion 실행 할지 말지 설정
	void SetOcclusionSuspended(bool bSuspended);

protected:
	// 카메라, 락온, 그룹 서브시스템 참조 확보 후 판정 타이머 시작
	virtual void BeginPlay() override;
	// 타이머 정리 및 숨긴 조각 전부 복구
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// 타이머 콜백: Sweep → 면 단위 확장 → 숨김 / 복구 순서로 처리
	void UpdateOcclusion();
	// 가림 판정 대상 수집: 플레이어 자신 + 락온 대상
	void CollectTargets(TArray<AActor*>& OutTargets) const;
	// 카메라에서 대상까지 Sweep하여 맞은 조각 수집
	void SweepToTarget(const FVector& CameraLocation, const AActor* Target, TSet<UPrimitiveComponent*>& OutOccludingPieces) const;
	// 바닥 / 천장 조각이 대상 발보다 CeilingMinHeight 이상 위에 있는지 판정. 벽 등 수직 조각은 항상 true
	bool IsAboveTargetFloor(const UPrimitiveComponent* Piece, float TargetFootZ) const;
	// 메인 패스 / 깊이 패스 렌더링을 꺼서 화면에서만 숨김
	void HidePiece(UPrimitiveComponent* Piece, float CurrentTime);
	// 숨기기 전 렌더링 설정으로 복구
	void RestorePiece(UPrimitiveComponent* Piece, const FHiddenPiece& Hidden);
	// 일정 시간 가리지 않은 조각 복구
	void RestoreExpiredPieces(float CurrentTime);
	// 숨긴 조각 전부 즉시 복구
	void RestoreAllPieces();

private:

	// 시점 기준 카메라: 락온으로 각도가 바뀌어도 실제 위치를 매번 사용
	TWeakObjectPtr<UCameraComponent> Camera;
	// 락온 대상 조회용
	TWeakObjectPtr<ULockOnComponent> LockOn;
	// 조각 → 면 그룹 조회용
	TWeakObjectPtr<UOcclusionGroupSubsystem> GroupSubsystem;
	// 현재 숨긴 조각 목록
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FHiddenPiece> HiddenPieces;
	// 판정 반복 타이머
	FTimerHandle UpdateTimerHandle;

	bool bOcclusionSuspended = false;
};
