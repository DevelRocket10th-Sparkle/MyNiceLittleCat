/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 기믹 공통 베이스 액터 구현

#include "Gimmicks/GimmickActorBase.h"
#include "Gimmicks/GimmickMoverComponent.h"
#include "Gimmicks/GimmickLinkVisualizerComponent.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AGimmickActorBase::AGimmickActorBase()
{
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	LinkVisualizer = CreateEditorOnlyDefaultSubobject<UGimmickLinkVisualizerComponent>(TEXT("LinkVisualizer"));
#endif
}

void AGimmickActorBase::BeginPlay()
{
	Super::BeginPlay();

	RefreshSignalInfo();

	bool bRestored = false;
	UGameInstance* GameInstance = GetGameInstance();
	UGameProgressSubsystem* Progress = GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr;
	if (bSaveState && Progress)
	{
		bool bSavedActivated = false;
		if (Progress->GetGimmickState(this, GetSaveId(), bSavedActivated))
		{
			SetActivated(bSavedActivated, this);
			bRestored = true;
		}
		CollectSaveHandle = Progress->OnCollectSaveData.AddUObject(this, &AGimmickActorBase::WriteSaveState);
	}

	if (!bRestored)
	{
		if (bReceiveSignals && bInvertSignal && CountLinkedSenders() > 0)
		{
			// 반전 신호는 신호가 없을 때 켜져 있어야 함
			SetActivated(EvaluateSignals(), this);
		}
		else if (bStartActivated)
		{
			SetActivated(true, this);
		}
	}

	// 같은 프레임에 BeginPlay하는 다른 기믹의 시작 신호까지 시작 상태로 처리
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bApplyingInitialState = false;
	}));
}

void AGimmickActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CollectSaveHandle.IsValid())
	{
		UGameInstance* GameInstance = GetGameInstance();
		if (UGameProgressSubsystem* Progress = GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr)
		{
			Progress->OnCollectSaveData.Remove(CollectSaveHandle);
		}
		CollectSaveHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

FName AGimmickActorBase::GetSaveId() const
{
	return SaveId.IsNone() ? GetFName() : SaveId;
}

void AGimmickActorBase::WriteSaveState(UGameProgressSaveGame* SaveGame)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (UGameProgressSubsystem* Progress = GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr)
	{
		Progress->SetGimmickState(this, GetSaveId(), bActivated);
	}
}

void AGimmickActorBase::SetActivated(bool bNewActivated, AActor* ChangeInstigator)
{
	if (bActivated == bNewActivated)
	{
		return;
	}

	bActivated = bNewActivated;

	UpdateMovers(bApplyingInitialState);
	HandleActivationChanged(bActivated, ChangeInstigator);
	K2_OnActivationChanged(bActivated, ChangeInstigator);
	OnActivationChanged.Broadcast(this, bActivated, ChangeInstigator);

	NotifyLinkedActors();
}

void AGimmickActorBase::UpdateMovers(bool bInstant)
{
	TInlineComponentArray<UGimmickMoverComponent*> Movers(this);
	for (UGimmickMoverComponent* Mover : Movers)
	{
		if (Mover->bFollowActivation)
		{
			Mover->SetOpen(bActivated, bInstant);
		}
	}
}

void AGimmickActorBase::NotifyLinkedActors()
{
	for (AActor* Linked : LinkedActors)
	{
		if (!IsValid(Linked) || Linked == this || !Linked->Implements<UActivatableInterface>())
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

void AGimmickActorBase::Activate_Implementation(AActor* Activator)
{
	ActiveSignalSources.Add(Activator);
	HandleSignalChanged(Activator, true);
}

void AGimmickActorBase::Deactivate_Implementation(AActor* Activator)
{
	ActiveSignalSources.Remove(Activator);
	HandleSignalChanged(Activator, false);
}

void AGimmickActorBase::HandleSignalChanged(AActor* Source, bool bSignalOn)
{
	if (!bReceiveSignals)
	{
		return;
	}

	SetActivated(EvaluateSignals(), Source);
}

bool AGimmickActorBase::EvaluateSignals() const
{
	int32 ActiveCount = 0;
	for (const TWeakObjectPtr<AActor>& Source : ActiveSignalSources)
	{
		// 신호를 보낸 뒤 사라진 액터는 제외 (BP에서 Activator 없이 호출한 신호는 유지)
		if (Source.IsValid() || Source.IsExplicitlyNull())
		{
			++ActiveCount;
		}
	}

	bool bConditionMet = false;
	if (SignalRule == EGimmickSignalRule::Any)
	{
		bConditionMet = ActiveCount > 0;
	}
	else
	{
		const int32 Required = GetRequiredSignalCount();
		bConditionMet = Required > 0 && ActiveCount >= Required;
	}

	return bInvertSignal ? !bConditionMet : bConditionMet;
}

int32 AGimmickActorBase::GetRequiredSignalCount() const
{
	if (SignalRule == EGimmickSignalRule::Any)
	{
		return 1;
	}
	return RequiredSignalCount > 0 ? RequiredSignalCount : CountLinkedSenders();
}

void AGimmickActorBase::RefreshSignalInfo()
{
	LinkedSenderCount = CountLinkedSenders();
	// 신호를 보내는 기믹이 없으면 0 (신호로 켜질 일 없음)
	EffectiveRequiredSignals = (!bReceiveSignals || LinkedSenderCount == 0) ? 0
		: (SignalRule == EGimmickSignalRule::Any ? 1 : (RequiredSignalCount > 0 ? RequiredSignalCount : LinkedSenderCount));
}

void AGimmickActorBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshSignalInfo();
}

int32 AGimmickActorBase::CountLinkedSenders() const
{
	TArray<AGimmickActorBase*> Senders;
	GetLinkedSenders(Senders);
	return Senders.Num();
}

void AGimmickActorBase::GetLinkedSenders(TArray<AGimmickActorBase*>& OutSenders) const
{
	OutSenders.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AGimmickActorBase> It(World); It; ++It)
	{
		if (*It != this && It->LinkedActors.Contains(this))
		{
			OutSenders.Add(*It);
		}
	}
}
