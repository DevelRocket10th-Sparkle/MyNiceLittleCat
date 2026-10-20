/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 보스 레이저 테스트용 발사기 구현

#include "Gimmicks/Boss/Test/Test_LaserEmitter.h"
#include "Gimmicks/Boss/Laser/BossLaserComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ATest_LaserEmitter::ATest_LaserEmitter()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Laser = CreateDefaultSubobject<UBossLaserComponent>(TEXT("Laser"));
	Laser->SetupAttachment(Body);
}

void ATest_LaserEmitter::BeginPlay()
{
	Super::BeginPlay();

	Laser->OnLaserDamaged.AddDynamic(this, &ATest_LaserEmitter::HandleLaserDamaged);

	if (bBindTestKeys)
	{
		if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			EnableInput(PlayerController);
			if (InputComponent)
			{
				InputComponent->BindKey(ToggleLaserKey, IE_Pressed, this, &ATest_LaserEmitter::ToggleLaser);
				InputComponent->BindKey(RetargetKey, IE_Pressed, this, &ATest_LaserEmitter::Retarget);
				InputComponent->BindKey(ToggleTrackingKey, IE_Pressed, this, &ATest_LaserEmitter::ToggleTracking);
			}
		}
	}

	if (bFireOnBeginPlay)
	{
		Retarget();
	}
}

void ATest_LaserEmitter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// GA가 매 틱 조준을 갱신하는 경우 흉내
	if (bTrackTarget && Laser->IsLaserActive())
	{
		if (const AActor* Target = GetTargetActor())
		{
			Laser->SetTargetLocation(Target->GetActorLocation());
		}
	}

	if (GEngine)
	{
		const AActor* HitActor = Laser->GetHitActor();

		const FString Message = FString::Printf(TEXT("[레이저 테스트] %s | 추적 %s | 맞은 액터 %s | 누적 피해 %.1f  (F 발사/정지, G 다시 조준, H 추적)"),
			Laser->IsLaserActive() ? TEXT("ON") : TEXT("OFF"),
			bTrackTarget ? TEXT("ON") : TEXT("OFF"),
			HitActor ? *HitActor->GetActorNameOrLabel() : TEXT("-"),
			TotalDamage);
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::Orange, Message);
	}
}

void ATest_LaserEmitter::ToggleLaser()
{
	if (Laser->IsLaserActive())
	{
		Laser->StopLaser();
	}
	else
	{
		Retarget();
	}
}

void ATest_LaserEmitter::Retarget()
{
	if (AActor* Target = GetTargetActor())
	{
		Laser->StartLaserAtActor(Target);
	}
	else
	{
		// 목표가 없으면 발사기 정면
		Laser->StartLaser(Laser->GetComponentLocation() + GetActorForwardVector() * 100.f);
	}
}

void ATest_LaserEmitter::ToggleTracking()
{
	bTrackTarget = !bTrackTarget;
}

AActor* ATest_LaserEmitter::GetTargetActor() const
{
	if (IsValid(TargetActor))
	{
		return TargetActor;
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	return PlayerController ? PlayerController->GetPawn() : nullptr;
}

void ATest_LaserEmitter::HandleLaserDamaged(UBossLaserComponent* InLaser, AActor* DamagedActor, float Damage)
{
	TotalDamage += Damage;
}
