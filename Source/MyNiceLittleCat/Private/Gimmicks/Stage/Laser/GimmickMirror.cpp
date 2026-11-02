/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 스테이지 거울 구현

#include "Gimmicks/Stage/Laser/GimmickMirror.h"
#include "Gimmicks/Reflection/ReflectionMath.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"

AGimmickMirror::AGimmickMirror()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	MirrorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MirrorPivot"));
	MirrorPivot->SetupAttachment(Mesh);
	MirrorPivot->SetMobility(EComponentMobility::Movable);
	// 받침(루트) 메시를 납작하게 늘이거나 줄여도 거울판 크기·높이가 바뀌지 않게
	MirrorPivot->SetUsingAbsoluteScale(true);

	// 스테이지 레이저 높이(60cm)를 가운데에 두는 판 (두께 8, 폭 120, 높이 120)
	MirrorSurface = CreateDefaultSubobject<UBoxComponent>(TEXT("MirrorSurface"));
	MirrorSurface->SetupAttachment(MirrorPivot);
	MirrorSurface->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	MirrorSurface->InitBoxExtent(FVector(4.f, 60.f, 60.f));
	MirrorSurface->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	MirrorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorMesh"));
	MirrorMesh->SetupAttachment(MirrorSurface);
	MirrorMesh->SetMobility(EComponentMobility::Movable);
	MirrorMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	ApplyRotateMode();
}

void AGimmickMirror::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyRotateMode();
}

void AGimmickMirror::BeginPlay()
{
	ApplyRotateMode();
	Super::BeginPlay();

	if (MirrorPivot)
	{
		InitialPivotYaw = MirrorPivot->GetRelativeRotation().Yaw;
	}
}

void AGimmickMirror::ResetGimmick_Implementation(AActor* ResetInstigator)
{
	if (!MirrorPivot)
	{
		return;
	}

	bRotating = false;
	SetActorTickEnabled(false);
	MirrorPivot->SetRelativeRotation(FRotator(0.f, InitialPivotYaw, 0.f));
}

void AGimmickMirror::ApplyRotateMode()
{
	// 거울은 켜짐 상태를 쓰지 않으므로 토글 옵션은 숨김, 신호는 Signal 모드에서만 받음
	bShowToggleableOption = false;
	bReceiveSignals = RotateMode == EGimmickMirrorRotateMode::Signal;
}

void AGimmickMirror::Interact_Implementation(AActor* InteractInstigator)
{
	if (!IInteractableInterface::Execute_CanInteract(this, InteractInstigator))
	{
		return;
	}

	RotateOnce();
}

bool AGimmickMirror::CanInteract_Implementation(AActor* InteractInstigator) const
{
	return IsInteractionEnabled() && RotateMode == EGimmickMirrorRotateMode::Interact && !bRotating;
}

void AGimmickMirror::HandleSignalChanged(AActor* Source, bool bSignalOn)
{
	// 켜짐 상태 대신 신호가 켜질 때마다 한 칸 회전
	if (bReceiveSignals && bSignalOn)
	{
		RotateOnce();
	}
}

void AGimmickMirror::RotateOnce()
{
	if (bRotating || !MirrorPivot || RotateMode == EGimmickMirrorRotateMode::Fixed)
	{
		return;
	}

	RotateStartYaw = MirrorPivot->GetRelativeRotation().Yaw;
	RotateTargetYaw = RotateStartYaw + RotateStep;
	RotateElapsed = 0.f;

	K2_OnRotateStarted();

	if (RotateDuration <= 0.f)
	{
		MirrorPivot->SetRelativeRotation(FRotator(0.f, FRotator::NormalizeAxis(RotateTargetYaw), 0.f));
		K2_OnRotateFinished();
		return;
	}

	bRotating = true;
	SetActorTickEnabled(true);
}

void AGimmickMirror::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRotating || !MirrorPivot)
	{
		SetActorTickEnabled(false);
		return;
	}

	RotateElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(RotateElapsed / RotateDuration, 0.f, 1.f);
	const float Yaw = FMath::Lerp(RotateStartYaw, RotateTargetYaw, FMath::SmoothStep(0.f, 1.f, Alpha));
	MirrorPivot->SetRelativeRotation(FRotator(0.f, FRotator::NormalizeAxis(Yaw), 0.f));

	if (Alpha >= 1.f)
	{
		bRotating = false;
		SetActorTickEnabled(false);
		K2_OnRotateFinished();
	}
}

bool AGimmickMirror::TryGetReflection_Implementation(const FVector& IncomingDirection, const FHitResult& Hit, FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;

	// 받침 등 다른 부분에 맞은 빛은 반사하지 않음
	if (!Hit.bBlockingHit || Hit.bStartPenetrating || Hit.GetComponent() != MirrorSurface)
	{
		return false;
	}

	const FVector Front = MirrorSurface->GetForwardVector();
	const FVector HitNormal(Hit.ImpactNormal);
	if (FReflectionMath::TryReflectFrontFace(IncomingDirection, Front, HitNormal, OutDirection))
	{
		return true;
	}

	return bDoubleSided && FReflectionMath::TryReflectFrontFace(IncomingDirection, -Front, HitNormal, OutDirection);
}
