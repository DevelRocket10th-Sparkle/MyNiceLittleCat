/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 신호로 열리고 닫히는 문 구현

#include "Gimmicks/Stage/GimmickDoor.h"
#include "Gimmicks/GimmickMoverComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "NavigationData.h"

AGimmickDoor::AGimmickDoor()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DoorMover = CreateDefaultSubobject<UGimmickMoverComponent>(TEXT("DoorMover"));
	DoorMover->SetupAttachment(Root);
	DoorMover->OpenOffset = FVector(0.f, 0.f, -300.f);
	DoorMover->OpenDuration = 1.f;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorMover);
	DoorMesh->SetMobility(EComponentMobility::Movable);
	// 내비 차단은 DoorwayVolume이 맡음 (메시가 열린 위치에서도 길을 막지 않게)
	DoorMesh->SetCanEverAffectNavigation(false);

	DoorwayVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorwayVolume"));
	DoorwayVolume->SetupAttachment(Root);
	DoorwayVolume->SetBoxExtent(FVector(40.f, 150.f, 150.f));
	DoorwayVolume->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	DoorwayVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DoorwayVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	DoorwayVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DoorwayVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	DoorwayVolume->SetGenerateOverlapEvents(true);
	DoorwayVolume->bDynamicObstacle = true;
	// 배치 상태(닫힘) 기준으로 내비 빌드에도 반영, 열리면 BeginPlay·상태 변경에서 끔
	DoorwayVolume->SetCanEverAffectNavigation(true);
	DoorwayVolume->ShapeColor = FColor(255, 160, 40);
}

void AGimmickDoor::BeginPlay()
{
	DoorwayVolume->OnComponentBeginOverlap.AddDynamic(this, &AGimmickDoor::HandleDoorwayBeginOverlap);
	DoorwayVolume->OnComponentEndOverlap.AddDynamic(this, &AGimmickDoor::HandleDoorwayOverlapChanged);

	Super::BeginPlay();

	// 시작 상태가 닫힘이면 Super에서 상태 변경이 없으므로 여기서 내비 반영
	ApplyNavigationBlock();

	if (bBlockNavigationWhenClosed)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
		if (NavData && NavData->GetRuntimeGenerationMode() == ERuntimeGenerationType::Static)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GimmickDoor] %s: 내비 런타임 생성이 Static이라 문 열림·닫힘이 내비게이션에 반영되지 않음 (Dynamic Modifiers Only 이상 필요)"), *GetName());
		}
	}
}

bool AGimmickDoor::IsDoorwayObstructed() const
{
	TArray<AActor*> Overlapping;
	DoorwayVolume->GetOverlappingActors(Overlapping);
	for (const AActor* Actor : Overlapping)
	{
		if (!IsValid(Actor) || Actor == this)
		{
			continue;
		}

		if (ObstructionClasses.Num() == 0)
		{
			if (Actor->IsA<APawn>())
			{
				return true;
			}
			continue;
		}

		for (const TSubclassOf<AActor>& Class : ObstructionClasses)
		{
			if (Class && Actor->IsA(Class))
			{
				return true;
			}
		}
	}
	return false;
}

void AGimmickDoor::UpdateMovers(bool bInstant)
{
	// 닫혀야 해도 문이 덜 닫혔고 공간이 막혀 있으면 열린 채 유지
	bool bOpen = IsActivated();
	if (!bOpen && bWaitWhileObstructed && !bInstant && DoorMover->GetProgress() > 0.f && IsDoorwayObstructed())
	{
		bOpen = true;
	}

	bDoorOpen = bOpen;

	TInlineComponentArray<UGimmickMoverComponent*> Movers(this);
	for (UGimmickMoverComponent* Mover : Movers)
	{
		if (Mover->bFollowActivation)
		{
			Mover->SetOpen(bOpen, bInstant);
		}
	}

	ApplyNavigationBlock();
}

void AGimmickDoor::HandleDoorwayBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	HandleDoorwayOverlapChanged(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex);
}

void AGimmickDoor::HandleDoorwayOverlapChanged(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 닫히는 중·닫힘 대기 중에만 다시 판단
	if (bWaitWhileObstructed && !IsActivated())
	{
		UpdateMovers(false);
	}
}

void AGimmickDoor::ApplyNavigationBlock()
{
	const bool bBlock = bBlockNavigationWhenClosed && !bDoorOpen;
	if (DoorwayVolume->CanEverAffectNavigation() != bBlock)
	{
		DoorwayVolume->SetCanEverAffectNavigation(bBlock);
	}
}
