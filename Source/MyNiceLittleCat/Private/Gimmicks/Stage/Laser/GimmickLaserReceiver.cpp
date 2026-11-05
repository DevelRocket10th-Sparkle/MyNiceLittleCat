/// 작성자: 최요환
/// 작성일자: 2026-11-02
///
/// 레이저 수신기 구현

#include "Gimmicks/Stage/Laser/GimmickLaserReceiver.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/PointLightComponent.h"

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

	// 받는 색 표시: 본체 스케일과 상관없이 크기·위치를 cm로 정함, 빛을 막지 않음
	ColorIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ColorIndicator"));
	ColorIndicator->SetupAttachment(Mesh);
	ColorIndicator->SetUsingAbsoluteScale(true);
	ColorIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ColorIndicator->SetGenerateOverlapEvents(false);
	ColorIndicator->SetCanEverAffectNavigation(false);
	ColorIndicator->SetCastShadow(false);

	// 보이는 등·코어: 본체 스케일과 상관없이 cm로 배치, 빛·이동을 막지 않음
	LampMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LampMesh"));
	LampMesh->SetupAttachment(Mesh);
	LampMesh->SetUsingAbsoluteScale(true);
	LampMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LampMesh->SetGenerateOverlapEvents(false);
	LampMesh->SetCanEverAffectNavigation(false);

	LampLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LampLight"));
	LampLight->SetupAttachment(LampMesh);
	LampLight->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	LampLight->SetIntensity(3000.f);
	LampLight->SetAttenuationRadius(600.f);
	LampLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.4f));
	LampLight->SetVisibility(false);

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(Mesh);
	CoreMesh->SetUsingAbsoluteScale(true);
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreMesh->SetGenerateOverlapEvents(false);
	CoreMesh->SetCanEverAffectNavigation(false);
	CoreMesh->SetCastShadow(false);

	// 빔으로만 켜짐 (다른 기믹 신호는 받지 않음), 한 번 켠 수신기는 세이브에 저장
	bReceiveSignals = false;
	bSaveState = true;
}

void AGimmickLaserReceiver::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyReceiveMode();
	UpdateColorIndicator();
	UpdateVisualParts();
	ApplyLampState(IsActivated());
}

void AGimmickLaserReceiver::BeginPlay()
{
	ApplyReceiveMode();

	Super::BeginPlay();

	UpdateColorIndicator();
	UpdateVisualParts();

	// 세이브로 켜진 채 복원됐으면 충전도 가득
	SetCharge(IsActivated() ? 1.f : 0.f);
	ApplyLampState(IsActivated());
}

void AGimmickLaserReceiver::PlaceByFront(USceneComponent* Part, const FVector& Offset, const FRotator& Rotation) const
{
	if (!Part)
	{
		return;
	}

	FVector Front = ReceiveDirection ? ReceiveDirection->GetForwardVector().GetSafeNormal2D() : GetActorForwardVector().GetSafeNormal2D();
	if (Front.IsNearlyZero())
	{
		Front = FVector::ForwardVector;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Front);
	const FVector Base = Mesh ? Mesh->GetComponentLocation() : GetActorLocation();
	const FVector Location = Base + Front * Offset.X + Right * Offset.Y + FVector::UpVector * Offset.Z;
	Part->SetWorldLocationAndRotation(Location, FQuat(Front.Rotation()) * FQuat(Rotation));
}

void AGimmickLaserReceiver::UpdateVisualParts()
{
	PlaceByFront(LampMesh, LampOffset, LampRotation);
	PlaceByFront(CoreMesh, CoreOffset, CoreRotation);

	if (CoreMesh && !ColorParameterName.IsNone())
	{
		const FLinearColor CoreColor = AcceptedColors.IsEmpty() ? CoreDefaultColor : GetLaserBeamDisplayColor(AcceptedColors[0]);
		CoreMesh->SetVectorParameterValueOnMaterials(ColorParameterName, FVector(CoreColor));
	}
}

void AGimmickLaserReceiver::ApplyLampState(bool bLit)
{
	if (LampMesh)
	{
		if (UStaticMesh* StateMesh = bLit ? LitLampMesh.Get() : UnlitLampMesh.Get())
		{
			LampMesh->SetStaticMesh(StateMesh);
		}
	}
	if (LampLight)
	{
		LampLight->SetVisibility(bLit);
	}
}

void AGimmickLaserReceiver::UpdateColorIndicator()
{
	if (!ColorIndicator)
	{
		return;
	}

	// 색 제한이 없으면 표시할 것이 없음
	const bool bShow = !AcceptedColors.IsEmpty();
	ColorIndicator->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}

	// 정면(수평) 기준으로 위치·회전 (본체 회전·스케일과 상관없이 통 입구에 맞춤)
	FVector Front = ReceiveDirection ? ReceiveDirection->GetForwardVector().GetSafeNormal2D() : GetActorForwardVector().GetSafeNormal2D();
	if (Front.IsNearlyZero())
	{
		Front = FVector::ForwardVector;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Front);
	const FVector Base = Mesh ? Mesh->GetComponentLocation() : GetActorLocation();
	const FVector Location = Base + Front * ColorIndicatorOffset.X + Right * ColorIndicatorOffset.Y + FVector::UpVector * ColorIndicatorOffset.Z;
	const FQuat Rotation = FQuat(Front.Rotation()) * FQuat(ColorIndicatorRotation);
	ColorIndicator->SetWorldLocationAndRotation(Location, Rotation);

	if (!ColorParameterName.IsNone())
	{
		ColorIndicator->SetVectorParameterValueOnMaterials(ColorParameterName, FVector(GetLaserBeamDisplayColor(AcceptedColors[0])));
	}
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
	bool bColorMismatch = false;
	if (AcceptsBeam(Laser, Hit, ReflectionCount, &bColorMismatch))
	{
		ReceivingLasers.Add(Laser);
		RejectedLasers.Remove(Laser);
	}
	else
	{
		ReceivingLasers.Remove(Laser);
		if (bColorMismatch)
		{
			HandleColorRejected(Laser);
		}
		else
		{
			RejectedLasers.Remove(Laser);
		}
	}

	UpdateTickEnabled();
}

