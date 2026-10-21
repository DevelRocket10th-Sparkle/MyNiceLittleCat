/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 보스 패턴 자성 코인 구현

#include "Gimmicks/Boss/MagneticCoin.h"
#include "Gimmicks/Boss/BarrierGenerator.h"
#include "Gimmicks/Boss/DamageTypes/DamageType_MagneticCoin.h"
#include "Gimmicks/Boss/DamageTypes/DamageType_MagneticCoinFall.h"
#include "Gimmicks/Magnetic/MagneticComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AMagneticCoin::AMagneticCoin()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->SetLinearDamping(1.f);
	Mesh->SetAngularDamping(1.f);

	MagneticComponent = CreateDefaultSubobject<UMagneticComponent>(TEXT("Magnetic"));

	// 템플릿 바닥 등은 WorldDynamic(BlockAllDynamic)인 경우가 있어 둘 다 바닥으로 인식
	GroundObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	GroundObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	FallDamageTypeClass = UDamageType_MagneticCoinFall::StaticClass();
	FallDamageTargetClasses.Add(APlayerCharacter::StaticClass());

	DamageTypeClass = UDamageType_MagneticCoin::StaticClass();
	DamageTargetClasses.Add(ABarrierGenerator::StaticClass());
}

void AMagneticCoin::BeginPlay()
{
	Super::BeginPlay();

	LandedCollisionProfile = Mesh->GetCollisionProfileName();

	Mesh->OnComponentHit.AddDynamic(this, &AMagneticCoin::HandleMeshHit);
	Mesh->OnComponentBeginOverlap.AddDynamic(this, &AMagneticCoin::HandleMeshBeginOverlap);
	MagneticComponent->OnMagneticForce.AddDynamic(this, &AMagneticCoin::HandleMagneticForce);

	if (bFallOnBeginPlay)
	{
		StartFall(EMagneticPolarity::None);
	}
}

void AMagneticCoin::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CoinState == EMagneticCoinState::Falling)
	{
		TickFalling(DeltaTime);
		return;
	}

	UpdateHeldState();

	PreviousSpeed = Mesh->GetPhysicsLinearVelocity().Size();
}

void AMagneticCoin::UpdateHeldState()
{
	const bool bSimulating = Mesh->IsSimulatingPhysics();

	switch (CoinState)
	{
	case EMagneticCoinState::Landed:
	case EMagneticCoinState::Thrown:
		// 착지 후 코인 스스로는 물리를 끄지 않으므로, 꺼졌다면 외부(펫 자석 잡기)가 직접 옮기는 중
		if (!bSimulating)
		{
			SetCoinState(EMagneticCoinState::Held);
		}
		else if (CoinState == EMagneticCoinState::Thrown && GetWorld()->GetTimeSeconds() - ThrownTime > ThrowGraceTime)
		{
			SetCoinState(EMagneticCoinState::Landed);
		}
		break;

	case EMagneticCoinState::Held:
		// 물리가 다시 켜지면 놓이거나 던져진 것 (놓을 때 속도가 느리면 MinImpactSpeed에서 걸러짐)
		if (bSimulating)
		{
			ThrownTime = GetWorld()->GetTimeSeconds();
			SetCoinState(EMagneticCoinState::Thrown);
		}
		break;

	default:
		break;
	}
}

void AMagneticCoin::StartFall(EMagneticPolarity NewPolarity)
{
	const FVector Start = GetActorLocation();
	FVector LandLocation = Start - FVector::UpVector * GroundTraceDistance;

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : GroundObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MagneticCoinGround), false, this);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, LandLocation, ObjectParams, QueryParams))
	{
		LandLocation = Hit.ImpactPoint;
	}

	StartFallTo(NewPolarity, LandLocation);
}

void AMagneticCoin::StartFallTo(EMagneticPolarity NewPolarity, const FVector& LandLocation)
{
	if (NewPolarity != EMagneticPolarity::None)
	{
		SetPolarity(NewPolarity);
	}

	// 낙하 중에는 물리·자력 없이 Pawn과 겹침만 판정
	// 채널 응답을 직접 바꾸면 프로파일 이름이 그대로 남아 착지 때 복구가 무시되므로 프로파일째 교체
	MagneticComponent->SetMagnetEnabled(false);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionProfileName(FallingCollisionProfile);

	FallHitActors.Reset();
	FallStartLocation = GetActorLocation();

	// 바닥 지점 위로 메시 절반 높이만큼 (코인 중심 기준)
	FallTargetLocation = LandLocation + FVector::UpVector * Mesh->Bounds.BoxExtent.Z;

	SetCoinState(EMagneticCoinState::Falling);
}

