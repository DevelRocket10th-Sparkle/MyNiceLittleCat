/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 플레이어나 물체(자성 박스 등)가 올라가면 작동하는 감압 발판입니다.
/// Hold: 올라가 있는 동안만 켜짐 (내려오면 ReleaseDelay 뒤 꺼짐) / Latch: 한 번 눌리면 계속 켜짐 (bSaveState로 저장 가능).
/// Trigger 박스 영역을 CheckInterval마다 검사해 ActivatorClasses에 맞는 액터 수를 셉니다 (물체에 Generate Overlap Events가 없어도 감지).
/// 눌리는 부분은 PlateMover 아래 PlateMesh에 붙입니다. 메시는 BP에서 지정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "GimmickPressurePlate.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UGimmickMoverComponent;
class AGimmickPressurePlate;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPressurePlateOccupantsChanged, AGimmickPressurePlate*, Plate, int32, OccupantCount);

UENUM(BlueprintType)
enum class EPressurePlateMode : uint8
{
	// 올라가 있는 동안만 켜짐
	Hold,
	// 한 번 눌리면 계속 켜짐
	Latch
};

UCLASS()
class MYNICELITTLECAT_API AGimmickPressurePlate : public AGimmickActorBase
{
	GENERATED_BODY()

public:
	AGimmickPressurePlate();

	UFUNCTION(BlueprintPure, Category = "Gimmick|PressurePlate")
	int32 GetOccupantCount() const { return OccupantCount; }

public:
	// 올라간 대상 수가 바뀔 때 (밟는 소리 등)
	UPROPERTY(BlueprintAssignable, Category = "Gimmick|PressurePlate")
	FOnPressurePlateOccupantsChanged OnOccupantsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	void StartChecking();
	void StopChecking();
	void CheckOccupants();
	int32 CountOccupants() const;
	void HandleReleaseDelayExpired();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGimmickMoverComponent> PlateMover;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PlateMesh;

	// 감지 영역 (발판 위)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate")
	EPressurePlateMode Mode = EPressurePlateMode::Hold;

	// 발판을 누르는 대상 (비면 감지된 모든 액터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate")
	TArray<TSubclassOf<AActor>> ActivatorClasses;

	// 감지할 오브젝트 타입 (플레이어 = Pawn, 물리 박스 = PhysicsBody)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate")
	TArray<TEnumAsByte<EObjectTypeQuery>> ActivatorObjectTypes;

	// 켜지는 데 필요한 대상 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	// Hold: 모두 내려온 뒤 꺼지기까지 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate", meta = (ClampMin = "0.0", Units = "s", EditCondition = "Mode == EPressurePlateMode::Hold"))
	float ReleaseDelay = 0.f;

	// 감지 주기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate", meta = (ClampMin = "0.02", Units = "s"))
	float CheckInterval = 0.1f;

private:
	int32 OccupantCount = 0;
	FTimerHandle CheckTimer;
	FTimerHandle ReleaseTimer;
};
