/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 신호로 열리고 닫히는 문입니다 (레버·버튼·발판의 LinkedActors에 넣어 사용).
/// 켜짐 = 열림. 신호 조건은 베이스의 SignalRule(Any/All)·bInvertSignal로 정합니다.
/// 움직이는 부분은 DoorMover 아래에 붙입니다 (C++ DoorMesh는 비어 있음 → BP에서 메시 지정, 문짝 여러 개면 메시를 더 붙임).
/// DoorwayVolume = 문이 지나가는 공간: 닫힐 때 플레이어 등이 안에 있으면 열린 채 기다리고(bWaitWhileObstructed),
/// 닫혀 있는 동안 내비게이션을 막아 AI가 길로 쓰지 않게 합니다 (프로젝트 내비 런타임 생성이 Static이면 반영 안 됨).
/// 열림·닫힘 연출은 DoorMover의 OnMoveStarted/OnMoveFinished 또는 On Activation Changed 이벤트에 연결합니다.
/// 연출(시퀀스, 스테이지 스크립트)은 ForceDoorState로 문 상태를 강제하고, 끝나면 ReleaseForcedDoorState로 신호 상태에 돌려줍니다.
/// bPlayerCanToggle을 켜면 플레이어가 상호작용 키로 여닫을 수 있습니다 (신호와 같은 켜짐 상태를 바꿈).
/// 잠긴 문(bStartLocked, SetLocked)은 플레이어가 여닫을 수 없고, 상호작용하면 덜컹 소리가 납니다.
///   연출이 닫아 둔 문도 플레이어에게는 잠긴 문처럼 보입니다.
///   열쇠로 여는 문은 자물쇠 기믹(AGimmickKeyLock)의 LinkedActors에 이 문을 넣습니다 (레버와 같은 방식).
/// 안내 UI 위치는 InteractPromptAnchor 컴포넌트(없으면 문 공간 중심)를 사용합니다. 레버와 같은 규칙입니다.
/// 우선순위: 연출 강제 > (신호 = 플레이어 조작, 마지막에 바뀐 쪽). 잠금은 플레이어 조작만 막음

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "Interfaces/InteractableInterface.h"
#include "GimmickDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UGimmickMoverComponent;
class USoundBase;

UCLASS()
class MYNICELITTLECAT_API AGimmickDoor : public AGimmickActorBase, public IInteractableInterface
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

	/* 연출 제어 */

	// 연출 등 외부에서 문 상태 강제: 해제 전까지 신호·플레이어 조작보다 우선 (잠금과 무관)
	// - Duration이 0 이상이면 이번 이동 시간, 음수면 Mover 기본 시간
	// - bInstant면 즉시 이동 (스킵, 초기화): 효과음과 Mover 이벤트 없음
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Door")
	void ForceDoorState(bool bOpen, float Duration = -1.f, bool bInstant = false);

	// 강제 해제: 현재 신호(켜짐) 상태로 복귀
	// - 강제한 위치에 그대로 두려면 먼저 SetActivated로 신호 상태를 맞춘 뒤 해제
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Door")
	void ReleaseForcedDoorState(bool bInstant = false);

	// Getter: 연출이 문 상태를 강제 중인지
	UFUNCTION(BlueprintPure, Category = "Gimmick|Door")
	bool IsDoorStateForced() const { return bForced; }

	/* 잠금 */

	// 잠금 설정: 잠기면 플레이어가 여닫을 수 없음 (신호·연출은 그대로 동작). 같은 상태면 무시
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Door|Lock")
	void SetLocked(bool bNewLocked, AActor* ChangeInstigator);

	// Getter: 잠겨 있는지
	UFUNCTION(BlueprintPure, Category = "Gimmick|Door|Lock")
	bool IsLocked() const { return bLocked; }

	/* IInteractableInterface: 플레이어 여닫기, 잠긴 문 확인 */

	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual FVector GetInteractFocusLocation_Implementation(AActor* InteractInstigator) const override;
	virtual FInteractAlignment GetInteractAlignment_Implementation(AActor* InteractInstigator) const override;
	virtual FInteractPromptInfo GetInteractPrompt_Implementation(AActor* InteractInstigator) const override;

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

	/* 플레이어 상호작용 */

	// 플레이어가 상호작용 키로 여닫을 수 있는지 (기본 꺼짐 = 신호·연출로만 여닫는 기존 문)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door|Interaction")
	bool bPlayerCanToggle = false;

	// 여닫을 때 안내 문구: 닫혀 있을 때 / 열려 있을 때
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door|Interaction", meta = (EditCondition = "bPlayerCanToggle"))
	FText OpenPromptText = NSLOCTEXT("GimmickDoor", "Open", "문 열기");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door|Interaction", meta = (EditCondition = "bPlayerCanToggle"))
	FText ClosePromptText = NSLOCTEXT("GimmickDoor", "Close", "문 닫기");

	/* 잠금 */

	// 시작할 때 잠겨 있는지 (플레이어가 열 수 없는 문: 덜컹 소리로 확인만)
	// - 잠금 상태는 저장하지 않음. 풀거나 잠그는 것은 SetLocked 또는 마커 Door:<번호>:Lock|Unlock
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door|Lock")
	bool bStartLocked = false;

	// 잠긴 문을 확인할 때 소리 (덜컹)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door|Lock")
	TObjectPtr<USoundBase> LockedSound;

	// 잠긴 문 안내 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door|Lock")
	FText LockedPromptText = NSLOCTEXT("GimmickDoor", "Locked", "잠긴 문 확인");

private:
	bool bDoorOpen = false;

	// 잠겨 있는지 (BeginPlay에서 bStartLocked로 시작)
	bool bLocked = false;

	// 연출 강제 상태: 강제 중인지, 강제한 열림 여부, 이동 시간(음수면 Mover 기본)
	bool bForced = false;
	bool bForcedOpen = false;
	float ForcedDuration = -1.f;
};
