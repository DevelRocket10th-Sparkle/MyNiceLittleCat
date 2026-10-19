#include "Pet/GAS/AbilityObject/PetReflector.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

APetReflector::APetReflector()
{
 	PrimaryActorTick.bCanEverTick = false;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);

    CollisionBox->InitBoxExtent(FVector(10.f, 100.f, 100.f));

    // 프리뷰에서는 충돌하지 않고, 실제 반사판 설치 시 켤 예정
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CollisionBox->SetGenerateOverlapEvents(false);

    ReflectorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReflectorMesh"));
    ReflectorMesh->SetupAttachment(CollisionBox);

    // 충돌은 CollisionBox가 일괄로..
    ReflectorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ReflectorMesh->SetGenerateOverlapEvents(false);
    ReflectorMesh->SetCanEverAffectNavigation(false);
}

void APetReflector::BeginPlay()
{
	Super::BeginPlay();
}

void APetReflector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float APetReflector::GetHalfWidth() const
{
    return CollisionBox->GetScaledBoxExtent().Y;
}

FVector APetReflector::GetPlacementHalfExtents() const
{
    return CollisionBox->GetScaledBoxExtent();
}
