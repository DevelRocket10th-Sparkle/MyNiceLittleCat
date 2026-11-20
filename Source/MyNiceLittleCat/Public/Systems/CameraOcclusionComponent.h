/*
* 작성자: 전선현
* 작성 일자: 2026.11.08.
* 수정 일자: 2026.11.20. (방 컷어웨이 후보 계산을 FRoomCutawayCache로 분리, 방 조각은 방이나 카메라 쪽 면이 바뀔 때만 다시 숨김)
* 카메라와 플레이어 사이를 가리는 벽, 지붕을 숨기는 컴포넌트 (플레이어에 부착)
* - 바깥: 카메라 → 플레이어 스윕으로 가리는 조각을 평면 그룹 단위로 숨김
* - 방 안: 현재 방(ARoomMaskVolume)의 위, 바깥, 카메라 쪽 옆면을 단면으로 숨기고 그림자 라이트를 줄임
*/
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Systems/OcclusionGroupSubsystem.h"
#include "Systems/RoomCutawayCache.h"
#include "CameraOcclusionComponent.generated.h"

class UCameraComponent;
class ULockOnComponent;
class UPrimitiveComponent;
class ULocalLightComponent;
class ARoomMaskVolume;

// 카메라와 플레이어 (및 락온 대상) 사이를 가리는 벽 / 천장을 면 단위로 숨기는 컴포넌트
// 1) 방 안: RoomMaskVolume 기준 컷어웨이만 사용 (태그 불필요, 방 배치만으로 동작)
//    가까이 간 가구 / 계단이 사라지지 않도록 방 안에서는 Sweep을 쓰지 않음
// 2) 방 밖: Sphere Sweep + 같은 평면에 이어진 조각 자동 그룹 (태그 불필요)
// 숨김은 메인 / 깊이 패스 렌더링만 끄는 방식이라 그림자와 콜리전은 유지
// 3) 성능: 방 안에서는 현재 방에 빛이 닿지 않는 조명의 그림자를 끔 (RoomCutaway.bCullOutsideLightShadows)
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
		// 현재 방 컷어웨이가 잡고 있는 조각: 방을 나가거나 카메라 쪽 면이 바뀔 때까지 복구하지 않음
		bool bHeldByRoom = false;
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
	// 방 단위 컷어웨이 기준
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion")
	FRoomCutawaySettings RoomCutaway;
	// Sweep 경로 (노랑)와 맞은 조각 위치 (빨강) 표시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion|Debug")
	bool bDrawDebug = false;

	// Occlusion 실행 할지 말지 설정 (컷신 동안은 시네마틱 서브시스템의 컷신 상태를 구독해 스스로 정지)
	void SetOcclusionSuspended(bool bSuspended);

protected:
	// 카메라, 락온, 그룹 서브시스템 참조 확보 후 판정 타이머 시작
	virtual void BeginPlay() override;
	// 타이머 정리 및 숨긴 조각 전부 복구
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// 타이머 콜백: 방 컷어웨이 (방 안) 또는 Sweep (방 밖) → 숨김 / 복구 순서로 처리
	void UpdateOcclusion();
	// 가림 판정 대상 수집: 플레이어 자신 + 락온 대상
	void CollectTargets(TArray<AActor*>& OutTargets) const;
	// 카메라에서 대상까지 Sweep하여 맞은 조각 수집
	void SweepToTarget(const FVector& CameraLocation, const AActor* Target, TSet<UPrimitiveComponent*>& OutOccludingPieces) const;
	// 바닥 / 천장 조각이 대상 발보다 CeilingMinHeight 이상 위에 있는지 판정. 벽 등 수직 조각은 항상 true
	bool IsAboveTargetFloor(const UPrimitiveComponent* Piece, float TargetFootZ) const;
	// 방 안이면 카메라 쪽 벽면 / 천장 / 윗층 / 방 밖을 숨김: 방이나 카메라 쪽 면이 바뀔 때만 다시 모음. 방 안인지 반환
	bool UpdateRoomCutaway(const FVector& CameraLocation, float CurrentTime);
	// 방 박스 면별 숨김 후보를 처음 들어갈 때 한 번 계산해 캐시
	const FRoomCutawayCache& GetOrBuildRoomCache(ARoomMaskVolume* Room);
	// 방 컷어웨이가 잡을 조각을 바꿈: 빠진 조각은 놓아서 RestoreDelay 뒤 복구, 새 조각은 숨김
	void HoldRoomPieces(const TSet<UPrimitiveComponent*>& RoomPieces, float CurrentTime);
	// 방 컷어웨이가 잡고 있던 조각을 모두 놓음 (방을 나갈 때)
	void ReleaseRoomPieces(float CurrentTime);
	// Sweep으로 가린 조각: 새로 가린 조각은 숨기고, 이미 숨긴 조각은 가림 시각만 갱신
	void MarkOccluded(const TSet<UPrimitiveComponent*>& OccludingPieces, float CurrentTime);
	// 메인 패스 / 깊이 패스 렌더링을 꺼서 화면에서만 숨김
	FHiddenPiece& HidePiece(UPrimitiveComponent* Piece, float CurrentTime);
	// 숨기기 전 렌더링 설정으로 복구
	void RestorePiece(UPrimitiveComponent* Piece, const FHiddenPiece& Hidden);
	// 일정 시간 가리지 않은 조각 복구
	void RestoreExpiredPieces(float CurrentTime);
	// 숨긴 조각 전부 즉시 복구
	void RestoreAllPieces();
	// 현재 방 기준으로 방에 닿지 않는 조명의 그림자를 끔 (방이 바뀔 때만 실제 변경)
	void ApplyLightShadowCulling(ARoomMaskVolume* Room, const FRoomCutawayCache& Cache);
	// 그림자를 끈 조명 전부 원래대로 복구
	void RestoreCulledLightShadows();

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
	FDelegateHandle CutsceneStateChangedHandle;
	// 방별 컷어웨이 후보 캐시
	TMap<TWeakObjectPtr<ARoomMaskVolume>, FRoomCutawayCache> RoomCaches;

	bool bOcclusionSuspended = false;

	// 방 컷어웨이가 지금 잡고 있는 기준: 셋 다 같으면 숨길 조각을 다시 모으지 않음
	TWeakObjectPtr<ARoomMaskVolume> HeldRoom;
	uint8 HeldSideMask = 0;
	bool bHeldOutsideRoom = false;
	// 방 컷어웨이가 잡고 있는 조각이 있는지
	bool bHasRoomHeldPieces = false;
	// 복구 대기 중인 조각이 있는지 (Sweep 조각, 방에서 놓은 조각): 없으면 복구 검사 생략
	bool bHasReleasingPieces = false;
	// 현재 그림자를 꺼 둔 조명과, 그 기준이 된 방
	TArray<TWeakObjectPtr<ULocalLightComponent>> CulledShadowLights;
	TWeakObjectPtr<ARoomMaskVolume> LightCullRoom;
};
