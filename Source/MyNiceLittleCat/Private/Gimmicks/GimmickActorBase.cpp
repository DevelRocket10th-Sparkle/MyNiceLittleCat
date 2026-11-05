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
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/PrimitiveComponent.h"

const FName AGimmickActorBase::SignalLinkPointTag(TEXT("SignalLinkPoint"));

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
	QueueSignal(Activator, true);
}

void AGimmickActorBase::Deactivate_Implementation(AActor* Activator)
{
	QueueSignal(Activator, false);
}

void AGimmickActorBase::QueueSignal(AActor* Source, bool bSignalOn)
{
	// 연결 이펙트는 게임 중 받은 신호에만 (시작 상태·에디터는 바로 적용)
	UWorld* World = GetWorld();
	const bool bUseLinkEffect = SignalLinkEffect && bReceiveSignals && !bApplyingInitialState && World && World->IsGameWorld();
	if (!bUseLinkEffect)
	{
		ApplySignal(Source, bSignalOn);
		return;
	}

	if ((bSignalOn || bSignalLinkOnDeactivate) && IsValid(Source) && Source != this)
	{
		PlaySignalLinkEffect(Source);
	}

	// 입자가 도착하는 시점에 적용. 켜짐·꺼짐 모두 같은 시간만큼 늦춰 순서 유지
	if (SignalLinkTravelTime <= 0.f)
	{
		ApplySignal(Source, bSignalOn);
		return;
	}

	const TWeakObjectPtr<AActor> WeakSource = Source;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakSource, bSignalOn]()
	{
		ApplySignal(WeakSource, bSignalOn);
	}), SignalLinkTravelTime, false);
}

void AGimmickActorBase::ApplySignal(TWeakObjectPtr<AActor> Source, bool bSignalOn)
{
	if (bSignalOn)
	{
		ActiveSignalSources.Add(Source);
	}
	else
	{
		ActiveSignalSources.Remove(Source);
	}
	HandleSignalChanged(Source.Get(), bSignalOn);
}

void AGimmickActorBase::PlaySignalLinkEffect(AActor* Source)
{
	const FVector Start = GetSignalLinkPoint(Source);
	const FVector End = GetSignalLinkPoint(this) + SignalLinkEndOffset;

	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SignalLinkEffect, Start,
		FRotator::ZeroRotator, FVector::OneVector, /*bAutoDestroy*/ true, /*bAutoActivate*/ true, ENCPoolMethod::None, /*bPreCullCheck*/ false);
	if (!Effect)
	{
		return;
	}

	Effect->SetVariableVec3(SignalLinkEndParam, End);
	Effect->SetVariableLinearColor(SignalLinkColorParam, SignalLinkColor);
	Effect->SetVariableFloat(SignalLinkArcHeightParam, SignalLinkArcHeight);

	// 잠깐만 입자를 내보낸 뒤 끔 (남은 입자는 도착할 때까지 날아가고, 다 사라지면 자동 삭제)
	const TWeakObjectPtr<UNiagaraComponent> WeakEffect = Effect;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakEffect]()
	{
		if (WeakEffect.IsValid())
		{
			WeakEffect->Deactivate();
		}
	}), FMath::Max(SignalLinkSpawnDuration, 0.05f), false);
}

FVector AGimmickActorBase::GetSignalLinkPoint(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return FVector::ZeroVector;
	}

	TInlineComponentArray<USceneComponent*> SceneComponents(Actor);
	for (const USceneComponent* Component : SceneComponents)
	{
		if (Component && Component->ComponentHasTag(SignalLinkPointTag))
		{
			return Component->GetComponentLocation();
		}
	}

	// 게임에서 보이는 메시들의 바운드 윗면 가운데 (숨긴 판정용 메시·에디터 전용 컴포넌트 제외)
	FBox Box(ForceInit);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive && Primitive->IsRegistered() && !Primitive->IsEditorOnly() && Primitive->ShouldRender())
		{
			Box += Primitive->Bounds.GetBox();
		}
	}

	if (!Box.IsValid)
	{
		return Actor->GetActorLocation();
	}

	const FVector Center = Box.GetCenter();
	return FVector(Center.X, Center.Y, Box.Max.Z);
}

void AGimmickActorBase::HandleSignalChanged(AActor* Source, bool bSignalOn)
{
	if (!bReceiveSignals)
	{
		return;
	}

	const bool bShouldActivate = EvaluateSignals();

	// 한 번 켜진 뒤로는 신호가 꺼져도 유지
	if (bLatchWhenSignaled && IsActivated() && !bShouldActivate)
	{
		return;
	}

	SetActivated(bShouldActivate, Source);
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
