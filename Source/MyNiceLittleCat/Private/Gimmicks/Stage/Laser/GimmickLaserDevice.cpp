/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 빛 장치 공통 베이스 구현

#include "Gimmicks/Stage/Laser/GimmickLaserDevice.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"

AGimmickLaserDevice::AGimmickLaserDevice()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// 기본 = 뒤(-X)에서 들어온 빛을 받음 (앞으로 지나가는 모양). 자식 클래스가 바꿈
	InputDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("InputDirection"));
	InputDirection->SetupAttachment(Mesh);
	InputDirection->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	InputDirection->SetHiddenInGame(true);
	InputDirection->ArrowColor = FColor(80, 200, 255);

	// 빛으로만 켜짐, 빛이 있어야 유지되므로 저장하지 않음
	bReceiveSignals = false;
	bSaveState = false;
}

void AGimmickLaserDevice::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 루트 스케일과 상관없이 바닥(액터 원점)에서 BeamHeight 높이로 (수평 위치·회전은 BP 배치 유지)
	TArray<ULaserBeamComponent*> Outputs;
	GetOutputLasers(Outputs);
	for (ULaserBeamComponent* Output : Outputs)
	{
		if (Output)
		{
			FVector Location = Output->GetComponentLocation();
			Location.Z = GetActorLocation().Z + BeamHeight;
			Output->SetWorldLocation(Location);
		}
	}
}

void AGimmickLaserDevice::BeginPlay()
{
	Super::BeginPlay();
	SetupOutputLasers();
}

void AGimmickLaserDevice::SetupOutputLasers()
{
	TArray<ULaserBeamComponent*> Outputs;
	GetOutputLasers(Outputs);
	for (ULaserBeamComponent* Output : Outputs)
	{
		if (!Output)
		{
			continue;
		}

		// 생성 스크립트가 다시 돌지 않은 배치(스케일만 바뀐 경우 등)도 높이를 맞춤
		FVector Location = Output->GetComponentLocation();
		Location.Z = GetActorLocation().Z + BeamHeight;
		Output->SetWorldLocation(Location);

		Output->SetEnableReflection(true);
		Output->SetFollowComponentRotation(true);
		Output->SetUseColorTypeDisplay(true);
		Output->SetHazardDisplay(false);

		// 퍼즐용 빛: Pawn을 통과해 플레이어·펫이 지나가도 끊기지 않음, 피해 없음
		TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = Output->GetTraceObjectTypes();
		ObjectTypes.Remove(UEngineTypes::ConvertToObjectType(ECC_Pawn));
		Output->SetTraceObjectTypes(ObjectTypes);
		Output->SetDamagePerHit(0.f);
	}
}

bool AGimmickLaserDevice::ComputeOutputColor(ELaserBeamColor InputColor, ELaserBeamColor& OutColor) const
{
	OutColor = InputColor;
	return true;
}

void AGimmickLaserDevice::ReceiveLaserHit_Implementation(ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount)
{
	// 같은 레이저라도 거울이 돌거나 색·장치 거친 횟수가 바뀔 수 있으므로 매 틱 다시 판단
	if (AcceptsBeam(Laser, Hit, ReflectionCount))
	{
		ReceivingLasers.Add(Laser);
		SetActorTickEnabled(true);
	}
	else
	{
		ReceivingLasers.Remove(Laser);
	}
}

void AGimmickLaserDevice::EndLaserHit_Implementation(ULaserBeamComponent* Laser)
{
	ReceivingLasers.Remove(Laser);
}

bool AGimmickLaserDevice::AcceptsBeam(const ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount) const
{
	return Laser && AcceptsInput(Laser->GetBeamColorType(), Laser->GetChainDepth(), Hit, ReflectionCount);
}

