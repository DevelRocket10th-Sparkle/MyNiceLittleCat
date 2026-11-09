/// 극성 에너지장 구현

#include "Gimmicks/Stage/GimmickPolarityField.h"
#include "Gimmicks/Magnetic/MagneticComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

AGimmickPolarityField::AGimmickPolarityField()
{
	// 켜진 채 시작 (신호로 끄려면 bInvertSignal 등 사용)
	bStartActivated = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	FieldVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("FieldVolume"));
	FieldVolume->SetupAttachment(Root);
	FieldVolume->SetBoxExtent(FVector(40.f, 200.f, 150.f));
	FieldVolume->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	FieldVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FieldVolume->SetGenerateOverlapEvents(false);
	FieldVolume->SetCanEverAffectNavigation(false);
	FieldVolume->ShapeColor = FColor(120, 200, 255);

	FieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FieldMesh"));
	FieldMesh->SetupAttachment(FieldVolume);
	FieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FieldMesh->SetGenerateOverlapEvents(false);
	FieldMesh->SetCanEverAffectNavigation(false);
	FieldMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FieldMesh->SetStaticMesh(CubeMesh.Object);
	}

	AffectedObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	AffectedObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	AffectedObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	FieldColors.Add(EMagneticPolarity::None, FLinearColor(0.6f, 0.6f, 0.7f));
	FieldColors.Add(EMagneticPolarity::N, FLinearColor(3.f, 0.25f, 0.2f));
	FieldColors.Add(EMagneticPolarity::S, FLinearColor(0.2f, 0.6f, 3.f));
}

void AGimmickPolarityField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 기본 큐브(100) 기준으로 영역 크기에 맞춤
	if (FieldVolume && FieldMesh)
	{
		FieldMesh->SetRelativeScale3D(FieldVolume->GetUnscaledBoxExtent() / 50.f);
	}
	if (FieldMesh && FieldMaterial)
	{
		FieldMesh->SetMaterial(0, FieldMaterial);
	}
}

void AGimmickPolarityField::BeginPlay()
{
	Super::BeginPlay();

	if (FieldMesh && FieldMesh->GetMaterial(0))
	{
		FieldMID = FieldMesh->CreateDynamicMaterialInstance(0, FieldMaterial ? FieldMaterial.Get() : nullptr);
	}
	UpdateVisual();

	GetWorldTimerManager().SetTimer(CheckTimer, this, &AGimmickPolarityField::CheckField, CheckInterval, true, 0.f);
}

void AGimmickPolarityField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
	ReleaseAllSuppressed();
	Super::EndPlay(EndPlayReason);
}

void AGimmickPolarityField::HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator)
{
	Super::HandleActivationChanged(bNewActivated, ChangeInstigator);

	// 장이 꺼지면 덮어쓴 극성을 바로 되돌림
	if (!bNewActivated)
	{
		ReleaseAllSuppressed();
	}
	UpdateVisual();
}

void AGimmickPolarityField::CheckField()
{
	UWorld* World = GetWorld();
	if (!World || !IsActivated() || AffectedObjectTypes.Num() == 0)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : AffectedObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PolarityField), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, FieldVolume->GetComponentLocation(), FieldVolume->GetComponentQuat(),
		ObjectParams, FieldVolume->GetCollisionShape(), QueryParams);

	TSet<UMagneticComponent*> Inside;
	auto ApplyTo = [this, &Inside](UMagneticComponent* Magnetic)
	{
		if (!Magnetic || Inside.Contains(Magnetic))
		{
			return;
		}
		Inside.Add(Magnetic);

		const EMagneticPolarity Old = Magnetic->GetPolarity();
		if (FieldMode == EPolarityFieldMode::Suppress && !SuppressedOriginals.Contains(Magnetic))
		{
			SuppressedOriginals.Add(Magnetic, Old);
		}
		if (Old != FieldPolarity)
		{
			Magnetic->SetPolarity(FieldPolarity);
			Magnetic->NotifyMagnetInteraction();
			OnFieldApplied.Broadcast(this, Magnetic->GetOwner(), Old);
		}
	};

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		ApplyTo(IsValid(Actor) ? Actor->FindComponentByClass<UMagneticComponent>() : nullptr);
	}

	// 펫이 들고 있는 물체는 콜리전이 꺼져 있어 오버랩에 안 잡히므로 위치로도 검사
	const FTransform VolumeTransform = FieldVolume->GetComponentTransform();
	const FVector Extent = FieldVolume->GetUnscaledBoxExtent();
	for (TObjectIterator<UMagneticComponent> It; It; ++It)
	{
		UMagneticComponent* Magnetic = *It;
		const AActor* MagneticOwner = IsValid(Magnetic) ? Magnetic->GetOwner() : nullptr;
		if (!MagneticOwner || Magnetic->GetWorld() != World || Inside.Contains(Magnetic))
		{
			continue;
		}
		const FVector Local = VolumeTransform.InverseTransformPosition(MagneticOwner->GetActorLocation());
		if (FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z)
		{
			ApplyTo(Magnetic);
		}
	}

	// Suppress: 영역을 벗어난 물체는 원래 극성으로
	if (FieldMode == EPolarityFieldMode::Suppress)
	{
		for (auto It = SuppressedOriginals.CreateIterator(); It; ++It)
		{
			UMagneticComponent* Magnetic = It.Key().Get();
			if (!Magnetic)
			{
				It.RemoveCurrent();
			}
			else if (!Inside.Contains(Magnetic))
			{
				Magnetic->SetPolarity(It.Value());
				It.RemoveCurrent();
			}
		}
	}
}

void AGimmickPolarityField::ReleaseAllSuppressed()
{
	for (const TPair<TWeakObjectPtr<UMagneticComponent>, EMagneticPolarity>& Pair : SuppressedOriginals)
	{
		if (UMagneticComponent* Magnetic = Pair.Key.Get())
		{
			Magnetic->SetPolarity(Pair.Value);
		}
	}
	SuppressedOriginals.Reset();
}

FLinearColor AGimmickPolarityField::GetFieldColor() const
{
	const FLinearColor* Color = FieldColors.Find(FieldPolarity);
	return Color ? *Color : FLinearColor::White;
}

void AGimmickPolarityField::UpdateVisual()
{
	if (!FieldMesh)
	{
		return;
	}
	FieldMesh->SetVisibility(IsActivated() || !bHideWhenOff);
	if (FieldMID)
	{
		FieldMID->SetVectorParameterValue(ColorParameterName, GetFieldColor());
	}
}
