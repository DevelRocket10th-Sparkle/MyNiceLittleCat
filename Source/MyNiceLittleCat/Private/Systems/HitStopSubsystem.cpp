// Fill out your copyright notice in the Description page of Project Settings.

#include "Systems/HitStopSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraModifier_CameraShake.h"

void UHitStopSubsystem::ApplyHitStop(float Duration)
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || Duration <= 0.f) return;

	if (!bActive)
	{
		bActive = true;
		SavedTimeDilation = UGameplayStatics::GetGlobalTimeDilation(World);
		UGameplayStatics::SetGlobalTimeDilation(World, FrozenTimeDilation);
		FreezeMeshes();
	}

	// 연타가 겹치면 끊기지 않고 연장
	RemainingSeconds = FMath::Max(RemainingSeconds, Duration);

	// 게임 시간은 멈춰 있으므로 실제 시간으로 셈
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UHitStopSubsystem::TickHitStop));
	}
}

bool UHitStopSubsystem::TickHitStop(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		TickHandle.Reset();
		RemainingSeconds = 0.f;
		bActive = false;
		FrozenMeshes.Reset();
		return false;
	}

	// 일시정지 메뉴가 열려 있는 동안은 남은 시간을 쓰지 않음
	if (World->IsPaused())
	{
		return true;
	}

	// 카메라 셰이크는 게임 시간으로 돌아서 같이 멈춰 버림 → 멈춘 동안 빠진 시간만큼 직접 진행
	const float MissingTime = DeltaTime * FMath::Max(1.f - FrozenTimeDilation, 0.f);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager.Get() : nullptr;
		if (!CameraManager) continue;

		if (UCameraModifier_CameraShake* ShakeModifier = Cast<UCameraModifier_CameraShake>(
			CameraManager->FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass())))
		{
			FMinimalViewInfo Scratch = CameraManager->GetCameraCacheView();
			ShakeModifier->ModifyCamera(MissingTime, Scratch);
		}
	}

	RemainingSeconds -= DeltaTime;
	if (RemainingSeconds > 0.f)
	{
		return true;
	}

	TickHandle.Reset();
	CancelHitStop();
	return false;
}

void UHitStopSubsystem::CancelHitStop()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	const bool bWasActive = bActive;
	bActive = false;
	RemainingSeconds = 0.f;

	UWorld* World = GetWorld();
	if (bWasActive && World && !World->bIsTearingDown)
	{
		UGameplayStatics::SetGlobalTimeDilation(World, SavedTimeDilation);
	}
	UnfreezeMeshes();
}

void UHitStopSubsystem::FreezeMeshes()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 애님 그래프(꼬리 RigidBody 등)를 통째로 멈춰서 포즈 고정
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		TArray<USkeletalMeshComponent*> Meshes;
		It->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->IsComponentTickEnabled())
			{
				Mesh->SetComponentTickEnabled(false);
				FrozenMeshes.Add(Mesh);
			}
		}
	}
}

void UHitStopSubsystem::UnfreezeMeshes()
{
	for (const TWeakObjectPtr<USkeletalMeshComponent>& Mesh : FrozenMeshes)
	{
		if (Mesh.IsValid())
		{
			Mesh->SetComponentTickEnabled(true);
		}
	}
	FrozenMeshes.Reset();
}

void UHitStopSubsystem::Deinitialize()
{
	CancelHitStop();
	Super::Deinitialize();
}