bool AGimmickLaserDevice::GetPreviewOutputLasers(ELaserBeamColor InputColor, const FHitResult& Hit, int32 ReflectionCount,
	TArray<ULaserBeamComponent*>& OutLasers, ELaserBeamColor& OutColor) const
{
	// 장치 거친 횟수는 프리뷰 쪽(레이저 컴포넌트)이 따로 제한
	if (!AcceptsInput(InputColor, 0, Hit, ReflectionCount) || !ComputeOutputColor(InputColor, OutColor))
	{
		return false;
	}

	GetOutputLasers(OutLasers);
	return !OutLasers.IsEmpty();
}

bool AGimmickLaserDevice::AcceptsInput(ELaserBeamColor InputColor, int32 InputChainDepth, const FHitResult& Hit, int32 ReflectionCount) const
{
	if (ReflectionCount < MinReflections || InputChainDepth >= MaxChainDepth)
	{
		return false;
	}

	ELaserBeamColor UnusedColor;
	if (!ComputeOutputColor(InputColor, UnusedColor))
	{
		return false;
	}

	if (bAcceptAnySide || !InputDirection)
	{
		return true;
	}

	// 들어온 방향의 반대가 받는 면 화살표와 허용 각도 안이어야 함 (수평 기준)
	const FVector Incoming = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal2D();
	const FVector Front = InputDirection->GetForwardVector().GetSafeNormal2D();
	if (Incoming.IsNearlyZero() || Front.IsNearlyZero())
	{
		return false;
	}

	return FVector::DotProduct(-Incoming, Front) >= FMath::Cos(FMath::DegreesToRadians(InputHalfAngle));
}

bool AGimmickLaserDevice::IsReceivingBeam() const
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

void AGimmickLaserDevice::Tick(float DeltaTime)
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

	if (IsReceivingBeam())
	{
		TimeSinceBeamLost = 0.f;
		UpdateOutputs();
	}
	else if (IsActivated())
	{
		TimeSinceBeamLost += DeltaTime;
		if (TimeSinceBeamLost >= ReleaseDelay)
		{
			StopOutputs();
			SetActivated(false, this);
		}
	}

	const bool bReceiving = IsReceivingBeam();
	if (bReceiving != bWasReceiving)
	{
		bWasReceiving = bReceiving;
		if (bReceiving)
		{
			K2_OnBeamReceived(OutputColor);
		}
		else
		{
			K2_OnBeamLost();
		}
	}

	// 받는 빛도 없고 꺼져 있으면 쉼 (다시 빛이 들어오면 ReceiveLaserHit가 켬)
	if (!bReceiving && !IsActivated())
	{
		SetActorTickEnabled(false);
	}
}

void AGimmickLaserDevice::UpdateOutputs()
{
	// 장치를 가장 적게 거친 빛 기준 (고리로 돌아온 빛보다 원래 발사기 쪽 빛이 우선)
	const ULaserBeamComponent* Source = nullptr;
	for (const TWeakObjectPtr<ULaserBeamComponent>& Laser : ReceivingLasers)
	{
		if (Laser.IsValid() && Laser->IsLaserActive() && (!Source || Laser->GetChainDepth() < Source->GetChainDepth()))
		{
			Source = Laser.Get();
		}
	}
	if (!Source)
	{
		return;
	}

	ComputeOutputColor(Source->GetBeamColorType(), OutputColor);
	const int32 OutputDepth = Source->GetChainDepth() + 1;

	TArray<ULaserBeamComponent*> Outputs;
	GetOutputLasers(Outputs);
	for (ULaserBeamComponent* Output : Outputs)
	{
		if (!Output)
		{
			continue;
		}

		Output->SetBeamColorType(OutputColor);
		Output->SetChainDepth(OutputDepth);
		if (!Output->IsLaserActive())
		{
			Output->StartLaserForward();
		}
	}

	SetActivated(true, this);
}

void AGimmickLaserDevice::StopOutputs()
{
	TArray<ULaserBeamComponent*> Outputs;
	GetOutputLasers(Outputs);
	for (ULaserBeamComponent* Output : Outputs)
	{
		if (Output)
		{
			Output->StopLaser();
		}
	}
}
