/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 스테이지 레이저 발사기 구현

#include "Gimmicks/Stage/Laser/GimmickLaserEmitter.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Enemy/EnemyCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

AGimmickLaserEmitter::AGimmickLaserEmitter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Laser = CreateDefaultSubobject<ULaserBeamComponent>(TEXT("Laser"));
	Laser->SetupAttachment(Mesh);
	Laser->SetRelativeLocation(FVector(0.f, 0.f, BeamHeight));

	ApplyEmitterSettings();
}

void AGimmickLaserEmitter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 루트 스케일과 상관없이 바닥(액터 원점)에서 BeamHeight 높이로 (수평 위치·회전은 BP 배치 유지)
	if (Laser)
	{
		FVector LaserLocation = Laser->GetComponentLocation();
		LaserLocation.Z = GetActorLocation().Z + BeamHeight;
		Laser->SetWorldLocation(LaserLocation);
	}

	ApplyEmitterSettings();
}

void AGimmickLaserEmitter::BeginPlay()
{
	// 베이스 BeginPlay가 시작 상태(bStartActivated)로 발사하므로 그 전에 설정
	ApplyEmitterSettings();

	if (Laser)
	{
		Laser->SetEnableReflection(true);
		Laser->SetFollowComponentRotation(true);

		if (bDamagePlayer)
		{
			TArray<TSubclassOf<AActor>> Targets;
			Targets.Add(APlayerCharacter::StaticClass());
			if (bDamageEnemies)
			{
				Targets.Add(AEnemyCharacter::StaticClass());
			}
			Laser->SetDamageTargetClasses(Targets);
			Laser->SetDamagePerHit(DamagePerHit);
		}
		else
		{
			// 퍼즐용 빛: Pawn을 통과해 플레이어·펫이 지나가도 수신기가 꺼지지 않음, 피해 없음
			TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = Laser->GetTraceObjectTypes();
			ObjectTypes.Remove(UEngineTypes::ConvertToObjectType(ECC_Pawn));
			Laser->SetTraceObjectTypes(ObjectTypes);
			Laser->SetDamagePerHit(0.f);
		}
	}

	Super::BeginPlay();

	SetActorTickEnabled(!FMath::IsNearlyZero(RotationSpeed));

	if (EmitMode == EGimmickLaserEmitMode::Pulse)
	{
		// 시작 시점을 주기 안에서 밀어 엇박자로 둠
		const float Cycle = PulseOnTime + PulseOffTime;
		const float Phase = FMath::Fmod(PulseStartOffset, Cycle);
		const bool bStartOn = Phase < PulseOnTime;
		const float Remaining = bStartOn ? PulseOnTime - Phase : Cycle - Phase;

		SetActivated(bStartOn, this);
		GetWorldTimerManager().SetTimer(PulseTimer, this, &AGimmickLaserEmitter::AdvancePulse, Remaining, false);
		if (bStartOn && PulseWarningTime > 0.f && Remaining > PulseWarningTime)
		{
			GetWorldTimerManager().SetTimer(PulseWarningTimer, this, &AGimmickLaserEmitter::HandlePulseWarning, Remaining - PulseWarningTime, false);
		}
	}
}

void AGimmickLaserEmitter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PulseTimer);
	GetWorldTimerManager().ClearTimer(PulseWarningTimer);

	Super::EndPlay(EndPlayReason);
}

void AGimmickLaserEmitter::ApplyEmitterSettings()
{
	// Always: 시작부터 켜짐 / Signal: 신호로만 켜고 끔 / Pulse: 타이머가 켜고 끔
	bReceiveSignals = EmitMode == EGimmickLaserEmitMode::Signal;
	if (EmitMode == EGimmickLaserEmitMode::Always)
	{
		bStartActivated = true;
	}
	else
	{
		// Pulse는 타이머가, Signal은 받은 신호가 켜고 끔 (Always 기본값 true가 남아 있으면 신호 없이 켜진 채 시작함)
		// Signal로 처음부터 켜 두려면 bInvertSignal 사용
		bStartActivated = false;
	}
}

void AGimmickLaserEmitter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 레이저는 컴포넌트 정면을 따라가므로 컴포넌트만 돌림
	if (Laser)
	{
		Laser->AddLocalRotation(FRotator(0.f, RotationSpeed * DeltaTime, 0.f));
	}
}

void AGimmickLaserEmitter::HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator)
{
	Super::HandleActivationChanged(bNewActivated, ChangeInstigator);

	if (!Laser)
	{
		return;
	}

	if (bNewActivated)
	{
		Laser->StartLaserForward();
	}
	else
	{
		Laser->StopLaser();
	}
}

void AGimmickLaserEmitter::AdvancePulse()
{
	SetActivated(!IsActivated(), this);

	const bool bOn = IsActivated();
	const float Duration = bOn ? PulseOnTime : PulseOffTime;
	GetWorldTimerManager().SetTimer(PulseTimer, this, &AGimmickLaserEmitter::AdvancePulse, Duration, false);

	if (bOn && PulseWarningTime > 0.f && PulseOnTime > PulseWarningTime)
	{
		GetWorldTimerManager().SetTimer(PulseWarningTimer, this, &AGimmickLaserEmitter::HandlePulseWarning, PulseOnTime - PulseWarningTime, false);
	}
}

void AGimmickLaserEmitter::HandlePulseWarning()
{
	K2_OnPulseWarning();
}
