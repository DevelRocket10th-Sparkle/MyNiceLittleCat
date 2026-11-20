#include "Enemy/Boss/Objects/MoneyGun.h"
#include "Enemy/Boss/Objects/MoneyProjectile.h"
#include "Enemy/Data/MoneyGunPatternSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

#include "Enemy/Combat/EnemyFeedback.h"

AMoneyGun::AMoneyGun()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetAbsolute(false, true, true);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(Mesh);
	MuzzlePoint->SetRelativeLocation(FVector(100.f, 0.f, 0.f));
	LoadPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LoadPoint"));
	LoadPoint->SetupAttachment(Mesh);
}

bool AMoneyGun::StartPattern(UMoneyGunPatternSettings* InSettings, AActor* InTarget, float InDamage)
{
	if (!InSettings || !InSettings->IsValidConfiguration() || !IsValid(InTarget) || Settings)
	{
		return false;
	}
	Settings = InSettings;
	Target = InTarget;
	Damage = InDamage;
	GunScale = Mesh->GetRelativeScale3D();
	SetActorRotation((Target->GetActorLocation() + Settings->TargetOffset - MuzzlePoint->GetComponentLocation()).Rotation());
	if (Settings->AppearSeconds > 0.f)
	{
		Mesh->SetRelativeScale3D(FVector::ZeroVector);
	}

	// 등장 연출
	EnemyFeedback::PlayAtLocation(this, Settings->AppearCue, GetActorLocation(), GetActorRotation());
	return true;
}

bool AMoneyGun::CreateLoadingVisuals()
{
	const AMoneyProjectile* Prototype = Settings->ProjectileClass->GetDefaultObject<AMoneyProjectile>();
	const UStaticMeshComponent* Source = Prototype->GetMoneyMesh();
	if (!Source || !Source->GetStaticMesh())
	{
		return false;
	}
	for (int32 Index = 0; Index < Settings->ProjectileCount; ++Index)
	{
		UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(this);
		Visual->SetupAttachment(Root);
		Visual->SetStaticMesh(Source->GetStaticMesh());
		for (int32 Material = 0; Material < Source->GetNumMaterials(); ++Material)
		{
			Visual->SetMaterial(Material, Source->GetMaterial(Material));
		}
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetGenerateOverlapEvents(false);
		Visual->RegisterComponent();
		const FVector Start = FMath::VRand() * Settings->LoadRadius;
		Visual->SetRelativeLocation(Start);
		Visual->SetRelativeRotation(FRotator(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-180.f, 180.f), 0.f));
		Visual->SetRelativeScale3D(Source->GetRelativeScale3D());
		LoadingStarts.Add(Start);
		LoadingScales.Add(Source->GetRelativeScale3D());
		LoadingVisuals.Add(Visual);
	}
	return true;
}

void AMoneyGun::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Settings || bFinished || bFailed)
	{
		return;
	}
	if (!Target.IsValid())
	{
		bFailed = true;
		return;
	}
	UpdateAim(DeltaSeconds);
	Elapsed += DeltaSeconds;
	if (ShotsFired == Settings->ProjectileCount)
	{
		UpdateDisappearance();
		return;
	}
	if (UpdateLoading(DeltaSeconds))
	{
		UpdateFiring();
	}
}

void AMoneyGun::UpdateAim(float DeltaSeconds)
{
	const FVector AimLocation = Target->GetActorLocation() + Settings->TargetOffset;
	const FRotator DesiredRotation = (AimLocation - MuzzlePoint->GetComponentLocation()).Rotation();
	SetActorRotation(FMath::RInterpConstantTo(
		GetActorRotation(), DesiredRotation, DeltaSeconds, Settings->AimDegreesPerSecond));
}

