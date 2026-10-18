// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HitStopSubsystem.generated.h"

class UAnimInstance;
class UAnimMontage;

UCLASS()
class MYNICELITTLECAT_API UHitStopSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void ApplyHitStop(AActor* Actor, float Duration);

	virtual void Deinitialize() override;

private:
	struct FHitStopState
	{
		FTimerHandle TimerHandle;
		TWeakObjectPtr<UAnimInstance> AnimInstance;
		TWeakObjectPtr<UAnimMontage> PausedMontage;
	};

	void EndHitStop(TWeakObjectPtr<AActor> WeakActor);
	static void ResumeMontage(const FHitStopState& State);

	TMap<TWeakObjectPtr<AActor>, FHitStopState> ActiveHitStops;
};