/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 보스 패턴 자성 코인 구현

#include "Gimmicks/Boss/MagneticCoin.h"
#include "Gimmicks/Boss/BarrierGenerator.h"
#include "Gimmicks/Boss/DamageTypes/DamageType_MagneticCoin.h"
#include "Gimmicks/Magnetic/MagneticComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AMagneticCoin::AMagneticCoin()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetLinearDamping(1.f);
	Mesh->SetAngularDamping(1.f);

	MagneticComponent = CreateDefaultSubobject<UMagneticComponent>(TEXT("Magnetic"));

	DamageTypeClass = UDamageType_MagneticCoin::StaticClass();
	DamageTargetClasses.Add(ABarrierGenerator::StaticClass());
}

void AMagneticCoin::BeginPlay()
{
	Super::BeginPlay();

	Mesh->OnComponentHit.AddDynamic(this, &AMagneticCoin::HandleMeshHit);
	MagneticComponent->OnMagneticForce.AddDynamic(this, &AMagneticCoin::HandleMagneticForce);
}

void AMagneticCoin::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PreviousSpeed = Mesh->GetPhysicsLinearVelocity().Size();
}

void AMagneticCoin::SetPolarity(EMagneticPolarity NewPolarity)
{
	MagneticComponent->SetPolarity(NewPolarity);
}

EMagneticPolarity AMagneticCoin::GetPolarity() const
{
	return MagneticComponent->GetPolarity();
}

void AMagneticCoin::HandleMagneticForce(UMagneticComponent* InMagneticComponent, FVector Force)
{
	LastMagneticForceTime = GetWorld()->GetTimeSeconds();
}

void AMagneticCoin::HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!CanDamageTarget(OtherActor))
	{
		return;
	}

	LastDamageTime = GetWorld()->GetTimeSeconds();

	// 대상이 피해를 거부하면(허용되지 않은 피해 타입 등) 코인은 남김
	const float AppliedDamage = UGameplayStatics::ApplyDamage(OtherActor, Damage, nullptr, this, DamageTypeClass);
	if (AppliedDamage <= 0.f)
	{
		return;
	}

	OnCoinDamaged.Broadcast(this, OtherActor);

	if (bDestroyOnDamage)
	{
		Destroy();
	}
}

bool AMagneticCoin::CanDamageTarget(const AActor* OtherActor) const
{
	if (!IsValid(OtherActor) || OtherActor == this || Damage <= 0.f || !IsDamageTargetClass(OtherActor))
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	if (LastDamageTime >= 0.f && Now - LastDamageTime < DamageCooldown)
	{
		return false;
	}

	if (bRequireMagneticForce && (LastMagneticForceTime < 0.f || Now - LastMagneticForceTime > MagneticForceGraceTime))
	{
		return false;
	}

	const float ImpactSpeed = FMath::Max(PreviousSpeed, Mesh->GetPhysicsLinearVelocity().Size());
	return ImpactSpeed >= MinImpactSpeed;
}

bool AMagneticCoin::IsDamageTargetClass(const AActor* OtherActor) const
{
	if (DamageTargetClasses.IsEmpty())
	{
		return true;
	}

	for (const TSubclassOf<AActor>& TargetClass : DamageTargetClasses)
	{
		if (TargetClass && OtherActor->IsA(TargetClass))
		{
			return true;
		}
	}

	return false;
}
