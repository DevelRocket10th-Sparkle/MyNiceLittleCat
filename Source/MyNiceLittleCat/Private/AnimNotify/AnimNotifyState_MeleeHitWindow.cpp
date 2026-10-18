// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/AnimNotifyState_MeleeHitWindow.h"
#include "Characters/PlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"

#include "Enemy/GAS/EnemyAttributeSet.h"

void UAnimNotifyState_MeleeHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

    if (!MeshComp) return;

    if (APlayerCharacter* Character =
        Cast<APlayerCharacter>(MeshComp->GetOwner()))
    {
        Character->SetbIsAbleToDodge(false);
        Character->SetbIsAbleToMove(false);
    }

    HitActorsByMesh.FindOrAdd(MeshComp).Empty();

}

void UAnimNotifyState_MeleeHitWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

    if (!MeshComp || !MeshComp->GetOwner()) return;

    AActor* Attacker = MeshComp->GetOwner();
    UWorld* World = MeshComp->GetWorld();

    if (!Attacker || !World) return;

    UStaticMeshComponent* WeaponMesh = nullptr;

    TArray<UStaticMeshComponent*> StaticMeshes;
    Attacker->GetComponents<UStaticMeshComponent>(StaticMeshes);

    for (UStaticMeshComponent* Component : StaticMeshes)
    {
        if (Component && Component->GetName().Contains(TEXT("WeaponMesh")))
        {
            WeaponMesh = Component;
            break;
        }
    }

    if (!WeaponMesh) return;

    if (APlayerCharacter* Character = Cast<APlayerCharacter>(Attacker))
    {
        Character->AddMovementInput(
            Character->GetActorForwardVector(),
            0.8f
        );
    }

    const FVector Start = WeaponMesh->GetSocketLocation(StartSocket);
    const FVector End = WeaponMesh->GetSocketLocation(EndSocket);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeHitSweep), false);

    Params.AddIgnoredActor(Attacker);

    TArray<FHitResult> Hits;

    const FCollisionShape Shape = FCollisionShape::MakeSphere(HitRadius);

    const bool bHit = World->SweepMultiByChannel(
        Hits,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        Shape,
        Params
    );

    if (bDrawDebug)
    {
        DrawDebugLine(
            World, Start, End,
            bHit ? FColor::Red : FColor::Green,
            false, 0.0f, 0, 2.0f);

        DrawDebugSphere(
            World, Start, HitRadius, 8,
            FColor::Yellow, false, 0.0f);

        DrawDebugSphere(
            World, End, HitRadius, 8,
            FColor::Yellow, false, 0.0f);
    }

    if (!bHit) return;

    TSet<TWeakObjectPtr<AActor>>& HitActors = HitActorsByMesh.FindOrAdd(MeshComp);

    ACharacter* AttackingCharacter = Cast<ACharacter>(Attacker);

    AController* InstigatorController =
        AttackingCharacter
        ? AttackingCharacter->GetController()
        : nullptr;

    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();

        if (!Target || HitActors.Contains(Target)) continue;

        HitActors.Add(Target);


        UAbilitySystemComponent* TargetASC =
            UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);

        if (!TargetASC || !DamageEffectClass) continue;

        FGameplayEffectContextHandle Context =
            TargetASC->MakeEffectContext();

        Context.AddSourceObject(Attacker);

        FGameplayEffectSpecHandle SpecHandle =
            TargetASC->MakeOutgoingSpec(
                DamageEffectClass,
                1.0f,
                Context
            );

        if (!SpecHandle.IsValid()) continue;

        const FGameplayTag DamageTag =FGameplayTag::RequestGameplayTag(FName("Data.Damage"));

        UE_LOG(LogTemp, Warning, TEXT("[MeleeHit] 피격: %s"), *Target->GetName());

        SpecHandle.Data->SetSetByCallerMagnitude(
            DamageTag,
            Damage
        );

        TargetASC->ApplyGameplayEffectSpecToSelf(
            *SpecHandle.Data.Get()
        );

        // 카메라 셰이크
        if (APlayerCharacter* Player = Cast<APlayerCharacter>(Attacker))
        {
            if (APlayerController* PC =
                Cast<APlayerController>(Player->GetController()))
            {
                PC->ClientStartCameraShake(
                    HitCameraShakeClass,
                    1.0f
                );
            }
        }
        // 히트 스톱
        ApplyMontageHitStop(Attacker, 0.2f);
        ApplyMontageHitStop(Target, 0.1f);
    }
}


void UAnimNotifyState_MeleeHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    if (APlayerCharacter* Character =
        Cast<APlayerCharacter>(MeshComp->GetOwner()))
    {
		Character->SetbIsAbleToDodge(true);
        Character->SetbIsAbleToMove(true);
    }
}

FString UAnimNotifyState_MeleeHitWindow::GetNotifyName_Implementation() const
{
    return TEXT("Melee Hit Window");
}

void UAnimNotifyState_MeleeHitWindow::ApplyMontageHitStop(AActor* Actor, float Duration)
{
    if (!IsValid(Actor) || Duration <= 0.f) return;

    USkeletalMeshComponent* Mesh =
        Actor->FindComponentByClass<USkeletalMeshComponent>();

    if (!Mesh) return;

    UWorld* World = Actor->GetWorld();
    if (!World) return;

    const float OriginalRate = Mesh->GlobalAnimRateScale;
    Mesh->GlobalAnimRateScale = 0.f;

    TWeakObjectPtr<USkeletalMeshComponent> WeakMesh = Mesh;

    FTimerHandle TimerHandle;
    World->GetTimerManager().SetTimer(
        TimerHandle,
        FTimerDelegate::CreateLambda(
            [WeakMesh, OriginalRate]()
            {
                if (WeakMesh.IsValid())
                {
                    WeakMesh->GlobalAnimRateScale = OriginalRate;
                }
            }),
        Duration,
        false
    );
}
