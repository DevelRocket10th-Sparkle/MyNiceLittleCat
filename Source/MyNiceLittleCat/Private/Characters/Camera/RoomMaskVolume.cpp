#include "Characters/Camera/RoomMaskVolume.h"
#include "Components/BoxComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/ConstructorHelpers.h"

TArray<TWeakObjectPtr<ARoomMaskVolume>> ARoomMaskVolume::ActiveRooms;
TWeakObjectPtr<ARoomMaskVolume> ARoomMaskVolume::CurrentRoom;
FVector ARoomMaskVolume::LastPlayerLocation = FVector::ZeroVector;
bool ARoomMaskVolume::bHasPlayerLocation = false;

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

	if (ActorHasTag(TEXT("DisableRoomMask")))
	{
		SetActorEnableCollision(false);
		return;
	}

	TArray<UBoxComponent*> Boxes;
	GatherBoxes(Boxes);
	for (UBoxComponent* B : Boxes)
	{
		B->SetCollisionObjectType(ECC_WorldDynamic);
		B->SetCollisionResponseToAllChannels(ECR_Ignore);
		B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);   // 플레이어만 충돌 감지
		B->OnComponentBeginOverlap.AddDynamic(this, &ARoomMaskVolume::OnBegin);
		B->OnComponentEndOverlap.AddDynamic(this, &ARoomMaskVolume::OnEnd);
		B->SetGenerateOverlapEvents(true);
		B->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	for (const UBoxComponent* B : Boxes)
	{
		TArray<AActor*> Players;
		B->GetOverlappingActors(Players, APlayerCharacter::StaticClass());
		if (!Players.IsEmpty())
		{
			ActiveRooms.AddUnique(this);
			Refresh(this, RoomMPC);
			break;
		}
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
	LastPlayerLocation = Other->GetActorLocation();
	bHasPlayerLocation = true;
	ActiveRooms.Remove(this);
	ActiveRooms.Add(this);
	Refresh(this, RoomMPC);
}

void ARoomMaskVolume::OnEnd(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32)
{
	// 같은 방의 다른 박스에 아직 있으면 유지
	if (!Other || !Other->IsA<APlayerCharacter>() || IsPlayerInside(Other)) return;
	LastPlayerLocation = Other->GetActorLocation();
	bHasPlayerLocation = true;
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

	ARoomMaskVolume* Selected = SelectRoom();
	CurrentRoom = Selected;
	if (Selected)
	{
		Selected->Apply();
	}
	else if (MPC)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(WorldContext, MPC, TEXT("RoomActive"), 0.f);
	}
}

void ARoomMaskVolume::UpdatePlayerLocation(const FVector& PlayerLocation)
{
	LastPlayerLocation = PlayerLocation;
	bHasPlayerLocation = true;

	// 겹친 박스 안에서 움직여도 오버랩 이벤트는 오지 않으므로 위치로 다시 고름
	ARoomMaskVolume* Selected = SelectRoom();
	if (Selected && Selected != CurrentRoom.Get())
	{
		CurrentRoom = Selected;
		Selected->Apply();
	}
}

ARoomMaskVolume* ARoomMaskVolume::SelectRoom()
{
	ARoomMaskVolume* Best = nullptr;
	float BestDepth = 0.f;
	ARoomMaskVolume* Last = nullptr;
	for (const TWeakObjectPtr<ARoomMaskVolume>& Weak : ActiveRooms)
	{
		ARoomMaskVolume* Room = Weak.Get();
		if (!Room)
		{
			continue;
		}
		Last = Room;
		if (!bHasPlayerLocation)
		{
			continue;
		}
		const float Depth = Room->GetInsideDepth(LastPlayerLocation);
		if (Depth >= 0.f && (!Best || Depth > BestDepth))
		{
			Best = Room;
			BestDepth = Depth;
		}
	}
	// 플레이어 중심이 어느 박스에도 없으면 (경계 위) 기존처럼 마지막에 들어간 방
	return Best ? Best : Last;
}

float ARoomMaskVolume::GetInsideDepth(const FVector& Point) const
{
	TArray<FBox> Bounds;
	GetRoomBounds(Bounds);
	float Best = -1.f;
	for (const FBox& B : Bounds)
	{
		if (!B.IsInside(Point))
		{
			continue;
		}
		const FVector ToMin = Point - B.Min;
		const FVector ToMax = B.Max - Point;
		const float Depth = FMath::Min(ToMin.GetMin(), ToMax.GetMin());
		Best = FMath::Max(Best, Depth);
	}
	return Best;
}

void ARoomMaskVolume::GatherBoxes(TArray<UBoxComponent*>& Out) const
{
	GetComponents<UBoxComponent>(Out);
	Out.Remove(Box);
	Out.Insert(Box, 0);   // 루트 박스는 항상 0번 슬롯
	if (Out.Num() > 4) Out.SetNum(4);
}

ARoomMaskVolume* ARoomMaskVolume::GetCurrentRoom()
{
	ARoomMaskVolume* Room = CurrentRoom.Get();
	return Room && ActiveRooms.Contains(Room) ? Room : nullptr;
}

void ARoomMaskVolume::GetRoomBounds(TArray<FBox>& OutBounds) const
{
	OutBounds.Reset();
	TArray<UBoxComponent*> Boxes;
	GatherBoxes(Boxes);
	for (const UBoxComponent* B : Boxes)
	{
		const FVector Center = B->GetComponentLocation();
		const FVector Extent = B->GetScaledBoxExtent();
		OutBounds.Add(FBox(Center - Extent, Center + Extent));
	}
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
