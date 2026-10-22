#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Enemy/Test/Abilities/GA_TestBoss_MeleeAttack.h"
#include "GA_TestBoss_RushAttack.generated.h"

// Ready -> hold Rush's first pose while moving -> play Rush -> Attack.
// Damage is limited to once per target across the entire ability execution.
UCLASS()
class MYNICELITTLECAT_API UGA_TestBoss_RushAttack : public UGA_TestBoss_MeleeAttack
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
	// Local-space offset from the character capsule center; X points forward.
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
