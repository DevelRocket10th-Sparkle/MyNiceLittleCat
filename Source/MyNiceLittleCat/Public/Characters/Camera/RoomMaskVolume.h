// 작성자: 강진구
// 작성일자: 2026-11-03
// 
// 방 하나당 하나 배치하는 룸 마스크 볼륨입니다.
// 플레이어가 안에 들어오면 이 방의 박스(최대 4개)를 MPC_Room에 써서 M_PP_RoomMask가 방 밖을 가립니다.
// L/T/U자 방은 Box Collision 컴포넌트를 추가해 박스를 합쳐 만듭니다 (회전 X, 월드 축 정렬).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomMaskVolume.generated.h"

class UBoxComponent;
class UMaterialParameterCollection;

UCLASS()
class MYNICELITTLECAT_API ARoomMaskVolume : public AActor
{
	GENERATED_BODY()

public:
	ARoomMaskVolume();

	// 플레이어가 현재 표시 중인 방 (없으면 nullptr): 카메라 가림 컴포넌트가 방 단위 숨김에 사용
	static ARoomMaskVolume* GetCurrentRoom();
	// 플레이어 위치로 표시할 방을 다시 고름: 박스가 겹치는 경계에서 옆방으로 넘어가지 않도록 주기적으로 호출
	static void UpdatePlayerLocation(const FVector& PlayerLocation);
	// 이 방을 이루는 박스들의 월드 AABB (회전 없는 박스 전제)
	void GetRoomBounds(TArray<FBox>& OutBounds) const;
	// 방에 들어왔을 때 카메라 쪽 벽 / 천장 / 윗층을 자동으로 숨길지 여부
	bool IsCutawayEnabled() const { return bCutaway; }
	// 이 방에서 컷어웨이로 숨기지 않을 액터인지
	bool IsCutawayIgnored(const AActor* Actor) const { return CutawayIgnoreActors.Contains(Actor); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	UFUNCTION()
	void OnBegin(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Index, bool bSweep, const FHitResult& Hit);

	UFUNCTION()
	void OnEnd(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Index);

	void Apply();
	static void Refresh(UObject* WorldContext, UMaterialParameterCollection* MPC);

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(EditAnywhere, Category = "Room")
	TObjectPtr<UMaterialParameterCollection> RoomMPC;

	// 방 단위 컷어웨이: 들어오면 카메라 쪽 벽면 (+ 벽에 붙은 소품), 천장, 윗층 전부를 화면에서 숨김
	// 복도처럼 숨기지 않아야 하는 방만 끄면 됨
	UPROPERTY(EditAnywhere, Category = "Room|Cutaway")
	bool bCutaway = true;

	// 이 방에 있을 때 위치상 숨김 대상이어도 계속 보여줄 액터 (스포이드로 지정)
	UPROPERTY(EditAnywhere, Category = "Room|Cutaway")
	TArray<TObjectPtr<AActor>> CutawayIgnoreActors;

private:
	void GatherBoxes(TArray<UBoxComponent*>& Out) const;
	bool IsPlayerInside(AActor* Player) const;
	// 점이 이 방 박스 안쪽으로 들어간 깊이 (가장 가까운 면까지 거리). 밖이면 음수
	float GetInsideDepth(const FVector& Point) const;
	// 겹친 방들 중 플레이어 위치가 가장 깊이 들어간 방 (위치 정보가 없거나 어느 박스에도 없으면 마지막에 들어간 방)
	static ARoomMaskVolume* SelectRoom();

	// 플레이어가 들어가 있는 방들 (들어간 순서)
	static TArray<TWeakObjectPtr<ARoomMaskVolume>> ActiveRooms;
	// 현재 표시 중인 방
	static TWeakObjectPtr<ARoomMaskVolume> CurrentRoom;
	// 마지막으로 알려진 플레이어 위치
	static FVector LastPlayerLocation;
	static bool bHasPlayerLocation;
};
