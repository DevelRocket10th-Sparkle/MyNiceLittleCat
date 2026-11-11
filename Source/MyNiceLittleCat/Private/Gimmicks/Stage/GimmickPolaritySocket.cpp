/// 극성 소켓 구현

#include "Gimmicks/Stage/GimmickPolaritySocket.h"
#include "Gimmicks/Magnetic/MagneticComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

AGimmickPolaritySocket::AGimmickPolaritySocket()
{
	// 소켓은 물체로만 켜짐
	bReceiveSignals = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SocketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SocketMesh"));
	SocketMesh->SetupAttachment(Root);
	SocketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SocketMesh->SetCanEverAffectNavigation(false);
	SocketMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.06f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		SocketMesh->SetStaticMesh(CylinderMesh.Object);
	}

	DockVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DockVolume"));
	DockVolume->SetupAttachment(Root);
	DockVolume->SetBoxExtent(FVector(110.f, 110.f, 80.f));
	DockVolume->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	DockVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DockVolume->SetGenerateOverlapEvents(false);
	DockVolume->SetCanEverAffectNavigation(false);
	DockVolume->ShapeColor = FColor(255, 200, 80);

	AffectedObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	AffectedObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	PolarityColors.Add(EMagneticPolarity::None, FLinearColor(0.5f, 0.5f, 0.5f));
	PolarityColors.Add(EMagneticPolarity::N, FLinearColor(1.f, 0.1f, 0.1f));
	PolarityColors.Add(EMagneticPolarity::S, FLinearColor(0.1f, 0.3f, 1.f));
}

void AGimmickPolaritySocket::BeginPlay()
{
	Super::BeginPlay();

	if (SocketMesh)
	{
		for (int32 Index = 0; Index < SocketMesh->GetNumMaterials(); ++Index)
		{
			if (UMaterialInstanceDynamic* MID = SocketMesh->CreateDynamicMaterialInstance(Index))
			{
				SocketMIDs.Add(MID);
			}
		}
	}
	UpdateVisual();

	GetWorldTimerManager().SetTimer(CheckTimer, this, &AGimmickPolaritySocket::CheckSocket, CheckInterval, true, 0.f);
}

void AGimmickPolaritySocket::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
	Super::EndPlay(EndPlayReason);
}

void AGimmickPolaritySocket::HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator)
{
	Super::HandleActivationChanged(bNewActivated, ChangeInstigator);
	UpdateVisual();
}

void AGimmickPolaritySocket::CheckSocket()
{
	UWorld* World = GetWorld();
	if (!World || AffectedObjectTypes.Num() == 0)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : AffectedObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PolaritySocket), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, DockVolume->GetComponentLocation(), DockVolume->GetComponentQuat(),
		ObjectParams, DockVolume->GetCollisionShape(), QueryParams);

	const FVector Center = DockVolume->GetComponentLocation();
	const float Now = World->GetTimeSeconds();
	const float DeltaTime = CheckInterval;

	TSet<AActor*> Seen;
	int32 NewMatched = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!IsValid(Actor) || Seen.Contains(Actor))
		{
			continue;
		}
		Seen.Add(Actor);

		const UMagneticComponent* Magnetic = Actor->FindComponentByClass<UMagneticComponent>();
		if (!Magnetic)
		{
			continue;
		}

		const EMagneticPolarity Polarity = Magnetic->GetPolarity();
		UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		const bool bFree = Body && Body->IsSimulatingPhysics();

		if (Polarity == RequiredPolarity)
		{
			++NewMatched;

			// 가운데로 끌어당겨 앉힘
			if (bFree && DockAcceleration > 0.f)
			{
				FVector ToCenter = Center - Actor->GetActorLocation();
				ToCenter.Z = 0.f;
				FVector Velocity = Body->GetPhysicsLinearVelocity();
				const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
				const FVector Pull = ToCenter.GetClampedToMaxSize(100.f) / 100.f * DockAcceleration * DeltaTime;
				const FVector Damp = -Horizontal * FMath::Clamp(DockDamping * DeltaTime, 0.f, 1.f);
				Body->SetPhysicsLinearVelocity(Velocity + Pull + Damp);
			}
		}
		else if (Polarity != EMagneticPolarity::None && bFree && RejectSpeed > 0.f)
		{
			// 반대 극성: 바깥쪽 위로 튕겨냄
			const float* Last = LastRejectTime.Find(Actor);
			if (!Last || Now - *Last >= RejectCooldown)
			{
				FVector Away = Actor->GetActorLocation() - Center;
				Away.Z = 0.f;
				Away = Away.IsNearlyZero() ? GetActorForwardVector() : Away.GetSafeNormal();
				Body->SetPhysicsLinearVelocity(Away * RejectSpeed + FVector(0.f, 0.f, RejectSpeed * 0.6f));
				LastRejectTime.Add(Actor, Now);
			}
		}
	}

	MatchedCount = NewMatched;
	SetActivated(MatchedCount >= RequiredCount, this);
}

void AGimmickPolaritySocket::UpdateVisual()
{
	const FLinearColor* Base = PolarityColors.Find(RequiredPolarity);
	const FLinearColor Color = (Base ? *Base : FLinearColor::White) * (IsActivated() ? ActiveColorScale : 1.f);
	for (UMaterialInstanceDynamic* MID : SocketMIDs)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(ColorParameterName, Color);
		}
	}
}
