#include "Enemy/Combat/EnemyFeedback.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

void FEnemyFeedbackInstance::Stop(float SoundFadeOutSeconds)
{
	// 자동 제거 설정으로 생성했으므로 비활성화만 하면 입자가 사라진 뒤 정리됨
	if (UNiagaraComponent* EffectComponent = Effect.Get())
	{
		EffectComponent->Deactivate();
	}

	if (UAudioComponent* SoundComponent = Sound.Get())
	{
		SoundComponent->FadeOut(FMath::Max(0.f, SoundFadeOutSeconds), 0.f);
	}

	Effect.Reset();
	Sound.Reset();
}

void EnemyFeedback::PlayAtLocation(const UObject* WorldContext, const FEnemyFeedbackCue& Cue,
	const FVector& Location, const FRotator& Rotation)
{
	if (!WorldContext || !Cue.IsSet())
		return;

	if (Cue.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Cue.Sound, Location, Rotation, 1.f, 1.f, 0.f, Cue.Attenuation);
	}

	if (Cue.Effect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, Cue.Effect, Location, Rotation, FVector(Cue.EffectScale));
	}
}

FEnemyFeedbackInstance EnemyFeedback::PlayAttached(const FEnemyFeedbackCue& Cue, USceneComponent* AttachTo, FName Socket)
{
	FEnemyFeedbackInstance Instance;
	if (!IsValid(AttachTo) || !Cue.IsSet())
		return Instance;

	if (Cue.Sound)
	{
		Instance.Sound = UGameplayStatics::SpawnSoundAttached(Cue.Sound, AttachTo, Socket, FVector::ZeroVector,
			EAttachLocation::SnapToTarget, true, 1.f, 1.f, 0.f, Cue.Attenuation);
	}

	if (Cue.Effect)
	{
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(Cue.Effect, AttachTo, Socket,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		if (Effect)
		{
			Effect->SetRelativeScale3D(FVector(Cue.EffectScale));
		}
		Instance.Effect = Effect;
	}

	return Instance;
}
