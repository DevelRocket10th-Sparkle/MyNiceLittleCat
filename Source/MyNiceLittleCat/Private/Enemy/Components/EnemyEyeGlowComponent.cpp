// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Components/EnemyEyeGlowComponent.h"
#include "Enemy/EnemyLog.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"

UEnemyEyeGlowComponent::UEnemyEyeGlowComponent()
{
	// 경고 깜빡임 동안만 Tick
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnemyEyeGlowComponent::SetGlowColor(FLinearColor Color)
{
	BaseGlowColor = Color;
	// 꺼진 상태에서 끄는 요청은 눈 메시를 만들 필요 없음
	const bool bTurnOff = Color.GetMax() <= 0.f;
	if (bTurnOff && !EyeMaterialInstance)
	{
		return;
	}

	if (!EyeMaterialInstance && !CreateEyeMeshes())
	{
		return;
	}

	if (PulseRemaining <= 0.f)
	{
		ApplyColor(BaseGlowColor);
	}
}

void UEnemyEyeGlowComponent::PlayWarningPulse(FLinearColor PeakColor, float Duration)
{
	if (Duration <= 0.f || (!EyeMaterialInstance && !CreateEyeMeshes()))
	{
		return;
	}

	PulsePeakColor = PeakColor;
	PulseRemaining = Duration;
	ApplyColor(PulsePeakColor);
	SetComponentTickEnabled(true);
}

void UEnemyEyeGlowComponent::StopWarningPulse()
{
	PulseRemaining = 0.f;
	SetComponentTickEnabled(false);
	ApplyColor(BaseGlowColor);
}

void UEnemyEyeGlowComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	PulseRemaining = FMath::Max(0.f, PulseRemaining - DeltaTime);

	// 남은 시간 기준 |sin|으로 밝기 진동: 최소 밝기 ~ 1
	const float Wave = FMath::Abs(FMath::Sin(PulseRemaining * PulseAngularSpeed));
	const float Intensity = PulseMinIntensity + (1.f - PulseMinIntensity) * Wave;
	ApplyColor(PulsePeakColor * Intensity);

	if (PulseRemaining <= 0.f)
	{
		StopWarningPulse();
	}
}

void UEnemyEyeGlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(SurfaceMesh) && SurfaceMesh->GetMaterial(EyeMaterialSlot) == EyeMaterialInstance)
	{
		SurfaceMesh->SetMaterial(EyeMaterialSlot, OriginalSurfaceMaterial);
	}
	SurfaceMesh = nullptr;
	OriginalSurfaceMaterial = nullptr;

	for (UStaticMeshComponent* Eye : EyeComponents)
	{
		if (IsValid(Eye))
		{
			Eye->DestroyComponent();
		}
	}

	EyeComponents.Reset();
	EyeMaterialInstance = nullptr;

	Super::EndPlay(EndPlayReason);
}

bool UEnemyEyeGlowComponent::CreateEyeMeshes()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (bUseMeshMaterial)
	{
		SurfaceMesh = Mesh;
		if (!SurfaceMesh || !EyeMaterial || EyeMaterialSlot < 0 || EyeMaterialSlot >= SurfaceMesh->GetNumMaterials())
		{
			return false;
		}
		OriginalSurfaceMaterial = SurfaceMesh->GetMaterial(EyeMaterialSlot);
		EyeMaterialInstance = SurfaceMesh->CreateDynamicMaterialInstance(EyeMaterialSlot, EyeMaterial);
		return EyeMaterialInstance != nullptr;
	}

	if (!Mesh || !EyeMesh || !EyeMaterial || EyeSockets.IsEmpty())
	{
		UE_LOG(LogEnemy, Warning, TEXT("[EyeGlow] %s: mesh, material or sockets not set"), *GetNameSafe(GetOwner()));
		return false;
	}

	EyeMaterialInstance = UMaterialInstanceDynamic::Create(EyeMaterial, this);

	for (const FEnemyEyeSocket& Socket : EyeSockets)
	{
		if (!Mesh->DoesSocketExist(Socket.RootSocket))
		{
			UE_LOG(LogEnemy, Warning, TEXT("[EyeGlow] %s: socket '%s' not found"), *GetNameSafe(GetOwner()), *Socket.RootSocket.ToString());
			continue;
		}

		// 위/아래 눈꺼풀 중간을 눈 표면으로 사용
		const bool bHasLids = Mesh->DoesSocketExist(Socket.UpperLidSocket) && Mesh->DoesSocketExist(Socket.LowerLidSocket);
		const FVector Surface = bHasLids
			? (Mesh->GetSocketLocation(Socket.UpperLidSocket) + Mesh->GetSocketLocation(Socket.LowerLidSocket)) * 0.5f
			: Mesh->GetSocketLocation(Socket.RootSocket);

		// 런타임 컴포넌트: 소유 액터의 인스턴스 컴포넌트로 등록해야 에디터 디버깅 목록에도 보임
		UStaticMeshComponent* Eye = NewObject<UStaticMeshComponent>(Character);
		Character->AddInstanceComponent(Eye);
		Eye->SetStaticMesh(EyeMesh);
		Eye->SetMaterial(0, EyeMaterialInstance);
		Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Eye->SetCastShadow(false);
		Eye->SetupAttachment(Mesh, Socket.RootSocket);
		Eye->RegisterComponent();
		Eye->SetWorldLocation(Surface + Character->GetActorForwardVector() * SurfaceForwardOffset);
		Eye->SetWorldScale3D(FVector(EyeScale));

		EyeComponents.Add(Eye);
	}

	// 붙일 수 있는 소켓이 하나도 없으면 다음 요청에서 다시 시도하도록 재질도 비움
	if (EyeComponents.IsEmpty())
	{
		EyeMaterialInstance = nullptr;
		return false;
	}

	return true;
}

void UEnemyEyeGlowComponent::ApplyColor(const FLinearColor& Color)
{
	if (EyeMaterialInstance)
	{
		EyeMaterialInstance->SetVectorParameterValue(ColorParameterName, Color);
	}
}
