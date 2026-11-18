#include "Frameworks/Sound/BGMSubSystem.h"
#include "Frameworks/Sound/BGMSetting.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

void UBGMSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 모든 월드의 액터 초기화 시점에 호출 -> 현재 게임 인스턴스의 월드만 골라서..
	ActorsInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this,
		&UBGMSubSystem::HandleWorldActorsInitialized);
}

void UBGMSubSystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(ActorsInitializedHandle);

	// 게임 종료하니까 남은 BGM 정리해요
	if (nullptr != CurrentComp)
	{
		CurrentComp->Stop();
		CurrentComp = nullptr;
	}
	Super::Deinitialize();
}

void UBGMSubSystem::EnterZone(UObject* Zone, USoundBase* Sound, int32 Priority, float FadeTime)
{
	if (nullptr == Zone) return;

	// 중복 진입 방지 (같은 Zone이 두 번 들어오면 기존 것 제거 후 새로 추가)
	ActiveZones.RemoveAll([Zone](const FActiveZone& Z) { return Z.Zone.Get() == Zone; });

	FActiveZone NewZone;
	NewZone.Zone = Zone;
	NewZone.Sound = Sound;
	NewZone.Priority = Priority;
	NewZone.EnterOrder = ++ZoneEnterCounter;
	ActiveZones.Add(NewZone);

	RefreshPlayback(GetFadeTime(FadeTime));
}

void UBGMSubSystem::ExitZone(UObject* Zone, float FadeTime)
{
	const int32 Removed = ActiveZones.RemoveAll([Zone](const FActiveZone& Z)
		{
			return Z.Zone.Get() == Zone || !Z.Zone.IsValid(); // 겸사겸사 죽은 항목도 정리
		});

	if (Removed > 0)
		RefreshPlayback(GetFadeTime(FadeTime));
}

void UBGMSubSystem::PushEventBGM(UObject* Owner, USoundBase* Sound, float FadeTime)
{
	if (nullptr == Owner || nullptr == Sound) return;

	// 같은 Owner가 이미 있으면 제거 후 맨 위로 (곡 교체 = 페이즈 전환 용도)
	EventStack.RemoveAll([Owner](const FEventEntry& E) { return E.Owner.Get() == Owner; });

	FEventEntry Entry;
	Entry.Owner = Owner;
	Entry.Sound = Sound;
	EventStack.Add(Entry);

	RefreshPlayback(GetFadeTime(FadeTime));
}

void UBGMSubSystem::PopEventBGM(UObject* Owner, float FadeTime)
{
	const int32 Removed = EventStack.RemoveAll([Owner](const FEventEntry& E)
		{
			return E.Owner.Get() == Owner || !E.Owner.IsValid();
		});

	// 스택이 비면 ResolveDesiredSound가 Zone/레벨 기본곡으로 자동 복귀
	if (Removed > 0)
		RefreshPlayback(GetFadeTime(FadeTime));
}

void UBGMSubSystem::SetMuted(bool bMute, float FadeTime)
{
	if (bMute == bMuted) return;

	bMuted = bMute;
	RefreshPlayback(GetFadeTime(FadeTime));
}

void UBGMSubSystem::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	if (nullptr == GetWorld() || !(GetWorld()->IsGameWorld()) || GetGameInstance() != (GetWorld()->GetGameInstance())) return;

	// 새 맵 = 이전 맵의 Zone/이벤트는 의미 없음 -> 초기화 
	// (보스 맵에서 나가면 보스 곡이 남지 않도록)
	ActiveZones.Reset();
	EventStack.Reset();
	LevelDefaultSound = FindLevelDefaultSound(GetWorld());

	// 같은 곡이면 RefreshPlayback이 무시 -> 레벨이 바뀌어도 곡이 이어짐
	// 스폰 위치가 Zone 안이면 이후 BeginOverlap이 와서 Zone 곡으로 덮어씀
	RefreshPlayback(GetFadeTime(-1.f));
}

USoundBase* UBGMSubSystem::FindLevelDefaultSound(const UWorld* World) const
{
	const UBGMSetting* Setting = GetDefault<UBGMSetting>();
	if (nullptr == Setting || nullptr == GetWorld()) return nullptr;

	// PIE에선 패키지 이름에 "UEDPIE_0_" 접두사가 붙으므로 제거 후 비교
	const FString CurrentPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());

	for (const TPair<TSoftObjectPtr<UWorld>, TSoftObjectPtr<USoundBase>>& Pair : Setting->LevelBGMs)
	{
		// BGM은 용량이 작고 맵 로드 시점이라 동기 로드로 단순하게 처리
		if (Pair.Key.ToSoftObjectPath().GetLongPackageName() == CurrentPackage)
			return Pair.Value.LoadSynchronous();
	}
	return nullptr;
}

USoundBase* UBGMSubSystem::ResolveDesiredSound() const
{
	if (true == bMuted) return nullptr;

	// 3순위: 이벤트 스택 맨 위 (유효한 것 중 가장 마지막)
	for (int32 i = EventStack.Num() - 1; i >= 0; --i)
	{
		if (EventStack[i].Owner.IsValid() && EventStack[i].Sound.IsValid())
			return EventStack[i].Sound.Get();
	}

	// 2순위: Zone 중 Priority 최고 -> 같으면 나중에 들어간 쪽
	const FActiveZone* Best = nullptr;
	for (const FActiveZone& Z : ActiveZones)
	{
		// 스트리밍 언로드 등으로 사라진 Zone은 무시
		if (false == Z.Zone.IsValid() || false == Z.Sound.IsValid())
			continue;

		if (!Best || Z.Priority > Best->Priority ||
			(Z.Priority == Best->Priority && Z.EnterOrder > Best->EnterOrder))
			Best = &Z;
	}

	if (Best)
		return Best->Sound.Get();

	// 1순위: 레벨 기본곡
	return LevelDefaultSound;
}

void UBGMSubSystem::RefreshPlayback(float FadeTime)
{
	USoundBase* Desired = ResolveDesiredSound();

	// 같은 곡이 이미 재생 중이면 아무것도 안 함 (끊김/재시작 방지)
	if (CurrentComp && CurrentComp->Sound == Desired && CurrentComp->IsPlaying())
		return;

	// 이전 곡 페이드아웃 (bAutoDestroy라 끝나면 알아서 파괴됨)
	if (CurrentComp)
	{
		CurrentComp->FadeOut(FadeTime, 0.f);
		CurrentComp = nullptr;
	}

	if (!Desired) return;

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return;

	// bPersistAcrossLevelTransition = true 가 핵심:
	// OpenLevel로 월드가 바뀌어도 이 컴포넌트는 계속 재생되어요..
	CurrentComp = UGameplayStatics::CreateSound2D(
		World, Desired,
		/*Volume*/ 1.f, /*Pitch*/ 1.f, /*StartTime*/ 0.f,
		/*Concurrency*/ nullptr,
		/*bPersistAcrossLevelTransition*/ true,
		/*bAutoDestroy*/ true);

	if (CurrentComp)
		CurrentComp->FadeIn(FadeTime, GetDefault<UBGMSetting>()->BGMVolume);
}

float UBGMSubSystem::GetFadeTime(float Requested) const
{
	//음수면 설정의 기본값 사용
	return Requested >= 0.f ? Requested : GetDefault<UBGMSetting>()->DefaultFadeTime;
}
