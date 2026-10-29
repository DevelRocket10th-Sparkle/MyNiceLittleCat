/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 보물 상자 구현

#include "Gimmicks/Stage/GimmickChest.h"
#include "Gimmicks/GimmickMoverComponent.h"
#include "Items/InventorySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"

AGimmickChest::AGimmickChest()
{
	// 한 번 열면 끝, 연 상태 저장, 신호로는 열리지 않음
	bShowToggleableOption = false;
	bToggleable = false;
	bSaveState = true;
	bReceiveSignals = false;

	// 몸통(100cm 큐브 기준) 뒤쪽 윗모서리 = 경첩, 뚜껑이 위로 젖혀짐
	LidMover = CreateDefaultSubobject<UGimmickMoverComponent>(TEXT("LidMover"));
	LidMover->SetupAttachment(Mesh);
	LidMover->SetRelativeLocation(FVector(-50.f, 0.f, 50.f));
	LidMover->OpenRotation = FRotator(100.f, 0.f, 0.f);
	LidMover->OpenDuration = 0.6f;

	LidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMesh"));
	LidMesh->SetupAttachment(LidMover);
	LidMesh->SetMobility(EComponentMobility::Movable);
	LidMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void AGimmickChest::HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator)
{
	Super::HandleActivationChanged(bNewActivated, ChangeInstigator);

	// 저장된 열린 상태로 시작할 때는 아이템을 다시 주지 않음
	if (!bNewActivated || IsApplyingInitialState())
	{
		return;
	}

	TArray<FItemStack> Acquired;
	UGameInstance* GameInstance = GetGameInstance();
	if (UInventorySubsystem* Inventory = GameInstance ? GameInstance->GetSubsystem<UInventorySubsystem>() : nullptr)
	{
		Acquired = Inventory->AddItems(this, Contents, bShowAcquireUI);
	}

	UE_LOG(LogTemp, Log, TEXT("[GimmickChest] %s opened by %s, acquired %d kind(s)"), *GetName(), *GetNameSafe(ChangeInstigator), Acquired.Num());
	K2_OnChestOpened(ChangeInstigator, Acquired);
}
