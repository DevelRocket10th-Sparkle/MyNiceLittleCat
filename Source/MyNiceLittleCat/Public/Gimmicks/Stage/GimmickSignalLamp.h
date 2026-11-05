/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 신호를 받으면 불이 켜지는 표시등입니다 (마을에서는 가로등).
/// 멀리 떨어진 퍼즐(수신기·레버 등)의 LinkedActors에 넣어 두면, 그 퍼즐이 풀렸는지를 결과 쪽(문 옆 등)에서 보여줍니다.
/// 예: 스테이지 2 빛의 문 옆 가로등 A·B (각 구역 수신기가 켜지면 점등), 문은 같은 수신기들로 All 조건.
/// 켜지면: LitMesh로 메시 교체(지정했을 때), Light 켜기, 메시 머티리얼 스칼라 파라미터(LitParameterName) 1.
/// 신호 조건(Any/All·반전)은 베이스 설정을 그대로 씁니다. 메시·빛 색은 BP에서 지정합니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "GimmickSignalLamp.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class MYNICELITTLECAT_API AGimmickSignalLamp : public AGimmickActorBase
{
	GENERATED_BODY()

public:
	AGimmickSignalLamp();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) override;

	// 메시·빛·머티리얼을 켜짐/꺼짐에 맞춤
	void ApplyLampState(bool bLit);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Light;

	// 꺼졌을 때 메시 (비면 메시를 바꾸지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Lamp")
	TObjectPtr<UStaticMesh> UnlitMesh;

	// 켜졌을 때 메시 (비면 메시를 바꾸지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Lamp")
	TObjectPtr<UStaticMesh> LitMesh;

	// 켜짐 정도를 넣을 메시 머티리얼 스칼라 파라미터 이름 (꺼짐 0 / 켜짐 1, 없으면 무시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Lamp")
	FName LitParameterName = TEXT("Lit");
};
