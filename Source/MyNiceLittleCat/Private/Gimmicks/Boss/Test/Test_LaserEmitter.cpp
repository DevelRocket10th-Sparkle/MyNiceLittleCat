/// 작성자: 최요환
/// 작성일자: 2026-10-20
///
/// 보스 레이저 테스트용 발사기 구현

#include "Gimmicks/Boss/Test/Test_LaserEmitter.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Gimmicks/Warning/AttackWarningDecal.h"
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

	Laser = CreateDefaultSubobject<ULaserBeamComponent>(TEXT("Laser"));
	Laser->SetupAttachment(Body);
}

void ATest_LaserEmitter::BeginPlay()
{
	Super::BeginPlay();

	// 에셋이 깨져 컴포넌트 포인터가 비면(클래스 이름 변경 후 리다이렉트 없이 저장 등) 크래시 대신 경고만 남기고 동작 중지
	if (!IsValid(Laser))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Laser 컴포넌트가 없습니다. BP를 다시 저장하거나 다시 배치하세요."), *GetName());
		SetActorTickEnabled(false);
		return;
	}

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
				InputComponent->BindKey(FloorStrikeKey, IE_Pressed, this, &ATest_LaserEmitter::FloorStrike);
			}
		}
	}

	if (bFireOnBeginPlay)
	{
		Retarget();
	}

	if (bAutoStrike)
	{
		GetWorldTimerManager().SetTimer(AutoStrikeTimerHandle, this, &ATest_LaserEmitter::FloorStrike, AutoStrikeInterval, true, 0.5f);
	}
}

void ATest_LaserEmitter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideStrikeWarning();

	Super::EndPlay(EndPlayReason);
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

		const FString Message = FString::Printf(TEXT("[레이저 테스트] %s | 추적 %s | 맞은 액터 %s | 누적 피해 %.1f  (F 발사/정지, G 다시 조준, H 추적, J 바닥 일격)"),
			bStrikePending ? TEXT("예고") : (Laser->IsLaserActive() ? TEXT("ON") : TEXT("OFF")),
			bTrackTarget ? TEXT("ON") : TEXT("OFF"),
			HitActor ? *HitActor->GetActorNameOrLabel() : TEXT("-"),
			TotalDamage);
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::Orange, Message);
	}
}

void ATest_LaserEmitter::ToggleLaser()
{
	if (!IsValid(Laser))
	{
		return;
	}

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
	if (!IsValid(Laser))
	{
		return;
	}

	if (AActor* Target = GetTargetActor())
	{
		Laser->StartLaserAtActor(Target);
	}
	else
	{
		// 목표가 없으면 발사기 정면
		Laser->StartLaserForward();
	}
}

void ATest_LaserEmitter::ToggleTracking()
{
	bTrackTarget = !bTrackTarget;
}

void ATest_LaserEmitter::FloorStrike()
{
	if (!IsValid(Laser))
	{
		return;
	}

	const AActor* Target = GetTargetActor();
	if (!Target || !FindFloorUnder(Target, StrikeLocation))
	{
		return;
	}

	// 이전 일격(예고·발사 중)은 취소하고 새 지점으로 다시 시작
	GetWorldTimerManager().ClearTimer(StrikeTimerHandle);
	HideStrikeWarning();
	Laser->StopLaser();

	if (StrikeWarningDecalClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		StrikeWarning = GetWorld()->SpawnActor<AAttackWarningDecal>(StrikeWarningDecalClass, StrikeLocation, FRotator::ZeroRotator, SpawnParams);
		if (StrikeWarning)
		{
			StrikeWarning->ShowCircleWarning(StrikeLocation, StrikeWarningRadius, StrikeWarningTime, StrikeWarningColor);
		}
	}

	bStrikePending = true;

	if (StrikeWarningTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(StrikeTimerHandle, this, &ATest_LaserEmitter::FireStrike, StrikeWarningTime, false);
	}
	else
	{
		FireStrike();
	}
}

void ATest_LaserEmitter::FireStrike()
{
	bStrikePending = false;
	HideStrikeWarning();

	// 예고 때 고정한 지점으로 발사 (그 사이 목표가 움직였으면 피한 것)
	Laser->StartLaser(StrikeLocation, StrikeDuration);
}

void ATest_LaserEmitter::HideStrikeWarning()
{
	if (IsValid(StrikeWarning))
	{
		StrikeWarning->HideWarning();
	}

	StrikeWarning = nullptr;
}

bool ATest_LaserEmitter::FindFloorUnder(const AActor* Target, FVector& OutLocation) const
{
	const FVector Start = Target->GetActorLocation();
	const FVector End = Start - FVector::UpVector * StrikeFloorTraceDistance;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LaserFloorStrike), false, this);
	QueryParams.AddIgnoredActor(Target);

	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, QueryParams))
	{
		return false;
	}

	OutLocation = Hit.ImpactPoint;
	return true;
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

void ATest_LaserEmitter::HandleLaserDamaged(ULaserBeamComponent* InLaser, AActor* DamagedActor, float Damage)
{
	TotalDamage += Damage;
}
