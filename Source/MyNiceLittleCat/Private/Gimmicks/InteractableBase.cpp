/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 상호작용 기믹 베이스 액터 구현

#include "Gimmicks/InteractableBase.h"
#include "Components/StaticMeshComponent.h"
#include "Interfaces/ActivatableInterface.h"

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