bool AMoneyGun::UpdateLoading(float DeltaSeconds)
{
	const float AppearAlpha = Settings->AppearSeconds > 0.f ? FMath::Clamp(Elapsed / Settings->AppearSeconds, 0.f, 1.f) : 1.f;
	Mesh->SetRelativeScale3D(GunScale * AppearAlpha);
	if (Elapsed < Settings->AppearSeconds)
	{
		return false;
	}
	if (!bLoadingCreated)
	{
		bLoadingCreated = true;
		if (!CreateLoadingVisuals())
		{
			bFailed = true;
			return false;
		}

		// 장전 시작 연출: 지폐가 모여드는 지점
		LoadingFeedback = EnemyFeedback::PlayAttached(Settings->LoadStartCue, LoadPoint);
	}
	const float LoadAlpha = FMath::Clamp((Elapsed - Settings->AppearSeconds) / Settings->LoadSeconds, 0.f, 1.f);
	const FVector End = Root->GetComponentTransform().InverseTransformPosition(LoadPoint->GetComponentLocation());
	for (int32 Index = 0; Index < LoadingVisuals.Num(); ++Index)
	{
		// 탄환은 동시에 표시하고, 장전 후반부에는 순서대로 흡수한다.
		const float Delay = 0.4f * Index / FMath::Max(1, Settings->ProjectileCount - 1);
		const float Alpha = FMath::Clamp((LoadAlpha - Delay) / (1.f - Delay), 0.f, 1.f);
		const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);
		const FVector Arc = FVector::CrossProduct(LoadingStarts[Index], FVector::UpVector) * FMath::Sin(Alpha * PI) * 0.5f;
		LoadingVisuals[Index]->SetRelativeLocation(FMath::Lerp(LoadingStarts[Index], End, Smooth) + Arc);
		LoadingVisuals[Index]->AddLocalRotation(FRotator(0.f, DeltaSeconds * 180.f, DeltaSeconds * 90.f));
		LoadingVisuals[Index]->SetRelativeScale3D(LoadingScales[Index] * (1.f - FMath::Pow(Alpha, 4.f)));
	}
	if (LoadAlpha < 1.f)
	{
		return false;
	}
	if (!bLoadFinished)
	{
		bLoadFinished = true;
		LoadingFeedback.Stop(0.04f);
		ClearLoadingVisuals();
		// 장전 완료: LoadPoint에서 소리와 이펙트
		EnemyFeedback::PlayAtLocation(this, Settings->LoadedCue, LoadPoint->GetComponentLocation(), GetActorRotation());
	}
	return true;
}

void AMoneyGun::UpdateFiring()
{
	const float FireStart = Settings->AppearSeconds + Settings->LoadSeconds + Settings->PreFireSeconds;
	// 프레임 지연 후에도 한 프레임에 한 발만 발사하여 발사 간격을 유지한다.
	if (ShotsFired < Settings->ProjectileCount && Elapsed >= FireStart + ShotsFired * Settings->ShotInterval)
	{
		if (!Fire())
		{
			bFailed = true;
			return;
		}
		++ShotsFired;
		Elapsed = FireStart + (ShotsFired - 1) * Settings->ShotInterval;
	}
	if (ShotsFired == Settings->ProjectileCount)
	{
		UpdateDisappearance();
	}
}

void AMoneyGun::UpdateDisappearance()
{
	// 마지막 발사를 기준으로 등장 스케일 연출을 거꾸로 재생한다. 날아간 지폐는 유지한다.
	const float LastShotTime = Settings->GetExecutionSeconds() - Settings->AppearSeconds;

	// 퇴장 연출은 사라지기 시작할 때 한 번
	if (!bDisappearStarted)
	{
		bDisappearStarted = true;
		EnemyFeedback::PlayAtLocation(this, Settings->DisappearCue, GetActorLocation(), GetActorRotation());
	}
	const float Alpha = Settings->AppearSeconds > 0.f
		? FMath::Clamp((Elapsed - LastShotTime) / Settings->AppearSeconds, 0.f, 1.f)
		: 1.f;
	Mesh->SetRelativeScale3D(GunScale * (1.f - Alpha));
	if (Alpha >= 1.f)
	{
		SetActorHiddenInGame(true);
		bFinished = true;
	}
}

bool AMoneyGun::Fire()
{
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = GetInstigator();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMoneyProjectile* Projectile = GetWorld()->SpawnActor<AMoneyProjectile>(Settings->ProjectileClass,
		MuzzlePoint->GetComponentLocation(), MuzzlePoint->GetComponentRotation(), Params);
	if (!Projectile)
	{
		return false;
	}
	Projectiles.Add(Projectile);
	Projectile->Launch(MuzzlePoint->GetForwardVector(), Settings->ProjectileSpeed, Damage, Settings->ProjectileLifeSeconds);
	// 발사: 총구 위치에서 소리와 이펙트
	EnemyFeedback::PlayAtLocation(this, Settings->ShotCue, MuzzlePoint->GetComponentLocation(), MuzzlePoint->GetComponentRotation());
	OnShot();
	return true;
}

void AMoneyGun::ClearLoadingVisuals()
{
	for (UStaticMeshComponent* Visual : LoadingVisuals)
	{
		if (IsValid(Visual))
		{
			Visual->DestroyComponent();
		}
	}
	LoadingVisuals.Reset();
	LoadingStarts.Reset();
	LoadingScales.Reset();
}

void AMoneyGun::ClearProjectiles()
{
	for (const TWeakObjectPtr<AMoneyProjectile>& Projectile : Projectiles)
	{
		if (Projectile.IsValid())
		{
			Projectile->Destroy();
		}
	}
	Projectiles.Reset();
}

void AMoneyGun::EndPlay(const EEndPlayReason::Type Reason)
{
	LoadingFeedback.Stop(0.f);
	ClearLoadingVisuals();
	Super::EndPlay(Reason);
}
