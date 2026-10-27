#include "Enemy/Boss/Objects/MoneyProjectile.h"
#include "Characters/PlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AMoneyProjectile::AMoneyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetCollisionObjectType(ECC_GameTraceChannel1); // 엔진 설정에 정의한 탄환 전용 충돌 채널
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	Collision->OnComponentHit.AddDynamic(this, &AMoneyProjectile::HandleHit);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bIsHomingProjectile = false;
	Movement->bAutoActivate = false;
	InitialLifeSpan = 5.f;
}

void AMoneyProjectile::Launch(const FVector& Direction, float Speed, float Damage, float LifeSeconds)
{
	HitDamage = Damage;
	Age = 0.f;
	FlutterPhase = FMath::FRandRange(0.f, 2.f * PI);
	InitialMeshRotation = Mesh->GetRelativeRotation();
	bLaunched = true;
	Collision->IgnoreActorWhenMoving(GetOwner(), true);
	Collision->IgnoreActorWhenMoving(GetInstigator(), true);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Movement->InitialSpeed = Movement->MaxSpeed = Speed;
	Movement->Velocity = Direction.GetSafeNormal() * Speed;
	Movement->Activate();
	SetLifeSpan(LifeSeconds);
}

void AMoneyProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLaunched)
	{
		return;
	}
	Age += DeltaSeconds;
	FRotator Rotation = InitialMeshRotation;
	Rotation.Roll += FMath::Sin(Age * FlutterFrequency * 2.f * PI + FlutterPhase) * FlutterDegrees;
	Mesh->SetRelativeRotation(Rotation);
}

void AMoneyProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bLaunched || bConsumed || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}
	bConsumed = true;
	if (HasAuthority())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor); Player && !Player->GetbIsDead())
			Player->ApplyPlayerHit(HitDamage, Movement->Velocity.GetSafeNormal2D());
	}
	Destroy();
}
