/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 인벤토리 서브시스템 구현

#include "Items/InventorySubsystem.h"
#include "Items/ItemData.h"
#include "Items/ItemSettings.h"
#include "WIdgets/Item/ItemAcquireWidget.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Frameworks/Save/GameProgressSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Sound/SoundBase.h"

void UInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 세이브 로드가 먼저 끝나야 함
	if (UGameProgressSubsystem* Progress = Collection.InitializeDependency<UGameProgressSubsystem>())
	{
		CollectSaveHandle = Progress->OnCollectSaveData.AddUObject(this, &UInventorySubsystem::WriteToSave);
		SaveReplacedHandle = Progress->OnSaveDataReplaced.AddUObject(this, &UInventorySubsystem::LoadFromSave);
	}

	LoadFromSave();
}

void UInventorySubsystem::Deinitialize()
{
	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		Progress->OnCollectSaveData.Remove(CollectSaveHandle);
		Progress->OnSaveDataReplaced.Remove(SaveReplacedHandle);
	}

	Super::Deinitialize();
}

void UInventorySubsystem::LoadFromSave()
{
	Items.Reset();

	const UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	const UGameProgressSaveGame* Save = Progress ? Progress->GetCurrentSave() : nullptr;
	if (!Save)
	{
		return;
	}

	for (const TPair<FSoftObjectPath, int32>& Pair : Save->InventoryItems)
	{
		UItemData* Item = Cast<UItemData>(Pair.Key.TryLoad());
		if (!Item)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Inventory] 저장된 아이템을 찾을 수 없음: %s"), *Pair.Key.ToString());
			continue;
		}
		if (Pair.Value > 0)
		{
			Items.Add(Item, Pair.Value);
		}
	}
}

void UInventorySubsystem::WriteToSave(UGameProgressSaveGame* SaveGame)
{
	if (!SaveGame)
	{
		return;
	}

	SaveGame->InventoryItems.Reset();
	for (const TPair<TObjectPtr<UItemData>, int32>& Pair : Items)
	{
		if (Pair.Key && Pair.Value > 0)
		{
			SaveGame->InventoryItems.Add(FSoftObjectPath(Pair.Key.Get()), Pair.Value);
		}
	}
}

void UInventorySubsystem::SetCount(UItemData* Item, int32 NewCount)
{
	const int32 OldCount = GetItemCount(Item);
	NewCount = FMath::Max(NewCount, 0);
	if (NewCount == 0)
	{
		Items.Remove(Item);
	}
	else
	{
		Items.Add(Item, NewCount);
	}

	if (NewCount != OldCount)
	{
		OnItemChanged.Broadcast(Item, NewCount, NewCount - OldCount);
	}
}

int32 UInventorySubsystem::AddItem(UItemData* Item, int32 Count)
{
	if (!Item || Count <= 0)
	{
		return 0;
	}

	const int32 OldCount = GetItemCount(Item);
	const int32 NewCount = Item->MaxCount > 0 ? FMath::Min(OldCount + Count, Item->MaxCount) : OldCount + Count;
	SetCount(Item, NewCount);

	UE_LOG(LogTemp, Log, TEXT("[Inventory] +%d %s (%d -> %d)"), NewCount - OldCount, *Item->GetName(), OldCount, NewCount);

	// 능력 해금 아이템을 처음 얻음 (세이브 로드는 AddItem을 거치지 않아 알리지 않음)
	if (OldCount <= 0 && NewCount > 0 && Item->ItemType == EItemType::Ability && Item->AbilityTag.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[Inventory] 능력 해금: %s"), *Item->AbilityTag.ToString());
		OnAbilityUnlocked.Broadcast(Item->AbilityTag, Item);
	}

	return NewCount - OldCount;
}

void UInventorySubsystem::NotifyAcquireFinished(const TArray<FItemStack>& AcquiredItems)
{
	for (const FItemStack& Stack : AcquiredItems)
	{
		const UItemData* Item = Stack.Item;
		if (Item && Stack.Count > 0 && Item->ItemType == EItemType::Ability && Item->AbilityTag.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[Inventory] 능력 획득 연출 끝: %s"), *Item->AbilityTag.ToString());
			OnAbilityAcquireFinished.Broadcast(Item->AbilityTag, Stack.Item);
		}
	}
}

bool UInventorySubsystem::HasAbility(FGameplayTag AbilityTag) const
{
	if (GetDefault<UItemSettings>()->bTestUnlockAllAbilities)
	{
		return true;
	}

	if (!AbilityTag.IsValid())
	{
		return false;
	}

	for (const TPair<TObjectPtr<UItemData>, int32>& Pair : Items)
	{
		const UItemData* Item = Pair.Key;
		if (Item && Pair.Value > 0 && Item->ItemType == EItemType::Ability && Item->AbilityTag.MatchesTagExact(AbilityTag))
		{
			return true;
		}
	}
	return false;
}

