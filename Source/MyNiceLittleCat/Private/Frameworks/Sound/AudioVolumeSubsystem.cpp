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
	SaveIfDirty();
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
	VolumeOf(Channel) = FMath::Clamp(InVolume, 0.f, 1.f);
	bDirty = true;
	Apply(Channel);
}

float UAudioVolumeSubsystem::GetVolume(EAudioVolumeChannel Channel) const
{
	return VolumeOf(Channel);
}

void UAudioVolumeSubsystem::SetMuted(EAudioVolumeChannel Channel, bool bInMuted)
{
	MutedOf(Channel) = bInMuted;
	bDirty = true;
	Apply(Channel);
}

bool UAudioVolumeSubsystem::IsMuted(EAudioVolumeChannel Channel) const
{
	return MutedOf(Channel);
}

void UAudioVolumeSubsystem::SaveIfDirty()
{
	if (bDirty)
	{
		bDirty = false;
		SaveConfig();
	}
}

float& UAudioVolumeSubsystem::VolumeOf(EAudioVolumeChannel Channel)
{
	return Channel == EAudioVolumeChannel::BGM ? BGMVolume : SFXVolume;
}

bool& UAudioVolumeSubsystem::MutedOf(EAudioVolumeChannel Channel)
{
	return Channel == EAudioVolumeChannel::BGM ? bBGMMuted : bSFXMuted;
}

void UAudioVolumeSubsystem::ApplyAll()
{
	for (const EAudioVolumeChannel Channel : TEnumRange<EAudioVolumeChannel>())
	{
		Apply(Channel);
	}
}

void UAudioVolumeSubsystem::Apply(EAudioVolumeChannel Channel)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const UAudioVolumeSetting* Settings = GetDefault<UAudioVolumeSetting>();
	USoundMix* Mix = Settings->VolumeMix.LoadSynchronous();
	USoundClass* Class = Settings->GetSoundClass(Channel).LoadSynchronous();
	if (!World || !Mix || !Class)
	{
		return;
	}

	const float Volume = IsMuted(Channel) ? 0.f : GetVolume(Channel);
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, Volume, 1.f, 0.f, true);

	// 클래스 값 변경은 이미 Push된 믹스에도 바로 반영되므로 월드마다 한 번만 Push
	if (MixPushedWorld.Get() != World)
	{
		UGameplayStatics::PushSoundMixModifier(World, Mix);
		MixPushedWorld = World;
	}
}
