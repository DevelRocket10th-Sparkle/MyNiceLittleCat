#include "Pet/GAS/AbilityObject/PetReflector.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gimmicks/Reflection/ReflectionMath.h"

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

    // 실제 설치된 반사판은 이동을 막음
    CollisionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    // 설치된 시점부터 수명 시작
    SetLifeSpan(ReflectorLifeSpan);
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

bool APetReflector::TryGetReflection_Implementation(const FVector& IncomingDirection, const FHitResult& Hit, FVector& OutDirection) const
{
    OutDirection = FVector::ZeroVector;

    if(false == Hit.bBlockingHit || Hit.bStartPenetrating) return false;
    if (CollisionBox.Get() != Hit.GetComponent()) return false;

    return FReflectionMath::TryReflectFrontFace(
        IncomingDirection, CollisionBox->GetForwardVector(), FVector(Hit.ImpactNormal), OutDirection);
}
