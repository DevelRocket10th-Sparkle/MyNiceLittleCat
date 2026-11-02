/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 레이저 수신기 구현

#include "Gimmicks/Stage/Laser/GimmickLaserReceiver.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"

AGimmickLaserReceiver::AGimmickLaserReceiver()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	ReceiveDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("ReceiveDirection"));
	ReceiveDirection->SetupAttachment(Mesh);
	ReceiveDirection->SetHiddenInGame(true);
	ReceiveDirection->ArrowColor = FColor(80, 200, 255);

	// 빔으로만 켜짐 (다른 기믹 신호는 받지 않음), 한 번 켠 수신기는 세이브에 저장
	bReceiveSignals = false;
	bSaveState = true;
}

void AGimmickLaserReceiver::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyReceiveMode();
}

void AGimmickLaserReceiver::BeginPlay()
{
	ApplyReceiveMode();

	Super::BeginPlay();

	// 세이브로 켜진 채 복원됐으면 충전도 가득
	SetCharge(IsActivated() ? 1.f : 0.f);
}

void AGimmickLaserReceiver::ApplyReceiveMode()
{
	// Hold는 빔이 있어야 유지되므로 저장하지 않음
	if (ReceiveMode == EGimmickLaserReceiveMode::Hold)
	{
		bSaveState = false;
	}
}

void AGimmickLaserReceiver::ReceiveLaserHit_Implementation(ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount)
{
	// Latch로 이미 켜졌으면 더 볼 필요 없음
	if (ReceiveMode == EGimmickLaserReceiveMode::Latch && IsActivated())
	{
		return;
	}

	// 같은 레이저라도 거울이 돌면 맞는 면이 바뀌므로 매 틱 다시 판단
	if (AcceptsBeam(Hit, ReflectionCount))
	{
		ReceivingLasers.Add(Laser);
	}
	else
	{
		ReceivingLasers.Remove(Laser);
	}

	UpdateTickEnabled();
}

void AGimmickLaserReceiver::EndLaserHit_Implementation(ULaserBeamComponent* Laser)
{
	ReceivingLasers.Remove(Laser);
	UpdateTickEnabled();
}

bool AGimmickLaserReceiver::AcceptsBeam(const FHitResult& Hit, int32 ReflectionCount) const
{
	if (ReflectionCount < MinReflections)
	{
		return false;
	}

	if (bAcceptAnySide || !ReceiveDirection)
	{
		return true;
	}

	// 들어온 방향의 반대가 정면 화살표와 허용 각도 안이어야 함 (수평 기준)
	const FVector Incoming = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal2D();
	const FVector Front = ReceiveDirection->GetForwardVector().GetSafeNormal2D();
	if (Incoming.IsNearlyZero() || Front.IsNearlyZero())
	{
		return false;
	}

	return FVector::DotProduct(-Incoming, Front) >= FMath::Cos(FMath::DegreesToRadians(ReceiveHalfAngle));
}

bool AGimmickLaserReceiver::IsReceivingBeam() const
{
	for (const TWeakObjectPtr<ULaserBeamComponent>& Laser : ReceivingLasers)
	{
		if (Laser.IsValid() && Laser->IsLaserActive())
		{
			return true;
		}
	}
	return false;
}

void AGimmickLaserReceiver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 끝 통지 없이 사라진 레이저 정리 (발사기 파괴 등)
	for (auto It = ReceivingLasers.CreateIterator(); It; ++It)
	{
		if (!It->IsValid() || !(*It)->IsLaserActive())
		{
			It.RemoveCurrent();
		}
	}

	const bool bReceiving = IsReceivingBeam();
	if (bReceiving != bWasReceiving)
	{
		bWasReceiving = bReceiving;
		if (bReceiving)
		{
			K2_OnBeamReceived();
		}
		else
		{
			K2_OnBeamLost();
		}
	}

	if (bReceiving)
	{
		TimeSinceBeamLost = 0.f;
		if (!IsActivated())
		{
			SetCharge(ChargeTime > 0.f ? Charge + DeltaTime / ChargeTime : 1.f);
			if (Charge >= 1.f)
			{
				SetActivated(true, this);
			}
		}
	}
	else if (IsActivated())
	{
		// Hold: 빔이 떠나고 ReleaseDelay가 지나면 꺼짐 (Latch는 켜진 뒤 틱이 꺼져 여기 오지 않음)
		if (ReceiveMode == EGimmickLaserReceiveMode::Hold)
		{
			TimeSinceBeamLost += DeltaTime;
			if (TimeSinceBeamLost >= ReleaseDelay)
			{
				SetActivated(false, this);
			}
		}
	}
	else
	{
		SetCharge(Charge - DrainRate * DeltaTime);
	}

	UpdateTickEnabled();
}

void AGimmickLaserReceiver::HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator)
{
	Super::HandleActivationChanged(bNewActivated, ChangeInstigator);

	TimeSinceBeamLost = 0.f;
	SetCharge(bNewActivated ? 1.f : 0.f);

	if (bNewActivated && ReceiveMode == EGimmickLaserReceiveMode::Latch)
	{
		// 켜진 뒤로는 빔을 보지 않음
		ReceivingLasers.Reset();
	}

	UpdateTickEnabled();
}

void AGimmickLaserReceiver::SetCharge(float NewCharge)
{
	NewCharge = FMath::Clamp(NewCharge, 0.f, 1.f);
	if (FMath::IsNearlyEqual(NewCharge, Charge) && HasActorBegunPlay())
	{
		return;
	}

	Charge = NewCharge;
	if (Mesh && !ChargeParameterName.IsNone())
	{
		Mesh->SetScalarParameterValueOnMaterials(ChargeParameterName, Charge);
	}
}

void AGimmickLaserReceiver::UpdateTickEnabled()
{
	bool bNeedTick = false;
	if (ReceiveMode == EGimmickLaserReceiveMode::Latch && IsActivated())
	{
		bNeedTick = false;
	}
	else
	{
		// 빔을 받는 중(충전·이벤트), 충전이 남아 줄어드는 중, Hold로 켜져 해제 대기 중
		bNeedTick = !ReceivingLasers.IsEmpty() || bWasReceiving || Charge > 0.f || IsActivated();
	}

	if (IsActorTickEnabled() != bNeedTick)
	{
		SetActorTickEnabled(bNeedTick);
	}
}
