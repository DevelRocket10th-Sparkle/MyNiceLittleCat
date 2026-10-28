/// 작성자: 최요환
/// 작성일자: 2026-10-18
///
/// 자성 컴포넌트 구현

#include "Gimmicks/Magnetic/MagneticComponent.h"
#include "Gimmicks/Magnetic/MagneticSubsystem.h"
#include "Gimmicks/Magnetic/MagneticSettings.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"

namespace MagneticEffectParams
{
	// 나이아가라 User 파라미터 이름 (NS 쪽과 맞춰야 함)
	const FName Color(TEXT("Color"));
	const FName Polarity(TEXT("Polarity"));
	const FName Activity(TEXT("Activity"));
	const FName IconOffset(TEXT("IconOffset"));
}

UMagneticComponent::UMagneticComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	LineOfSightObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

	PolarityColors.Add(EMagneticPolarity::None, FLinearColor(0.5f, 0.5f, 0.5f));
	PolarityColors.Add(EMagneticPolarity::N, FLinearColor(1.f, 0.1f, 0.1f));
	PolarityColors.Add(EMagneticPolarity::S, FLinearColor(0.1f, 0.3f, 1.f));
}

void UMagneticComponent::BeginPlay()
{
	Super::BeginPlay();

	UpdatePolarityColor();
	SpawnPolarityEffect();
}

void UMagneticComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateEffectActivity(DeltaTime);

	if (!bMagnetEnabled || Polarity == EMagneticPolarity::None)
	{
		return;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	UMagneticSubsystem* MagneticSubsystem = World ? World->GetSubsystem<UMagneticSubsystem>() : nullptr;
	if (!Owner || !MagneticSubsystem)
	{
		return;
	}

	// 자력원이 여러 개면 힘을 합산
	const FVector Location = Owner->GetActorLocation();
	FVector TotalForce = FVector::ZeroVector;

	MagneticSubsystem->ForEachSource([&](AActor* SourceActor, const FMagneticSourceInfo& Info)
	{
		if (SourceActor == Owner)
		{
			return;
		}

		FVector Force;
		if (CalculateForceFrom(SourceActor, Info, Location, Force))
		{
			TotalForce += Force;
		}
	});

	if (!TotalForce.IsNearlyZero())
	{
		ApplyMagneticForce(TotalForce);
	}
}

bool UMagneticComponent::CalculateForceFrom(AActor* SourceActor, const FMagneticSourceInfo& Info, const FVector& Location, FVector& OutForce) const
{
	OutForce = FVector::ZeroVector;

	if (Polarity == EMagneticPolarity::None || Info.Polarity == EMagneticPolarity::None
		|| Info.Strength <= 0.f || Info.Range <= 0.f)
	{
		return false;
	}

	FVector Delta = Location - Info.Location;

	// 자력원과 높이 차이가 크면 (위아래로 떨어진 물체) 작용 안 함
	if (Info.bLimitHeight && FMath::Abs(Delta.Z) > Info.HeightLimit)
	{
		return false;
	}

	FVector Aim = Info.AimDirection;
	if (Info.bPlanar)
	{
		Delta.Z = 0.f;
		Aim.Z = 0.f;
	}

	Aim = Aim.GetSafeNormal();
	const float Distance = Delta.Size();
	if (Aim.IsNearlyZero() || Distance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (!IsInsideField(Info, Delta, Aim))
	{
		return false;
	}

	const bool bSamePolarity = Info.Polarity == Polarity;

	// 이미 자력원 가까이 끌려온 상태면 더 당기지 않음
	if (!bSamePolarity && Distance <= PullStopDistance)
	{
		return false;
	}

	if (Info.bRequireLineOfSight && IsLineOfSightBlocked(SourceActor, Info.Location, Location))
	{
		return false;
	}

	float FalloffScale = 1.f;
	if (Info.Falloff == EMagneticFalloff::Linear)
	{
		FalloffScale = 1.f - FMath::Clamp(Distance / Info.Range, 0.f, 1.f);
	}

	// 펫 기준: 같은 극성은 자력원 반대쪽, 다른 극성은 자력원 쪽
	const FVector AwayFromSource = Delta / Distance;
	const float Sign = bSamePolarity ? 1.f : -1.f;
	OutForce = AwayFromSource * Sign * Info.Strength * ForceMultiplier * FalloffScale;
	return true;
}

bool UMagneticComponent::IsInsideField(const FMagneticSourceInfo& Info, const FVector& Delta, const FVector& Aim)
{
	// 대상 액터 위치(중심) 기준 판정
	if (Info.Shape == EMagneticFieldShape::Line)
	{
		const float Along = FVector::DotProduct(Delta, Aim);
		if (Along < 0.f || Along > Info.Range)
		{
			return false;
		}

		const float FromAxis = (Delta - Aim * Along).Size();
		return FromAxis <= Info.LineRadius;
	}

	if (Delta.Size() > Info.Range)
	{
		return false;
	}

	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(Info.ConeHalfAngle));
	return FVector::DotProduct(Delta.GetSafeNormal(), Aim) >= CosHalfAngle;
}

bool UMagneticComponent::IsLineOfSightBlocked(AActor* SourceActor, const FVector& From, const FVector& To) const
{
	UWorld* World = GetWorld();
	if (!World || LineOfSightObjectTypes.IsEmpty())
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : LineOfSightObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MagneticLineOfSight), false);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(SourceActor);

	FHitResult Hit;
	return World->LineTraceSingleByObjectType(Hit, From, To, ObjectParams, QueryParams);
}

