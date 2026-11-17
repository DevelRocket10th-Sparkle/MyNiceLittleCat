// 플레이어 피격 효과음 컴포넌트입니다. (동구 요청으로 추가)
// 플레이어 체력(PlayerAttributeSet.Health)이 줄어들 때마다 맞는 소리를 냅니다.
// 체력이 0이 되는 마지막 한 방은 사망 소리(DeathSoundComponent)에 맡기고 건너뜁니다 (bSkipOnLethalHit).
// 회복(물약·체크포인트)은 체력이 늘어나므로 소리 없음. 플레이어 코드를 수정하지 않고 동작합니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "HitSoundComponent.generated.h"

class USoundBase;
class USoundAttenuation;

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UHitSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitSoundComponent();

	// 직접 재생 (테스트용)
	UFUNCTION(BlueprintCallable, Category = "Hit Sound")
	void PlayHitSound();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleHealthChanged(const FOnAttributeChangeData& Data);

protected:
	// 맞을 때 소리들 (여러 개면 랜덤, 같은 소리 연속 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Sound")
	TArray<TObjectPtr<USoundBase>> HitSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Sound", meta = (ClampMin = "0"))
	FVector2D VolumeRange = FVector2D(0.9f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Sound", meta = (ClampMin = "0.1"))
	FVector2D PitchRange = FVector2D(1.1f, 1.3f);

	// 이 시간 안에 다시 맞으면 소리 생략 (여러 판정이 한꺼번에 들어올 때)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Sound", meta = (ClampMin = "0", Units = "s"))
	float MinInterval = 0.15f;

	// 치명타(체력 0)는 사망 소리만 나게 건너뜀
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Sound")
	bool bSkipOnLethalHit = true;

	// true면 화면 소리(2D)로 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Sound")
	bool bPlay2D = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Sound", meta = (EditCondition = "!bPlay2D"))
	TObjectPtr<USoundAttenuation> Attenuation;

private:
	FDelegateHandle HealthChangedHandle;
	TWeakObjectPtr<class UAbilitySystemComponent> BoundASC;
	float LastPlayTime = -1000.f;
	int32 LastIndex = INDEX_NONE;
};
