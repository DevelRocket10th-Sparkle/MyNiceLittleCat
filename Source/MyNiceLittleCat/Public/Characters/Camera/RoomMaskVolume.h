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

private:
	void GatherBoxes(TArray<UBoxComponent*>& Out) const;
	bool IsPlayerInside(AActor* Player) const;

	// 플레이어가 들어가 있는 방들 (들어간 순서, 마지막 = 표시 중인 방)
	static TArray<TWeakObjectPtr<ARoomMaskVolume>> ActiveRooms;
};
