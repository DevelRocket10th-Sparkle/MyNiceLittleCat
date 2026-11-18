// 플레이어 사망 효과음 구현

#include "Characters/PlayerComponents/DeathSoundComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UDeathSoundComponent::UDeathSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDeathSoundComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
	{
		Player->OnPlayerDied.AddUniqueDynamic(this, &UDeathSoundComponent::HandlePlayerDied);
	}
}

void UDeathSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ImpactTimer);
	}
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
	{
		Player->OnPlayerDied.RemoveDynamic(this, &UDeathSoundComponent::HandlePlayerDied);
	}
	Super::EndPlay(EndPlayReason);
}

void UDeathSoundComponent::HandlePlayerDied()
{
	PlayDeathSound();
}

bool UDeathSoundComponent::IsOwnerEasyMode() const
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	return Player && Player->IsEasyMode();
}

void UDeathSoundComponent::PlayDeathSound()
{
	const bool bEasy = IsOwnerEasyMode();
	PlayOne((bEasy && EasyModeVoiceSound) ? EasyModeVoiceSound.Get() : VoiceSound.Get(), VoiceVolume, VoicePitch);

	if ((bEasy && EasyModeImpactSound) || ImpactSound)
	{
		if (ImpactDelay > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(ImpactTimer, this, &UDeathSoundComponent::PlayImpact, ImpactDelay, false);
		}
		else
		{
			PlayImpact();
		}
	}
}

void UDeathSoundComponent::PlayImpact()
{
	USoundBase* Sound = (IsOwnerEasyMode() && EasyModeImpactSound) ? EasyModeImpactSound.Get() : ImpactSound.Get();
	PlayOne(Sound, ImpactVolume, 1.f);
}

void UDeathSoundComponent::PlayOne(USoundBase* Sound, float Volume, float Pitch) const
{
	if (!Sound)
	{
		return;
	}
	if (bPlay2D)
	{
		UGameplayStatics::PlaySound2D(this, Sound, Volume, Pitch);
	}
	else if (const AActor* Owner = GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Owner->GetActorLocation(), Volume, Pitch, 0.f, Attenuation);
	}
}
