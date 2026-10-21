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

    const FVector Direction = IncomingDirection.GetSafeNormal(); // 들어온 방향 (입사 벡터)
    const FVector FrontNormal = CollisionBox->GetForwardVector(); // 앞면 (반사판의 법선 벡터)
    const FVector HitNormal = FVector(Hit.ImpactNormal).GetSafeNormal();

    if (true == Direction.IsNearlyZero() || true == HitNormal.IsNearlyZero())
        return false;

    // 반사판 앞면에 맞았는지 체크 (뒷면 or 옆면은 반사 처리 하지 않아요)
    constexpr float FrontFaceThreshold = 0.99f; // 거의 1에 가까운 (적당한 앞면 오차 각도를 허용)
    if (FrontFaceThreshold > FVector::DotProduct(HitNormal, FrontNormal)) return false;

    // 앞면을 향해 들어오는 공격인지 확인
    const float IncomingDot = FVector::DotProduct(Direction, FrontNormal);
    if (IncomingDot >= -KINDA_SMALL_NUMBER) return false;

    // 반사 방향 R = D + 2N(-D내적N)
    OutDirection = (Direction - 2.f * IncomingDot * FrontNormal).GetSafeNormal();
    return (false == OutDirection.IsNearlyZero());
}
