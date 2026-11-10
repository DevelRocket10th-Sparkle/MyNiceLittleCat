#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Systems/OcclusionGroupSubsystem.h"
#include "CameraOcclusionComponent.generated.h"

class UCameraComponent;
class ULockOnComponent;
class UPrimitiveComponent;
class ARoomMaskVolume;

// 방 단위 컷어웨이 판정 기준
// 플레이어가 RoomMaskVolume 안에 있으면 태그 없이 위치만으로 숨길 메시를 고름
//  - 윗층: 방 박스 윗면보다 중심이 높고, 방 바닥 면적과 겹치는 메시 전부 (천장, 윗층 바닥 / 벽 / 소품, 지붕)
//  - 벽면: 카메라 쪽을 향한 박스 옆면에 붙은 얇은 띠 안의 메시 (벽, 문틀, 창, 커튼, 액자 등 벽에 붙은 소품)
// 숨기지 않을 액터는 해당 RoomMaskVolume의 CutawayIgnoreActors에 등록
USTRUCT(BlueprintType)
struct FRoomCutawaySettings
{
	GENERATED_BODY()

	// 방 단위 컷어웨이 사용 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway")
	bool bEnabled = true;
	// 벽면에서 방 안쪽으로 이 거리 이내까지만 들어온 메시를 벽에 붙은 것으로 간주 (cm)
	// 커튼 / 액자 / 벽등은 잡히고, 소파 / 침대처럼 깊은 가구는 남도록 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float InnerDepth = 80.f;
	// 벽면에서 방 바깥쪽으로 이 거리 이내에 걸친 메시까지 숨김: 벽 두께 + 외벽에 붙은 장식 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float OuterDepth = 150.f;
	// 옆면 법선과 (플레이어 → 카메라) 방향의 내적이 이 값보다 크면 카메라 쪽 벽으로 간주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaceDotThreshold = 0.3f;
	// 박스 윗면보다 이만큼 낮은 곳까지 중심이 있으면 천장 / 윗층으로 간주 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float TopTolerance = 30.f;
	// 윗층 판정 시 방 바닥 면적을 바깥으로 넓히는 거리: 방 경계 위 벽 / 외벽 윗부분 포함 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float TopFootprintMargin = 150.f;
	// 방 박스와 전혀 겹치지 않는 메시 (옆방 기둥 / 소품, 건물 밖)도 숨김
	// RoomMask가 방 밖을 어둡게 해도 경계 부드러움 (EdgeSoftness) 구간이나 반투명 메시는 비쳐 보이므로 아예 그리지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway")
	bool bHideOutsideRoom = true;
	// 방 박스를 이만큼 줄여서 겹침 판정: 경계에 살짝 걸친 옆방 메시도 바깥으로 취급 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float OutsideInset = 20.f;
	// 방 박스에서 이 거리 안에 있는 바깥 메시만 숨김 대상: 멀리 있는 메시는 RoomMask가 완전히 가림 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float OutsideRadius = 5000.f;
	// 벽면 (박스 옆면)을 이 거리 이상 뚫고 나간 메시는 벽에 박힌 것으로 보고 깊이와 관계없이 벽과 함께 숨김: 벽난로, 문틀, 창 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float WallEmbedDepth = 20.f;
	// 벽에 박힌 메시로 인정하는 방 안쪽 최대 깊이: 모서리에서 옆벽 조각까지 같이 숨는 것 방지 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float MaxEmbeddedReach = 250.f;
};

// 방 하나의 컷어웨이 후보: 처음 들어갈 때 한 번만 계산
struct FRoomCutawayCache
{
	// 옆면 인덱스: 0 = -X, 1 = +X, 2 = -Y, 3 = +Y
	static constexpr int32 SideCount = 4;
	// 옆면별 숨길 조각: 박스가 여러 개면 같은 방향 옆면끼리 합침
	TArray<TWeakObjectPtr<UPrimitiveComponent>> SidePieces[SideCount];
	// 천장 / 윗층 조각: 방 안이면 항상 숨김
	TArray<TWeakObjectPtr<UPrimitiveComponent>> TopPieces;
	// 방 박스 밖 조각 (옆방, 건물 밖): 방 안이면 항상 숨김
	TArray<TWeakObjectPtr<UPrimitiveComponent>> OutsidePieces;
};

// 카메라와 플레이어 (및 락온 대상) 사이를 가리는 벽 / 천장을 면 단위로 숨기는 컴포넌트
// 1) 방 안: RoomMaskVolume 기준 컷어웨이만 사용 (태그 불필요, 방 배치만으로 동작)
//    가까이 간 가구 / 계단이 사라지지 않도록 방 안에서는 Sweep을 쓰지 않음
// 2) 방 밖: Sphere Sweep + 같은 평면에 이어진 조각 자동 그룹 (태그 불필요)
// 숨김은 메인 / 깊이 패스 렌더링만 끄는 방식이라 그림자와 콜리전은 유지
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
	// 방 단위 컷어웨이 기준
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion")
	FRoomCutawaySettings RoomCutaway;
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
	// 현재 방에서 카메라 쪽 벽면 / 윗층 메시 수집
	void CollectRoomCutaway(const FVector& CameraLocation, TSet<UPrimitiveComponent*>& OutOccludingPieces);
	// 방 박스 면별 숨김 후보를 처음 들어갈 때 한 번 계산해 캐시
	const FRoomCutawayCache& GetOrBuildRoomCache(ARoomMaskVolume* Room);
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
	// 방별 컷어웨이 후보 캐시
	TMap<TWeakObjectPtr<ARoomMaskVolume>, FRoomCutawayCache> RoomCaches;

	bool bOcclusionSuspended = false;
	// 이번 판정에서 방 컷어웨이가 적용 중인지: 적용 중이면 Sweep 생략
	bool bInCutawayRoom = false;
};
