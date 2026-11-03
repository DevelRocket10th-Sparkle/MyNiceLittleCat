#include "Gimmicks/Turret/TurretProjectile.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

#include "Pet/PetCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Enemy/EnemyCharacter.h"

ATurretProjectile::ATurretProjectile()
{
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionObjectType(ECC_GameTraceChannel1);
	Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	SetRootComponent(Collision);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded()) Visual->SetStaticMesh(SphereMesh.Object);
	Visual->SetRelativeScale3D(FVector(0.5f));

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 1000.f;
	Movement->MaxSpeed = 1000.f;
	Movement->ProjectileGravityScale = 0.f;

	InitialLifeSpan = 3.f;
}

void ATurretProjectile::BeginPlay()
{
	Super::BeginPlay();

	Collision->OnComponentHit.AddDynamic(this, &ATurretProjectile::HandleHit);

	// 포탑·펫은 항상 통과. 적대 탄은 적을, 아군 탄은 플레이어(Instigator)를 통과
	Collision->IgnoreActorWhenMoving(GetOwner(), true);
	Collision->IgnoreActorWhenMoving(UGameplayStatics::GetActorOfClass(this, APetCharacter::StaticClass()), true);

	if (bHostile)
	{
		TArray<AActor*> Enemies;
		UGameplayStatics::GetAllActorsOfClass(this, AEnemyCharacter::StaticClass(), Enemies);
		for (AActor* Enemy : Enemies) Collision->IgnoreActorWhenMoving(Enemy, true);
	}
	else
	{
		Collision->IgnoreActorWhenMoving(GetInstigator(), true);
	}
}

void ATurretProjectile::HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	UE_LOG(LogTemp, Warning, TEXT("[TurretProj] hit %s"), *GetNameSafe(OtherActor));
	if (bHostile)
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor))
		{
			Player->ApplyPlayerHit(Damage, GetVelocity().GetSafeNormal2D());
		}
		Destroy();
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (TargetASC && DamageEffectClass && !OtherActor->IsA<APlayerCharacter>())
	{
		FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
		Context.AddSourceObject(this);

		FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), Damage);
			TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	Destroy();
}
