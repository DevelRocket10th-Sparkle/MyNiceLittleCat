#include "Gimmicks/Turret/Turret.h"
#include "Gimmicks/Turret/TurretProjectile.h"
#include "Gimmicks/Magnetic/MagneticComponent.h"
#include "Pet/PetCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "NiagaraFunctionLibrary.h"

ATurret::ATurret()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	// 굴러가거나 기울지 않게 Pitch/Roll 축 잠금 (Yaw만 회전 가능)
	Mesh->BodyInstance.bLockXRotation = true;
	Mesh->BodyInstance.bLockYRotation = true;
	Mesh->BodyInstance.DOFMode = EDOFMode::SixDOF;

	Magnetic = CreateDefaultSubobject<UMagneticComponent>(TEXT("Magnetic"));

	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Head"));
	Head->SetupAttachment(Mesh);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(Head);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Head);

	ProjectileClass = ATurretProjectile::StaticClass();
}

void ATurret::BeginPlay()
{
	Super::BeginPlay();
	CachedPet = Cast<APetCharacter>(UGameplayStatics::GetActorOfClass(this, APetCharacter::StaticClass()));
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->OnComponentHit.AddDynamic(this, &ATurret::HandleMeshHit);
	HeadBaseScale = HeadMesh->GetRelativeScale3D();
}

void ATurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (JellyElapsed >= 0.f)
	{
		JellyElapsed += DeltaTime;
		const float W = JellyAmount * FMath::Exp(-JellyDamping * JellyElapsed) * FMath::Cos(JellyFrequency * JellyElapsed);
		HeadMesh->SetRelativeScale3D(HeadBaseScale * FVector(1.f - W, 1.f + W * 0.5f, 1.f + W * 0.5f));

		if (JellyElapsed > 0.6f)
		{
			HeadMesh->SetRelativeScale3D(HeadBaseScale);
			JellyElapsed = -1.f;
		}
	}
	// 펫이 리스폰 등으로 바뀌었으면 다시 찾음
	if (!CachedPet.IsValid())
	{
		CachedPet = Cast<APetCharacter>(UGameplayStatics::GetActorOfClass(this, APetCharacter::StaticClass()));
	}

	// 다른 자성 물체와 같은 CarryPoint에 붙어 있으면 "장착"
	const bool bNowCarried = CachedPet.IsValid() && CachedPet->GetCarriedActor() == this;
	if (bNowCarried != bCarried)
	{
		SetCarried(bNowCarried);
	}

	if (!bLanded && Mesh->IsSimulatingPhysics() && FMath::Abs(Mesh->GetPhysicsLinearVelocity().Z) < 5.f)
	{
		Land();
	}

	const bool bArmed = bLanded && GetWorld()->GetTimeSeconds() - LandedTime >= ArmDelay;
	if (bHostile && !bCarried && !IsBeingPulled() && bArmed)
	{
		TickHostile(DeltaTime);
	}

	if (bCarried)
	{
		Fire();
	}
}

void ATurret::TickHostile(float DeltaTime)
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(DetectRange)) return;

	// Yaw만 회전 (포신은 항상 수평)
	const FRotator Desired(0.f, (Player->GetActorLocation() - Head->GetComponentLocation()).Rotation().Yaw, 0.f);
	const FRotator Current(0.f, Head->GetComponentRotation().Yaw, 0.f);
	Head->SetWorldRotation(FMath::RInterpTo(Current, Desired, DeltaTime, HeadTurnSpeed));

	const float AimDot = FVector::DotProduct(Head->GetForwardVector(), Desired.Vector());
	const float Now = GetWorld()->GetTimeSeconds();
	if (AimDot >= FMath::Cos(FMath::DegreesToRadians(AimToleranceDeg)) && Now >= NextHostileFireTime)
	{
		LastFireTime = Now;
		NextHostileFireTime = Now + FMath::FRandRange(HostileFireIntervalMin, FMath::Max(HostileFireIntervalMin, HostileFireIntervalMax));
		SpawnProjectile(true);
	}
}

void ATurret::SetCarried(bool bNewCarried)
{
	bCarried = bNewCarried;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	if (bCarried)
	{
		if (bHostile)
		{
			bHostile = false;          // 잡히는 순간 아군으로 전환
			CurrentAmmo = MaxAmmo;     // 처음 잡을 때만 장전 (다시 잡아도 리필 안 됨)
		}
		Head->SetRelativeRotation(FRotator::ZeroRotator);   // 포신을 몸체 정면(=펫 정면)으로

		// 포구를 펫 정면으로 맞춤. CarryPoint에 붙어 있으므로 이후엔 펫이 도는 대로 같이 돎
		if (APetCharacter* Pet = CachedPet.Get())
		{
			SetActorRotation(FRotator(0.f, Pet->GetActorRotation().Yaw, 0.f));
			AttachToComponent(Pet->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}

		EnableInput(PC);
		if (InputComponent && !bFireKeyBound)
		{
			InputComponent->BindKey(FireKey, IE_Pressed, this, &ATurret::Fire);
			bFireKeyBound = true;
		}
	}
	else
	{
		DisableInput(PC);

		// 놓는 순간 수평으로 세우고 회전 속도 제거 → 똑바로 떨어져 착지
		SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f), ETeleportType::TeleportPhysics);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

void ATurret::Fire()
{
	if (!bCarried || !ProjectileClass || CurrentAmmo <= 0) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < FireCooldown) return;
	LastFireTime = Now;

	--CurrentAmmo;
	SpawnProjectile(false);
}
void ATurret::SpawnProjectile(bool bHostileShot)
{
	if (!ProjectileClass) return;

	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* InstigatorPawn = bHostileShot ? nullptr : (PC ? PC->GetPawn() : nullptr);

	FTransform SpawnTM = Muzzle->GetComponentTransform();
	if (!bHostileShot && PlayerSpreadDeg > 0.f)
	{
		FRotator Rot = SpawnTM.Rotator();
		Rot.Yaw += FMath::FRandRange(-PlayerSpreadDeg, PlayerSpreadDeg);
		SpawnTM.SetRotation(Rot.Quaternion());
	}

	ATurretProjectile* Proj = GetWorld()->SpawnActorDeferred<ATurretProjectile>(
		ProjectileClass, SpawnTM, this, InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Proj) return;

	Proj->SetHostile(bHostileShot);
	Proj->SetDamageEffect(DamageEffectClass);
	Proj->FinishSpawning(SpawnTM);;
	if (MuzzleFlashEffect)
	{
		// 총구에 붙여서 헤드가 돌아도 따라감
		UNiagaraFunctionLibrary::SpawnSystemAttached(MuzzleFlashEffect, Muzzle, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, MuzzleFlashScale,
			EAttachLocation::SnapToTarget, true, ENCPoolMethod::AutoRelease);
	}
	if (FireSound && !bHostileShot)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Muzzle->GetComponentLocation());
	}
	JellyElapsed = 0.f;
}

void ATurret::HandleMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bLanded && Hit.ImpactNormal.Z > 0.7f)
	{
		Land();
	}
}

void ATurret::Land()
{
	bLanded = true;
	LandedTime = GetWorld()->GetTimeSeconds();
	NextHostileFireTime = LandedTime + ArmDelay + FMath::FRandRange(0.f, HostileFireIntervalMax);
	if (Mesh->IsSimulatingPhysics())
	{
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}