TArray<FItemStack> UInventorySubsystem::AddItems(const UObject* WorldContextObject, const TArray<FItemStack>& InItems, bool bShowAcquireUI)
{
	TArray<FItemStack> Acquired;
	for (const FItemStack& Stack : InItems)
	{
		const int32 Added = AddItem(Stack.Item, Stack.Count);
		if (Added > 0)
		{
			Acquired.Emplace(Stack.Item, Added);
		}
	}

	if (Acquired.Num() > 0)
	{
		OnItemsAcquired.Broadcast(Acquired);
		if (bShowAcquireUI)
		{
			ShowAcquireUI(WorldContextObject, Acquired);
		}
	}
	return Acquired;
}

bool UInventorySubsystem::RemoveItem(UItemData* Item, int32 Count)
{
	if (!Item || Count <= 0 || GetItemCount(Item) < Count)
	{
		return false;
	}

	SetCount(Item, GetItemCount(Item) - Count);
	return true;
}

int32 UInventorySubsystem::GetItemCount(const UItemData* Item) const
{
	const int32* Found = Item ? Items.Find(Item) : nullptr;
	return Found ? *Found : 0;
}

TArray<FItemStack> UInventorySubsystem::GetAllItems() const
{
	TArray<FItemStack> Result;
	for (const TPair<TObjectPtr<UItemData>, int32>& Pair : Items)
	{
		if (Pair.Key && Pair.Value > 0)
		{
			Result.Emplace(Pair.Key.Get(), Pair.Value);
		}
	}
	return Result;
}

UItemAcquireWidget* UInventorySubsystem::ShowAcquireUI(const UObject* WorldContextObject, const TArray<FItemStack>& InItems, bool bWaitForClose)
{
	TArray<FItemStack> Visible;
	for (const FItemStack& Stack : InItems)
	{
		if (Stack.Item && Stack.Item->bShowInAcquireUI)
		{
			Visible.Add(Stack);
		}
	}
	if (Visible.Num() == 0)
	{
		return nullptr;
	}

	const UItemSettings* Settings = GetDefault<UItemSettings>();
	UClass* WidgetClass = Settings->AcquireWidgetClass.LoadSynchronous();
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (!WidgetClass || !PlayerController)
	{
		UE_LOG(LogTemp, Log, TEXT("[Inventory] 획득 UI 없음 (프로젝트 설정 > Item > Acquire Widget Class 확인)"));
		return nullptr;
	}

	// 위젯 하나를 재사용 (맵이 바뀌거나 플레이어 컨트롤러가 바뀌면 다시 만듦)
	if (!IsValid(AcquireWidget) || AcquireWidget->GetOwningPlayer() != PlayerController)
	{
		AcquireWidget = CreateWidget<UItemAcquireWidget>(PlayerController, WidgetClass);
	}
	if (!AcquireWidget)
	{
		return nullptr;
	}

	if (!AcquireWidget->IsInViewport())
	{
		AcquireWidget->AddToViewport(Settings->AcquireWidgetZOrder);
	}
	// 닫기를 기다리면 자동으로 숨기지 않음
	AcquireWidget->ShowItems(Visible, bWaitForClose ? 0.f : Settings->AcquireDisplayTime, bWaitForClose);
	PlayAcquireSound(WorldContextObject, Visible);
	return AcquireWidget;
}

void UInventorySubsystem::PlayAcquireSound(const UObject* WorldContextObject, const TArray<FItemStack>& InItems) const
{
	// 능력 아이템 소리 우선, 그다음 목록 순서
	USoundBase* Sound = nullptr;
	for (int32 Pass = 0; Pass < 2 && !Sound; ++Pass)
	{
		for (const FItemStack& Stack : InItems)
		{
			const bool bAbility = Stack.Item && Stack.Item->ItemType == EItemType::Ability;
			if (Stack.Item && (Pass == 1 || bAbility) && !Stack.Item->AcquireSound.IsNull())
			{
				Sound = Stack.Item->AcquireSound.LoadSynchronous();
				if (Sound)
				{
					break;
				}
			}
		}
	}

	const UItemSettings* Settings = GetDefault<UItemSettings>();
	if (!Sound)
	{
		Sound = Cast<USoundBase>(Settings->DefaultAcquireSound.TryLoad());
	}

	// 상자는 열면 게임이 일시정지되므로 UI 소리(2D)로 재생
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(WorldContextObject, Sound, Settings->AcquireSoundVolume);
	}
}