void UMagneticComponent::ApplyMagneticForce(const FVector& Force)
{
	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	if (RootPrimitive && RootPrimitive->IsSimulatingPhysics())
	{
		RootPrimitive->AddForce(Force, NAME_None, bIgnoreMass);
	}
	// TODO: 물리 미사용 액터는 OnMagneticForce를 받아 직접 이동 (필요해지면 기본 처리 추가)

	NotifyMagnetInteraction();
	OnMagneticForce.Broadcast(this, Force);
}

void UMagneticComponent::NotifyMagnetInteraction()
{
	ActivityHoldRemaining = ActivityHoldTime;
	CurrentActivity = 1.f;
	if (PolarityEffectComponent)
	{
		PolarityEffectComponent->SetVariableFloat(MagneticEffectParams::Activity, CurrentActivity);
	}
}

void UMagneticComponent::SetPolarity(EMagneticPolarity NewPolarity)
{
	if (Polarity == NewPolarity)
	{
		return;
	}

	const EMagneticPolarity OldPolarity = Polarity;
	Polarity = NewPolarity;

	UpdatePolarityColor();
	UpdatePolarityEffect();
	OnPolarityChanged.Broadcast(this, NewPolarity, OldPolarity);
}

FLinearColor UMagneticComponent::GetPolarityColor(EMagneticPolarity InPolarity) const
{
	const FLinearColor* Color = PolarityColors.Find(InPolarity);
	return Color ? *Color : FLinearColor::White;
}

void UMagneticComponent::CreateDynamicMaterials()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents(Meshes);

	for (UMeshComponent* Mesh : Meshes)
	{
		if (!ColorTargetTag.IsNone() && !Mesh->ComponentHasTag(ColorTargetTag))
		{
			continue;
		}

		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			if (UMaterialInstanceDynamic* DynamicMaterial = Mesh->CreateDynamicMaterialInstance(Index))
			{
				DynamicMaterials.Add(DynamicMaterial);
			}
		}
	}
}

void UMagneticComponent::UpdatePolarityColor()
{
	if (!bApplyPolarityColor)
	{
		return;
	}

	if (DynamicMaterials.IsEmpty())
	{
		CreateDynamicMaterials();
	}

	const FLinearColor* Color = PolarityColors.Find(Polarity);
	if (!Color)
	{
		return;
	}

	for (UMaterialInstanceDynamic* DynamicMaterial : DynamicMaterials)
	{
		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(ColorParameterName, *Color);
		}
	}
}

UMeshComponent* UMagneticComponent::FindEffectMesh() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (EffectMeshTag.IsNone() || Mesh->ComponentHasTag(EffectMeshTag))
		{
			return Mesh;
		}
	}
	return nullptr;
}

void UMagneticComponent::SpawnPolarityEffect()
{
	if (!bSpawnPolarityEffect || PolarityEffectComponent)
	{
		return;
	}

	UNiagaraSystem* System = PolarityEffect;
	if (!System)
	{
		System = GetDefault<UMagneticSettings>()->DefaultPolarityEffect.LoadSynchronous();
	}

	UMeshComponent* Mesh = FindEffectMesh();
	if (!System || !Mesh)
	{
		return;
	}

	PolarityEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, false, false);
	if (!PolarityEffectComponent)
	{
		return;
	}

	// 전기 이펙트의 메시 표면 샘플링은 붙은 메시(Attach Parent)를 사용
	UpdatePolarityEffect();
}

void UMagneticComponent::UpdatePolarityEffect()
{
	if (!PolarityEffectComponent)
	{
		return;
	}

	if (Polarity == EMagneticPolarity::None)
	{
		PolarityEffectComponent->Deactivate();
		return;
	}

	// 아이콘은 메시 바운드 윗면 위에 띄움 (이펙트 위치 기준 높이)
	float IconHeight = IconHeightOffset;
	if (const USceneComponent* AttachParent = PolarityEffectComponent->GetAttachParent())
	{
		if (const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(AttachParent))
		{
			IconHeight += Primitive->Bounds.Origin.Z + Primitive->Bounds.BoxExtent.Z - Primitive->GetComponentLocation().Z;
		}
	}

	PolarityEffectComponent->SetVariableLinearColor(MagneticEffectParams::Color, GetPolarityColor(Polarity));
	PolarityEffectComponent->SetVariableFloat(MagneticEffectParams::Polarity, Polarity == EMagneticPolarity::S ? 1.f : 0.f);
	PolarityEffectComponent->SetVariableFloat(MagneticEffectParams::Activity, CurrentActivity);
	PolarityEffectComponent->SetVariableVec3(MagneticEffectParams::IconOffset, FVector(0.f, 0.f, IconHeight));

	if (!PolarityEffectComponent->IsActive())
	{
		PolarityEffectComponent->Activate(true);
	}
}

void UMagneticComponent::UpdateEffectActivity(float DeltaTime)
{
	if (!PolarityEffectComponent || CurrentActivity <= 0.f)
	{
		return;
	}

	if (ActivityHoldRemaining > 0.f)
	{
		ActivityHoldRemaining -= DeltaTime;
		return;
	}

	CurrentActivity = FMath::Max(0.f, CurrentActivity - ActivityFadeSpeed * DeltaTime);
	PolarityEffectComponent->SetVariableFloat(MagneticEffectParams::Activity, CurrentActivity);
}
