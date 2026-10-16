/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 상호작용 기믹 베이스 액터 구현

#include "Gimmicks/InteractableBase.h"
#include "Components/StaticMeshComponent.h"
#include "Interfaces/ActivatableInterface.h"
#include "Gimmicks/InteractionPointComponent.h"

AInteractableBase::AInteractableBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

void AInteractableBase::BeginPlay()
{
	Super::BeginPlay();

	if (bStartActivated)
	{
		SetActivated(true, this);
	}
}

void AInteractableBase::Interact_Implementation(AActor* InteractInstigator)
{
	if (!IInteractableInterface::Execute_CanInteract(this, InteractInstigator))
	{
		return;
	}

	SetActivated(!bActivated, InteractInstigator);
}

bool AInteractableBase::CanInteract_Implementation(AActor* InteractInstigator) const
{
	if (!bInteractionEnabled)
	{
		return false;
	}

	// 토글 불가 기믹은 한 번 켜지면 잠김
	return bToggleable || !bActivated;
}

void AInteractableBase::OnFocusBegin_Implementation(AActor* InteractInstigator)
{
	if (bHighlightOnFocus && Mesh)
	{
		Mesh->SetRenderCustomDepth(true);
	}
}

void AInteractableBase::OnFocusEnd_Implementation(AActor* InteractInstigator)
{
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(false);
	}
}

FInteractAlignment AInteractableBase::GetInteractAlignment_Implementation(AActor* InteractInstigator) const
{
	FInteractAlignment Result;
	Result.Mode = AlignMode;

	if (AlignMode == EInteractAlignMode::None || !IsValid(InteractInstigator))
	{
		Result.Mode = EInteractAlignMode::None;
		return Result;
	}

	const FVector InstigatorLocation = InteractInstigator->GetActorLocation();
	const FVector ActorLocation = GetActorLocation();

	if (AlignMode == EInteractAlignMode::SnapToPoint)
	{
		bool bFoundTarget = false;

		// 배치된 상호작용 지점 중 플레이어와 가장 가까운 지점
		TInlineComponentArray<UInteractionPointComponent*> Points(this);
		float BestDistSq = TNumericLimits<float>::Max();
		for (const UInteractionPointComponent* Point : Points)
		{
			const float DistSq = FVector::DistSquared2D(Point->GetComponentLocation(), InstigatorLocation);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Result.TargetLocation = Point->GetComponentLocation();
				Result.TargetRotation = FRotator(0.f, Point->GetComponentRotation().Yaw, 0.f);
				bFoundTarget = true;
			}
		}

		// 지점이 없으면 액터 중심에서 플레이어 쪽으로 AlignRadius만큼 떨어진 위치
		if (!bFoundTarget && AlignRadius > 0.f)
		{
			FVector Dir = (InstigatorLocation - ActorLocation).GetSafeNormal2D();
			if (Dir.IsNearlyZero())
			{
				Dir = GetActorForwardVector().GetSafeNormal2D();
			}
			Result.TargetLocation = ActorLocation + Dir * AlignRadius;
			Result.TargetRotation = FRotator(0.f, (-Dir).Rotation().Yaw, 0.f);
			bFoundTarget = true;
		}

		if (bFoundTarget && FVector::Dist2D(Result.TargetLocation, InstigatorLocation) <= MaxAlignDistance)
		{
			return Result;
		}

		// 목표가 없거나 너무 멀면 바라보기만
		Result.Mode = EInteractAlignMode::FaceOnly;
	}

	// FaceOnly: 제자리에서 액터 중심을 바라봄
	Result.TargetLocation = InstigatorLocation;
	Result.TargetRotation = FRotator(0.f, (ActorLocation - InstigatorLocation).Rotation().Yaw, 0.f);
	return Result;
}

void AInteractableBase::SetActivated(bool bNewActivated, AActor* ChangeInstigator)
{
	if (bActivated == bNewActivated)
	{
		return;
	}

	bActivated = bNewActivated;

	HandleActivationChanged(bActivated, ChangeInstigator);
	K2_OnActivationChanged(bActivated, ChangeInstigator);
	OnActivationChanged.Broadcast(this, bActivated, ChangeInstigator);

	NotifyLinkedActors();
}

void AInteractableBase::NotifyLinkedActors()
{
	for (AActor* Linked : LinkedActors)
	{
		if (!IsValid(Linked) || !Linked->Implements<UActivatableInterface>())
		{
			continue;
		}

		if (bActivated)
		{
			IActivatableInterface::Execute_Activate(Linked, this);
		}
		else
		{
			IActivatableInterface::Execute_Deactivate(Linked, this);
		}
	}
}