void AMagneticCoin::TickFalling(float DeltaTime)
{
	const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), FallTargetLocation, DeltaTime, FallSpeed);
	SetActorLocation(NewLocation);

	if (NewLocation.Equals(FallTargetLocation, 0.5f))
	{
		Land();
	}
}

void AMagneticCoin::Land()
{
	SetActorLocation(FallTargetLocation);

	Mesh->SetCollisionProfileName(LandedCollisionProfile);
	Mesh->SetSimulatePhysics(true);

	// 직선 낙하 이동 속도가 물리 바디에 남아 바닥을 뚫지 않도록 정지 상태로 시작
	Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	MagneticComponent->SetMagnetEnabled(true);

	PreviousSpeed = 0.f;
	SetCoinState(EMagneticCoinState::Landed);
}

void AMagneticCoin::SetCoinState(EMagneticCoinState NewState)
{
	if (CoinState == NewState)
	{
		return;
	}

	CoinState = NewState;
	OnCoinStateChanged.Broadcast(this, NewState);
}

float AMagneticCoin::GetFallProgress() const
{
	if (CoinState != EMagneticCoinState::Falling)
	{
		return 1.f;
	}

	const float TotalDistance = FVector::Dist(FallStartLocation, FallTargetLocation);
	if (TotalDistance <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	const float RemainingDistance = FVector::Dist(GetActorLocation(), FallTargetLocation);
	return FMath::Clamp(1.f - RemainingDistance / TotalDistance, 0.f, 1.f);
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

void AMagneticCoin::HandleMeshBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (CoinState != EMagneticCoinState::Falling || !IsValid(OtherActor) || OtherActor == this || FallDamage <= 0.f)
	{
		return;
	}

	if (!IsActorOfClasses(OtherActor, FallDamageTargetClasses) || FallHitActors.Contains(OtherActor))
	{
		return;
	}

	FallHitActors.Add(OtherActor);

	// TODO(합의): 플레이어가 TakeDamage → ApplyPlayerHit를 처리하게 되면 플레이어도 ApplyDamage로 통일
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor))
	{
		// 피해·경직·무적은 플레이어 쪽에서 처리 (무적 중이면 무시됨), 코인에서 플레이어 쪽으로 밀림
		Player->ApplyPlayerHit(FallDamage, Player->GetActorLocation() - GetActorLocation());
	}
	else
	{
		UGameplayStatics::ApplyDamage(OtherActor, FallDamage, nullptr, this, FallDamageTypeClass);
	}

	OnCoinFallHit.Broadcast(this, OtherActor);
}

void AMagneticCoin::HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if ((CoinState != EMagneticCoinState::Landed && CoinState != EMagneticCoinState::Thrown) || !CanDamageTarget(OtherActor))
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
	if (!IsValid(OtherActor) || OtherActor == this || Damage <= 0.f || !IsActorOfClasses(OtherActor, DamageTargetClasses))
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	if (LastDamageTime >= 0.f && Now - LastDamageTime < DamageCooldown)
	{
		return false;
	}

	if (bRequireMagneticForce && CoinState != EMagneticCoinState::Thrown
		&& (LastMagneticForceTime < 0.f || Now - LastMagneticForceTime > MagneticForceGraceTime))
	{
		return false;
	}

	const float ImpactSpeed = FMath::Max(PreviousSpeed, Mesh->GetPhysicsLinearVelocity().Size());
	return ImpactSpeed >= MinImpactSpeed;
}

bool AMagneticCoin::IsActorOfClasses(const AActor* Actor, const TArray<TSubclassOf<AActor>>& Classes)
{
	if (Classes.IsEmpty())
	{
		return true;
	}

	for (const TSubclassOf<AActor>& Class : Classes)
	{
		if (Class && Actor->IsA(Class))
		{
			return true;
		}
	}

	return false;
}
