/// 극성 소켓 기믹입니다.
/// DockVolume 안에 RequiredPolarity 극성의 자성 물체가 RequiredCount개 이상 있으면 켜집니다.
/// 맞는 극성은 소켓 가운데로 끌어당겨 앉히고, 반대 극성은 밀어내서 "틀렸다"는 걸 바로 보여줍니다 (물리 시뮬레이션 중인 물체만).
/// 펫이 들고 있는 물체는 건드리지 않습니다 (루트가 물리 시뮬레이션이 아니면 힘을 주지 않음).
/// SocketMesh는 BP에서 지정하고, 머티리얼의 ColorParameterName에 요구 극성 색(켜지면 ActiveColorScale배)을 넣습니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "Gimmicks/Magnetic/MagneticTypes.h"
#include "GimmickPolaritySocket.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class MYNICELITTLECAT_API AGimmickPolaritySocket : public AGimmickActorBase
{
	GENERATED_BODY()

public:
	AGimmickPolaritySocket();

	UFUNCTION(BlueprintPure, Category = "Gimmick|PolaritySocket")
	int32 GetMatchedCount() const { return MatchedCount; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	void CheckSocket();
	void UpdateVisual();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SocketMesh;

	// 감지·끌어당김 영역
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> DockVolume;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket")
	EMagneticPolarity RequiredPolarity = EMagneticPolarity::N;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	// 맞는 극성을 가운데로 당기는 가속도 (0이면 당기지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket", meta = (ClampMin = "0.0", Units = "cm/s"))
	float DockAcceleration = 1500.f;

	// 가운데 근처에서 흔들리지 않게 수평 속도를 줄이는 비율 (초당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket", meta = (ClampMin = "0.0"))
	float DockDamping = 4.f;

	// 반대 극성을 밀어내는 속도 (0이면 밀지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RejectSpeed = 700.f;

	// 같은 물체를 다시 밀어내기까지 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket", meta = (ClampMin = "0.0", Units = "s"))
	float RejectCooldown = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket")
	TArray<TEnumAsByte<EObjectTypeQuery>> AffectedObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket", meta = (ClampMin = "0.02", Units = "s"))
	float CheckInterval = 0.05f;

	/* 표시 */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket|Visual")
	FName ColorParameterName = TEXT("Color");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket|Visual")
	TMap<EMagneticPolarity, FLinearColor> PolarityColors;

	// 켜졌을 때 색 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PolaritySocket|Visual", meta = (ClampMin = "0.0"))
	float ActiveColorScale = 4.f;

private:
	int32 MatchedCount = 0;
	TMap<TWeakObjectPtr<AActor>, float> LastRejectTime;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SocketMIDs;

	FTimerHandle CheckTimer;
};
