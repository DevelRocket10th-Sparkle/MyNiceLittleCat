// 플레이어 피격 효과음 구현

#include "Characters/PlayerComponents/HitSoundComponent.h"
#include "Characters/GAS/PlayerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UHitSoundComponent::UHitSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHitSoundComponent::BeginPlay()
{
	Super::BeginPlay();

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	if (UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr)
	{
		HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UPlayerAttributeSet::GetHealthAttribute())
			.AddUObject(this, &UHitSoundComponent::HandleHealthChanged);
		BoundASC = ASC;
	}
}

void UHitSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UPlayerAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
	}
	HealthChangedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void UHitSoundComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// 스폰 직후 체력 초기화(기본값 → 최대 체력)로 줄어드는 건 피격이 아님
	constexpr float SpawnIgnoreSeconds = 1.f;
	if (const AActor* Owner = GetOwner(); !Owner || Owner->GetGameTimeSinceCreation() < SpawnIgnoreSeconds)
	{
		return;
	}

	// 줄어들 때만 (회복·초기화 무시)
	if (Data.NewValue >= Data.OldValue)
	{
		return;
	}
	if (bSkipOnLethalHit && Data.NewValue <= 0.f)
	{
		return;
	}
	PlayHitSound();
}

void UHitSoundComponent::PlayHitSound()
{
	const UWorld* World = GetWorld();
	const int32 Count = HitSounds.Num();
	if (!World || Count == 0)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastPlayTime < MinInterval)
	{
		return;
	}
	LastPlayTime = Now;

	int32 Index = FMath::RandRange(0, Count - 1);
	if (Count > 1 && Index == LastIndex)
	{
		Index = (Index + 1 + FMath::RandRange(0, Count - 2)) % Count;
	}
	LastIndex = Index;

	USoundBase* Sound = HitSounds[Index];
	if (!Sound)
	{
		return;
	}

	const float Volume = FMath::FRandRange(VolumeRange.X, VolumeRange.Y);
	const float Pitch = FMath::FRandRange(PitchRange.X, PitchRange.Y);
	if (bPlay2D)
	{
		UGameplayStatics::PlaySound2D(this, Sound, Volume, Pitch);
	}
	else if (const AActor* Owner = GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Owner->GetActorLocation(), Volume, Pitch, 0.f, Attenuation);
	}
}
