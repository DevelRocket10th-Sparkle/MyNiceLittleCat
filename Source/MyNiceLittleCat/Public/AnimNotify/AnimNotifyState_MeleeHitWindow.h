// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayEffectTypes.h"
#include "AnimNotifyState_MeleeHitWindow.generated.h"


class UGameplayEffect;
class UCameraShakeBase;
/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API UAnimNotifyState_MeleeHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
    virtual void NotifyBegin(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        float TotalDuration,
        const FAnimNotifyEventReference& EventReference
    ) override;

    virtual void NotifyTick(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        float FrameDeltaTime,
        const FAnimNotifyEventReference& EventReference
    ) override;

    virtual void NotifyEnd(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference
    ) override;

    virtual FString GetNotifyName_Implementation()
        const override;
protected:
    void ApplyMontageHitStop(AActor* Actor, float Duration);

protected:
    UPROPERTY(EditAnywhere, Category = "Melee Hit")
    FName StartSocket = TEXT("Weapon_Start");

    UPROPERTY(EditAnywhere, Category = "Melee Hit")
    FName EndSocket = TEXT("Weapon_Tip");

    UPROPERTY(EditAnywhere, Category = "Melee Hit",
        meta = (ClampMin = "1.0"))
    float HitRadius = 20.0f;

    UPROPERTY(EditAnywhere, Category = "Melee Hit")
    float Damage = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Melee Hit")
    bool bDrawDebug = false;

    UPROPERTY(EditAnywhere, Category = "Melee Hit")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditAnywhere, Category = "Melee Hit")
    TSubclassOf<UCameraShakeBase> HitCameraShakeClass;

    UPROPERTY(EditAnywhere, Category = "Melee Hit")
    TObjectPtr<USoundBase> HitSound;

    UPROPERTY(EditAnywhere, Category = "Melee Hit|Hit Stop", meta = (ClampMin = "0.0"))
    float HitStopDuration = 0.08f;

    UPROPERTY(EditAnywhere, Category = "Melee Hit|Hit Stop", meta = (ClampMin = "0.0"))
    float KillHitStopDuration = 0.2f;

    UPROPERTY(EditAnywhere, Category = "Deflect")
    FName DeflectTag = TEXT("Deflect");

    UPROPERTY(EditAnywhere, Category = "Deflect")
    TObjectPtr<USoundBase> DeflectSound;

private:
    TMap<TWeakObjectPtr<USkeletalMeshComponent>, TSet<TWeakObjectPtr<AActor>>> HitActorsByMesh;
    TMap<TWeakObjectPtr<USkeletalMeshComponent>, FTimerHandle> HitStopTimers;
};
