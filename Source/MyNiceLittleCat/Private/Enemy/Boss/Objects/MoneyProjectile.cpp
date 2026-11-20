#include "Enemy/Boss/Objects/MoneyProjectile.h"
#include "Enemy/Combat/EnemyFeedback.h"
#include "Enemy/Combat/EnemyTargetUtils.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"

AMoneyProjectile::AMoneyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 충돌 설정은 Project Settings > Collision의 "EnemyProjectile" 프로필로 지정
	// (채널 번호가 바뀌어도 코드 수정 불필요, 다른 탄환 BP도 같은 프로필 재사용)
	Collision->SetCollisionProfileName(TEXT("EnemyProjectile"));

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

void AMoneyProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* Shooter = GetInstigator())
	{
		Shooter->MoveIgnoreActorRemove(this);
	}

	Super::EndPlay(EndPlayReason);
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

	// 반대 방향도 무시: 보스가 움직이다 자기 탄환에 막혀 멈추는 것 방지 (EndPlay에서 해제)
	if (APawn* Shooter = GetInstigator())
	{
		Shooter->MoveIgnoreActorAdd(this);
	}
	Movement->InitialSpeed = Movement->MaxSpeed = Speed;
	Movement->Velocity = Direction.GetSafeNormal() * Speed;
	Movement->Activate();
	SetLifeSpan(LifeSeconds);

	// 외형이 펄럭여도 궤적은 탄환의 진행 방향을 따른다.
	EnemyFeedback::PlayAttached(TrailCue, Collision);
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

	// 충돌 연출: 맞은 면 바깥쪽을 향하도록
	EnemyFeedback::PlayAtLocation(this, ImpactCue, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
	
	// 대상 판정과 피해 전달은 EnemyTargetUtils에 위임 (탄환은 플레이어 타입을 모름)
	// - 가해자 컨트롤러는 이 탄환의 Instigator(발사한 보스)에서 구함
	if (HasAuthority())
	{
		EnemyTargetUtils::ApplyAttackHit(OtherActor, HitDamage, Movement->Velocity.GetSafeNormal2D(), this);
	}

	Destroy();
}
