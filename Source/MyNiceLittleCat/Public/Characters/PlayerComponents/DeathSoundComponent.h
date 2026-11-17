// 플레이어 사망 효과음 컴포넌트입니다. (동구 요청으로 추가)
// APlayerCharacter::OnPlayerDied를 듣고, 쓰러질 때 비명(VoiceSound) → 잠시 뒤 쿵(ImpactSound)을 재생합니다.
// 플레이어 BP에 컴포넌트로 추가하고 사운드만 지정하면 됩니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeathSoundComponent.generated.h"

class USoundBase;
class USoundAttenuation;

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UDeathSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeathSoundComponent();

	// 직접 재생 (테스트용)
	UFUNCTION(BlueprintCallable, Category = "Death Sound")
	void PlayDeathSound();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandlePlayerDied();

	void PlayImpact();

protected:
	// 쓰러지는 순간 소리 (비명·울음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death Sound")
	TObjectPtr<USoundBase> VoiceSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death Sound", meta = (ClampMin = "0"))
	float VoiceVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death Sound", meta = (ClampMin = "0.1"))
	float VoicePitch = 1.15f;

	// 몸이 바닥에 닿는 소리 (선택)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death Sound")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death Sound", meta = (ClampMin = "0"))
	float ImpactVolume = 0.8f;

	// 사망 시작부터 쿵 소리까지 (사망 애니메이션에서 쓰러지는 타이밍에 맞춤)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death Sound", meta = (ClampMin = "0", Units = "s"))
	float ImpactDelay = 0.6f;

	// true면 화면 소리(2D)로 재생 (카메라 거리와 상관없이 항상 들림)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death Sound")
	bool bPlay2D = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death Sound", meta = (EditCondition = "!bPlay2D"))
	TObjectPtr<USoundAttenuation> Attenuation;

private:
	void PlayOne(USoundBase* Sound, float Volume, float Pitch) const;

	FTimerHandle ImpactTimer;
};
