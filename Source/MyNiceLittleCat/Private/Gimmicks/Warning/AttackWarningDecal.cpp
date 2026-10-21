/// 작성자: 최요환
/// 작성일자: 2026-10-21
///
/// 공격 예고 데칼 구현

#include "Gimmicks/Warning/AttackWarningDecal.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AAttackWarningDecal::AAttackWarningDecal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 데칼은 로컬 X 방향으로 투영 → Pitch -90으로 아래를 향하게 (로컬 Y = 월드 Y, 로컬 Z = 월드 X)
	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal->SetupAttachment(Root);
	Decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Decal->DecalSize = FVector(ProjectionDepth * 0.5f, 100.f, 100.f);
	Decal->SetVisibility(false);
}

void AAttackWarningDecal::BeginPlay()
{
	Super::BeginPlay();

	if (FadeScreenSize > 0.f)
	{
		Decal->SetFadeScreenSize(FadeScreenSize);
	}
}

void AAttackWarningDecal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bShowing || ShowDuration <= 0.f)
	{
		SetActorTickEnabled(false);
		return;
	}

	ShowElapsed += DeltaTime;
	SetProgress(ShowElapsed / ShowDuration);

	if (Progress >= 1.f)
	{
		SetActorTickEnabled(false);

		if (bHideOnComplete)
		{
			HideWarning();
		}
	}
}

void AAttackWarningDecal::ShowWarning(const FVector& Location, FVector2D HalfSize, float Yaw, float Duration, FLinearColor Color)
{
	SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f));
	Decal->DecalSize = FVector(ProjectionDepth * 0.5f, FMath::Max(HalfSize.Y, 1.f), FMath::Max(HalfSize.X, 1.f));

	if (UMaterialInstanceDynamic* MID = GetOrCreateDynamicMaterial())
	{
		Decal->SetDecalMaterial(MID);
	}

	SetColor(Color);

	ShowDuration = Duration;
	ShowElapsed = 0.f;
	SetProgress(0.f);

	bShowing = true;
	Decal->SetVisibility(true);
	// DecalSize는 렌더 상태 갱신이 필요
	Decal->MarkRenderStateDirty();

	SetActorTickEnabled(Duration > 0.f);
}

void AAttackWarningDecal::ShowCircleWarning(const FVector& Location, float Radius, float Duration, FLinearColor Color)
{
	ShowWarning(Location, FVector2D(Radius, Radius), 0.f, Duration, Color);
}

void AAttackWarningDecal::HideWarning()
{
	bShowing = false;
	SetActorTickEnabled(false);

	if (bDestroyOnHide)
	{
		Destroy();
		return;
	}

	Decal->SetVisibility(false);
}

void AAttackWarningDecal::SetProgress(float NewProgress)
{
	Progress = FMath::Clamp(NewProgress, 0.f, 1.f);

	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(ProgressParameterName, Progress);
	}
}

void AAttackWarningDecal::SetColor(FLinearColor NewColor)
{
	if (UMaterialInstanceDynamic* MID = GetOrCreateDynamicMaterial())
	{
		MID->SetVectorParameterValue(ColorParameterName, NewColor);
	}
}

UMaterialInstanceDynamic* AAttackWarningDecal::GetOrCreateDynamicMaterial()
{
	if (!DynamicMaterial && WarningMaterial)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(WarningMaterial, this);
	}

	return DynamicMaterial;
}
