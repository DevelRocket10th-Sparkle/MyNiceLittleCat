/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 신호 표시등 구현

#include "Gimmicks/Stage/GimmickSignalLamp.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"

AGimmickSignalLamp::AGimmickSignalLamp()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	Light->SetIntensity(3000.f);
	Light->SetAttenuationRadius(600.f);
	Light->SetLightColor(FLinearColor(1.f, 0.75f, 0.4f));
	Light->SetVisibility(false);
}

void AGimmickSignalLamp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서도 시작 상태 모습으로 보이게
	ApplyLampState(bStartActivated);
}

void AGimmickSignalLamp::HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator)
{
	Super::HandleActivationChanged(bNewActivated, ChangeInstigator);
	ApplyLampState(bNewActivated);
}

void AGimmickSignalLamp::ApplyLampState(bool bLit)
{
	if (Mesh)
	{
		if (UStaticMesh* StateMesh = bLit ? LitMesh.Get() : UnlitMesh.Get())
		{
			Mesh->SetStaticMesh(StateMesh);
		}

		// 다이내믹 머티리얼이 만들어지므로 게임 중에만 (에디터 배치 인스턴스에 저장되지 않게)
		const UWorld* World = GetWorld();
		if (!LitParameterName.IsNone() && World && World->IsGameWorld())
		{
			Mesh->SetScalarParameterValueOnMaterials(LitParameterName, bLit ? 1.f : 0.f);
		}
	}

	if (Light)
	{
		Light->SetVisibility(bLit);
	}
}
