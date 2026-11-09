#include "Enemy/Bomb/BombEnemy.h"
#include "Enemy/GAS/Abilities/GA_SelfDestruct.h"
#include "Gimmicks/Magnetic/MagneticComponent.h"
#include "Pet/PetCharacter.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"

ABombEnemy::ABombEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Magnetic = CreateDefaultSubobject<UMagneticComponent>(TEXT("Magnetic"));
}

void ABombEnemy::BeginPlay()
{
	Super::BeginPlay();
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ABombEnemy::HandleCapsuleHit);
}

void ABombEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bExploded || !IsAlive()) return;

	// 펫에게 잡힘/놓임 감지
	const bool bGrabbedNow = IsBeingGrabbed();
	if (bGrabbedNow != bGrabbed)
	{
		SetGrabbed(bGrabbedNow);
	}

	// 던져진 상태: 적과 닿으면 폭발
	if (bThrown)
	{
		if (GetWorld()->GetTimeSeconds() > ThrownUntil)
		{
			bThrown = false;
		}
		else
		{
			CheckThrownContact();
			if (bExploded) return;
		}
	}

	// 플레이어 근접 점화
	if (!IsFuseLit() && !bGrabbed && TriggerRadius > 0.f)
	{
		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Player)
		{
			const FVector Delta = Player->GetActorLocation() - GetActorLocation();
			if (FMath::Abs(Delta.Z) <= TriggerHeightTolerance && Delta.SizeSquared2D() <= FMath::Square(TriggerRadius))
			{
				TriggerSelfDestruct();
			}
		}
	}
}

void ABombEnemy::TriggerSelfDestruct()
{
	if (bExploded || !IsAlive() || IsFuseLit() || !SelfDestructAbility || !ASC) return;

	// 패턴 세트에 없더라도 직접 부여해서 사용
	if (!ASC->FindAbilitySpecFromClass(SelfDestructAbility))
	{
		ASC->GiveAbility(FGameplayAbilitySpec(SelfDestructAbility, 1, INDEX_NONE, this));
	}
	ASC->TryActivateAbilityByClass(SelfDestructAbility);
}

void ABombEnemy::DetonateNow()
{
	if (bExploded) return;

	if (UGA_SelfDestruct* Active = ActiveSelfDestruct.Get())
	{
		Active->DetonateNow();
	}
	else if (SelfDestructAbility)
	{
		SelfDestructAbility.GetDefaultObject()->Explode(this);   // GA가 안 켜져 있으면 CDO 설정으로 폭발
	}
}

void ABombEnemy::SetActiveSelfDestruct(UGA_SelfDestruct* Ability)
{
	ActiveSelfDestruct = Ability;
}

void ABombEnemy::MarkExploded()
{
	bExploded = true;
	LifeState = EEnemyLifeState::Dying;   // 연쇄 폭발에서 IsAlive() = false
}

void ABombEnemy::HandleExploded()
{
	LifeState = EEnemyLifeState::Dead;

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Exploded"));
		}
		AI->StopMovement();
	}
	if (ASC)
	{
		ASC->CancelAllAbilities();
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 폭발 처리 콜스택이 끝난 뒤 제거
	SetLifeSpan(0.1f);
}

void ABombEnemy::BeginDeath()
{
	if (bExploded) return;

	// 예약 폭발 전에 죽음 → 설정에 따라 그 자리에서 폭발
	if (SelfDestructAbility && SelfDestructAbility.GetDefaultObject()->ShouldExplodeOnDeath())
	{
		DetonateNow();
		return;
	}
	GetMesh()->SetVisibility(false, true);
	Super::BeginDeath();   // 불발: 일반 사망 (진행 중인 GA는 여기서 취소되며 연출 복구)
}

bool ABombEnemy::IsBeingGrabbed() const
{
	if (!Magnetic || !Magnetic->IsMagnetEnabled()) return false;

	// 펫 자석은 끌어오는 동안 액터 충돌을 끄고, 도착하면 CarryPoint에 붙임
	if (!GetActorEnableCollision()) return true;
	const APetCharacter* Pet = CachedPet.Get();
	return Pet && Pet->GetCarriedActor() == this;
}

void ABombEnemy::SetGrabbed(bool bNewGrabbed)
{
	bGrabbed = bNewGrabbed;
	UCharacterMovementComponent* CMC = GetCharacterMovement();

	if (bGrabbed)
	{
		if (!CachedPet.IsValid())
		{
			CachedPet = Cast<APetCharacter>(UGameplayStatics::GetActorOfClass(this, APetCharacter::StaticClass()));
		}

		// 잡으려는 순간 점화
		TriggerSelfDestruct();

		// 펫이 옮기는 동안 중력/AI가 끼어들지 않게
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			AI->StopMovement();
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Grabbed"));
			}
		}
		CMC->StopMovementImmediately();
		CMC->SetMovementMode(MOVE_Flying);
	}
	else
	{
		// 놓임/던져짐: 펫 자석이 LaunchCharacter로 날려줌. 일정 시간 동안 적과 닿으면 폭발
		CMC->SetMovementMode(MOVE_Falling);
		bThrown = true;
		ThrownUntil = GetWorld()->GetTimeSeconds() + ThrownWindowSeconds;
	}
}

void ABombEnemy::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	bThrown = false;
}

bool ABombEnemy::IsValidThrowVictim(const AActor* Actor) const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor);
	return Enemy && Enemy != this && Enemy->IsAlive();
}

void ABombEnemy::HandleCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bThrown && !bExploded && IsValidThrowVictim(OtherActor))
	{
		DetonateNow();
	}
}

void ABombEnemy::CheckThrownContact()
{
	// 캡슐끼리 스치기만 하는 경우도 잡도록 살짝 큰 구로 검사
	const float Probe = GetCapsuleComponent()->GetScaledCapsuleRadius() + 40.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BombThrownContact), false);
	Params.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Probe), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (IsValidThrowVictim(Overlap.GetActor()))
		{
			DetonateNow();
			return;
		}
	}
}