void AGimmickLaserReceiver::EndLaserHit_Implementation(ULaserBeamComponent* Laser)
{
	ReceivingLasers.Remove(Laser);
	RejectedLasers.Remove(Laser);
	UpdateTickEnabled();
}

void AGimmickLaserReceiver::HandleColorRejected(ULaserBeamComponent* Laser)
{
	// 같은 빛이 계속 닿는 동안은 한 번만 알림 (떠났다 다시 오면 다시 알림)
	if (!Laser || RejectedLasers.Contains(Laser))
	{
		return;
	}
	RejectedLasers.Add(Laser);

	if (RejectFlashDuration > 0.f)
	{
		RejectFlashRemaining = RejectFlashDuration;
		SetRejectFlash(1.f);
	}

	const ELaserBeamColor BeamColor = Laser->GetBeamColorType();
	OnBeamRejected.Broadcast(this, BeamColor);
	K2_OnBeamRejected(BeamColor);
}

void AGimmickLaserReceiver::SetRejectFlash(float Alpha)
{
	if (RejectFlashParameterName.IsNone())
	{
		return;
	}

	if (Mesh)
	{
		Mesh->SetScalarParameterValueOnMaterials(RejectFlashParameterName, Alpha);
	}
	if (ColorIndicator)
	{
		ColorIndicator->SetScalarParameterValueOnMaterials(RejectFlashParameterName, Alpha);
	}
	if (CoreMesh)
	{
		CoreMesh->SetScalarParameterValueOnMaterials(RejectFlashParameterName, Alpha);
	}
}

void AGimmickLaserReceiver::ResetGimmick_Implementation(AActor* ResetInstigator)
{
	// 빔은 다음 틱 ReceiveLaserHit로 다시 들어옴
	ReceivingLasers.Reset();
	RejectedLasers.Reset();
	bWasReceiving = false;
	SetActivated(false, ResetInstigator);
	SetCharge(0.f);
	UpdateTickEnabled();
}

bool AGimmickLaserReceiver::AcceptsBeam(const ULaserBeamComponent* Laser, const FHitResult& Hit, int32 ReflectionCount, bool* bOutColorMismatch) const
{
	if (bOutColorMismatch)
	{
		*bOutColorMismatch = false;
	}

	if (ReflectionCount < MinReflections)
	{
		return false;
	}

	// 빛 받는 통 등 다른 부품에 맞은 빛은 인정하지 않음 (본체 메시에 닿은 빛만)
	if (Mesh && Hit.GetComponent() != Mesh)
	{
		return false;
	}

	if (!bAcceptAnySide && ReceiveDirection)
	{
		// 들어온 방향의 반대가 정면 화살표와 허용 각도 안이어야 함 (수평 기준)
		const FVector Incoming = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal2D();
		const FVector Front = ReceiveDirection->GetForwardVector().GetSafeNormal2D();
		if (Incoming.IsNearlyZero() || Front.IsNearlyZero())
		{
			return false;
		}

		if (FVector::DotProduct(-Incoming, Front) < FMath::Cos(FMath::DegreesToRadians(ReceiveHalfAngle)))
		{
			// 통 없이 각도로만 막으므로, 본체에 닿았지만 방향이 틀린 빛도 거부 연출로 알려 줌
			if (bOutColorMismatch)
			{
				*bOutColorMismatch = Laser != nullptr;
			}
			return false;
		}
	}

	// 색은 마지막에 확인 (다른 조건은 맞고 색만 틀린 빛을 거부 연출로 알려 주기 위해)
	if (!AcceptedColors.IsEmpty() && (!Laser || !AcceptedColors.Contains(Laser->GetBeamColorType())))
	{
		if (bOutColorMismatch)
		{
			*bOutColorMismatch = Laser != nullptr;
		}
		return false;
	}

	return true;
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

	// 거부 깜빡임 (1→0)
	if (RejectFlashRemaining > 0.f)
	{
		RejectFlashRemaining = FMath::Max(0.f, RejectFlashRemaining - DeltaTime);
		SetRejectFlash(RejectFlashDuration > 0.f ? RejectFlashRemaining / RejectFlashDuration : 0.f);
	}

	// 끝 통지 없이 사라진 레이저 정리 (발사기 파괴 등)
	for (auto It = ReceivingLasers.CreateIterator(); It; ++It)
	{
		if (!It->IsValid() || !(*It)->IsLaserActive())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = RejectedLasers.CreateIterator(); It; ++It)
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
	ApplyLampState(bNewActivated);

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
	if (!ChargeParameterName.IsNone())
	{
		if (Mesh)
		{
			Mesh->SetScalarParameterValueOnMaterials(ChargeParameterName, Charge);
		}
		if (CoreMesh)
		{
			CoreMesh->SetScalarParameterValueOnMaterials(ChargeParameterName, Charge);
		}
	}
}

void AGimmickLaserReceiver::UpdateTickEnabled()
{
	bool bNeedTick = false;
	if (RejectFlashRemaining > 0.f)
	{
		// 거부 깜빡임이 끝날 때까지
		bNeedTick = true;
	}
	else if (ReceiveMode == EGimmickLaserReceiveMode::Latch && IsActivated())
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
