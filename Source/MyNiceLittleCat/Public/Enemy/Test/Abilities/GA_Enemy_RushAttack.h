#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Enemy/Test/Abilities/GA_Enemy_MeleeAttack.h"
#include "GA_Enemy_RushAttack.generated.h"

// 준비 동작 후 돌진의 첫 자세를 유지하며 이동하고, 돌진 동작과 공격을 차례로 재생한다.
// 어빌리티 실행 전체에서 대상마다 피해를 한 번만 적용한다.
UCLASS()
class MYNICELITTLECAT_API UGA_Enemy_RushAttack : public UGA_Enemy_MeleeAttack
{
	GENERATED_BODY()
public:
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush", meta = (ClampMin = "1", Units = "cm/s"))
	float RushSpeed = 1500.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush")
	FName RushSection = TEXT("Rush");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush")
	FName AttackSection = TEXT("Attack");
	// 캐릭터 캡슐 중심 기준의 로컬 오프셋이다. X축이 전방이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush|Hit")
	FVector RushHitOffset = FVector(100.f, 0.f, 0.f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rush|Hit", meta = (ClampMin = "1", Units = "cm"))
	float RushHitRadius = 80.f;

	virtual bool ValidatePattern() const override;
	virtual void OnMontageStarted() override;
private:
	void UpdateRush();
	void FinishRush();
	FTimerHandle RushTimer;
	FVector RushDirection = FVector::ZeroVector;
	double LastUpdateTime = 0.0;
	float DistanceRemaining = 0.f;
	float RushStartTime = 0.f;
	bool bRushStarted = false;
	bool bRushing = false;
	bool bOwnsRootMotionOverride = false;
	TEnumAsByte<ERootMotionMode::Type> PreviousRootMotionMode;
};
