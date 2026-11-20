#include "Frameworks/Feedback/FeedbackCue.h"
#include "Frameworks/Feedback/FeedbackVisual.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

void FFeedbackInstance::Stop(float SoundFadeOutSeconds, bool bDestroyEffect)
{
	// 자동 제거 설정으로 생성했으므로 비활성화만 하면 입자가 사라진 뒤 정리됨 (바로 지워야 하면 제거)
	if (UNiagaraComponent* EffectComponent = Effect.Get())
	{
		if (bDestroyEffect)
		{
			EffectComponent->DestroyComponent();
		}
		else
		{
			EffectComponent->Deactivate();
		}
	}

	if (UAudioComponent* SoundComponent = Sound.Get())
	{
		SoundComponent->FadeOut(FMath::Max(0.f, SoundFadeOutSeconds), 0.f);
	}

	if (AActor* Actor = Visual.Get())
	{
		Actor->Destroy();
	}
	Visual.Reset();
	Effect.Reset();
	Sound.Reset();
}

void FeedbackCue::PlayAtLocation(const UObject* WorldContext, const FFeedbackCue& Cue,
	const FVector& Location, const FRotator& Rotation)
{
	if (!WorldContext)
		return;

	if (Cue.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Cue.Sound, Location, Rotation, 1.f, 1.f, 0.f, Cue.Attenuation);
	}

	// 절차적 연출을 지정했으면 그것만, 아니면 나이아가라 이펙트
	const FTransform Transform(Rotation, Location, FVector(Cue.EffectScale));
	if (!AFeedbackVisual::Play(WorldContext, Cue.ProceduralVisual, Transform) && Cue.Effect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, Cue.Effect, Location, Rotation, FVector(Cue.EffectScale));
	}
}

FFeedbackInstance FeedbackCue::PlayAttached(const FFeedbackCue& Cue, USceneComponent* AttachTo,
	FName Socket, const FVector& Offset)
{
	FFeedbackInstance Instance;
	if (!IsValid(AttachTo))
		return Instance;

	if (Cue.Sound)
	{
		Instance.Sound = UGameplayStatics::SpawnSoundAttached(Cue.Sound, AttachTo, Socket, FVector::ZeroVector,
			EAttachLocation::SnapToTarget, true, 1.f, 1.f, 0.f, Cue.Attenuation);
	}

	// 절차적 연출을 지정했으면 그것만 붙여 재생, 아니면 나이아가라 이펙트
	const FTransform Base = AttachTo->GetSocketTransform(Socket);
	const FTransform Transform(Base.GetRotation(), Base.TransformPosition(Offset), FVector(Cue.EffectScale));
	Instance.Visual = AFeedbackVisual::Play(AttachTo, Cue.ProceduralVisual, Transform, AttachTo, Socket);
	if (!Instance.Visual.IsValid() && Cue.Effect)
	{
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(Cue.Effect, AttachTo, Socket,
			Offset, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
		if (Effect)
		{
			Effect->SetRelativeScale3D(FVector(Cue.EffectScale));
		}
		Instance.Effect = Effect;
	}

	return Instance;
}
