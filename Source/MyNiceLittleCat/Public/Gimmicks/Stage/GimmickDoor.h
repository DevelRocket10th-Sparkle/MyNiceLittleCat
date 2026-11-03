/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 신호로 열리고 닫히는 문입니다 (레버·버튼·발판의 LinkedActors에 넣어 사용).
/// 켜짐 = 열림. 신호 조건은 베이스의 SignalRule(Any/All)·bInvertSignal로 정합니다.
/// 움직이는 부분은 DoorMover 아래에 붙입니다 (C++ DoorMesh는 비어 있음 → BP에서 메시 지정, 문짝 여러 개면 메시를 더 붙임).
/// DoorwayVolume = 문이 지나가는 공간: 닫힐 때 플레이어 등이 안에 있으면 열린 채 기다리고(bWaitWhileObstructed),
/// 닫혀 있는 동안 내비게이션을 막아 AI가 길로 쓰지 않게 합니다 (프로젝트 내비 런타임 생성이 Static이면 반영 안 됨).
/// 열림·닫힘 연출은 DoorMover의 OnMoveStarted/OnMoveFinished 또는 On Activation Changed 이벤트에 연결합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "GimmickDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UGimmickMoverComponent;

UCLASS()
class MYNICELITTLECAT_API AGimmickDoor : public AGimmickActorBase
{
	GENERATED_BODY()

public:
	AGimmickDoor();

	// 문 공간에 막는 대상이 있는지
	UFUNCTION(BlueprintPure, Category = "Gimmick|Door")
	bool IsDoorwayObstructed() const;

	// 지금 열려 있거나 열리는 중인지 (막혀서 열린 채 기다리는 중 포함)
	UFUNCTION(BlueprintPure, Category = "Gimmick|Door")
	bool IsDoorOpen() const { return bDoorOpen; }

protected:
	virtual void BeginPlay() override;
	virtual void UpdateMovers(bool bInstant) override;

	UFUNCTION()
	void HandleDoorwayOverlapChanged(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void HandleDoorwayBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyNavigationBlock();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGimmickMoverComponent> DoorMover;

	// 문짝 (BP에서 메시 지정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	// 문이 지나가는 공간 (끼임 확인 + 닫혔을 때 내비 차단 영역)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> DoorwayVolume;

	// 닫힐 때 문 공간에 막는 대상이 있으면 열린 채 기다림 (닫히는 중에 들어와도 다시 열림)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door")
	bool bWaitWhileObstructed = true;

	// 문을 막는 대상 (비면 모든 폰)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door", meta = (EditCondition = "bWaitWhileObstructed"))
	TArray<TSubclassOf<AActor>> ObstructionClasses;

	// 닫혀 있는 동안 DoorwayVolume 영역을 내비게이션에서 막음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door")
	bool bBlockNavigationWhenClosed = true;

private:
	bool bDoorOpen = false;
};
