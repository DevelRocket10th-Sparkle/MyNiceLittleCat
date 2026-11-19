#include "Frameworks/Sound/AudioVolumeSubsystem.h"
#include "Frameworks/Sound/AudioVolumeSetting.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"

void UAudioVolumeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// PIE / OpenLevel 모두 월드가 준비되면 다시 적용
	ActorsInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this,
		&UAudioVolumeSubsystem::HandleWorldActorsInitialized);
}

void UAudioVolumeSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(ActorsInitializedHandle);
	Super::Deinitialize();
}

void UAudioVolumeSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	if (Params.World && GetGameInstance() && Params.World == GetGameInstance()->GetWorld())
	{
		ApplyAll();
	}
}

void UAudioVolumeSubsystem::SetVolume(EAudioVolumeChannel Channel, float InVolume)
{
	const float Clamped = FMath::Clamp(InVolume, 0.f, 1.f);
	(Channel == EAudioVolumeChannel::BGM ? BGMVolume : SFXVolume) = Clamped;
	Apply(Channel);
	SaveConfig();
}

float UAudioVolumeSubsystem::GetVolume(EAudioVolumeChannel Channel) const
{
	return Channel == EAudioVolumeChannel::BGM ? BGMVolume : SFXVolume;
}

void UAudioVolumeSubsystem::SetMuted(EAudioVolumeChannel Channel, bool bInMuted)
{
	(Channel == EAudioVolumeChannel::BGM ? bBGMMuted : bSFXMuted) = bInMuted;
	Apply(Channel);
	SaveConfig();
}

bool UAudioVolumeSubsystem::IsMuted(EAudioVolumeChannel Channel) const
{
	return Channel == EAudioVolumeChannel::BGM ? bBGMMuted : bSFXMuted;
}

void UAudioVolumeSubsystem::ApplyAll()
{
	Apply(EAudioVolumeChannel::BGM);
	Apply(EAudioVolumeChannel::SFX);
}

void UAudioVolumeSubsystem::Apply(EAudioVolumeChannel Channel)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const UAudioVolumeSetting* Settings = GetDefault<UAudioVolumeSetting>();
	USoundMix* Mix = Settings->VolumeMix.LoadSynchronous();
	USoundClass* Class = (Channel == EAudioVolumeChannel::BGM ? Settings->BGMSoundClass : Settings->SFXSoundClass).LoadSynchronous();
	if (!World || !Mix || !Class)
	{
		return;
	}

	const float Volume = IsMuted(Channel) ? 0.f : GetVolume(Channel);
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, Volume, 1.f, 0.f, true);
	UGameplayStatics::PushSoundMixModifier(World, Mix);
}
