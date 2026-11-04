#include "Characters/Camera/RoomMaskVolume.h"
#include "Components/BoxComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/ConstructorHelpers.h"

TArray<TWeakObjectPtr<ARoomMaskVolume>> ARoomMaskVolume::ActiveRooms;

ARoomMaskVolume::ARoomMaskVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(400.f, 400.f, 200.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPC(TEXT("/Game/Game/VFX/Materials/MPC_Room.MPC_Room"));
	if (MPC.Succeeded()) RoomMPC = MPC.Object;
}

void ARoomMaskVolume::BeginPlay()
{
	Super::BeginPlay();

	TArray<UBoxComponent*> Boxes;
	GatherBoxes(Boxes);
	for (UBoxComponent* B : Boxes)
	{
		B->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		B->SetCollisionObjectType(ECC_WorldDynamic);
		B->SetCollisionResponseToAllChannels(ECR_Ignore);
		B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);   // 플레이어만 충돌 감지
		B->OnComponentBeginOverlap.AddDynamic(this, &ARoomMaskVolume::OnBegin);
		B->OnComponentEndOverlap.AddDynamic(this, &ARoomMaskVolume::OnEnd);
	}
}

void ARoomMaskVolume::EndPlay(const EEndPlayReason::Type Reason)
{
	ActiveRooms.Remove(this);
	Refresh(this, RoomMPC);
	Super::EndPlay(Reason);
}

void ARoomMaskVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	TArray<UBoxComponent*> Boxes;
	GetComponents<UBoxComponent>(Boxes);
	for (UBoxComponent* B : Boxes)
	{
		B->SetCanEverAffectNavigation(false);
	}
}

void ARoomMaskVolume::OnBegin(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!Other || !Other->IsA<APlayerCharacter>()) return;
	ActiveRooms.Remove(this);
	ActiveRooms.Add(this);
	Refresh(this, RoomMPC);
}

void ARoomMaskVolume::OnEnd(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32)
{
	// 같은 방의 다른 박스에 아직 있으면 유지
	if (!Other || !Other->IsA<APlayerCharacter>() || IsPlayerInside(Other)) return;
	ActiveRooms.Remove(this);
	Refresh(this, RoomMPC);
}

void ARoomMaskVolume::Apply() 
{
	static const FName Centers[] = { TEXT("RoomCenter"), TEXT("RoomCenter1"), TEXT("RoomCenter2"), TEXT("RoomCenter3") };
	static const FName Extents[] = { TEXT("RoomExtent"), TEXT("RoomExtent1"), TEXT("RoomExtent2"), TEXT("RoomExtent3") };

	TArray<UBoxComponent*> Boxes;
	GatherBoxes(Boxes);

	for (int32 i = 0; i < 4; ++i)
	{
		const bool bUsed = Boxes.IsValidIndex(i);
		UKismetMaterialLibrary::SetVectorParameterValue(this, RoomMPC, Centers[i],
			bUsed ? FLinearColor(Boxes[i]->GetComponentLocation()) : FLinearColor::Black);
		UKismetMaterialLibrary::SetVectorParameterValue(this, RoomMPC, Extents[i],
			bUsed ? FLinearColor(Boxes[i]->GetScaledBoxExtent()) : FLinearColor::Black);
	}
	UKismetMaterialLibrary::SetScalarParameterValue(this, RoomMPC, TEXT("RoomActive"), 1.f);
}

void ARoomMaskVolume::Refresh(UObject* WorldContext, UMaterialParameterCollection* MPC)
{
	ActiveRooms.RemoveAll([](const TWeakObjectPtr<ARoomMaskVolume>& R) { return !R.IsValid(); });

	if (ActiveRooms.Num() > 0)
	{
		ActiveRooms.Last()->Apply();
	}
	else if (MPC)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(WorldContext, MPC, TEXT("RoomActive"), 0.f);
	}
}

void ARoomMaskVolume::GatherBoxes(TArray<UBoxComponent*>& Out) const
{
	GetComponents<UBoxComponent>(Out);
	Out.Remove(Box);
	Out.Insert(Box, 0);   // 루트 박스는 항상 0번 슬롯
	if (Out.Num() > 4) Out.SetNum(4);
}

bool ARoomMaskVolume::IsPlayerInside(AActor* Player) const
{
	TArray<UBoxComponent*> Boxes;
	GatherBoxes(Boxes);
	for (const UBoxComponent* B : Boxes)
	{
		if (B->IsOverlappingActor(Player)) return true;
	}
	return false;
}
