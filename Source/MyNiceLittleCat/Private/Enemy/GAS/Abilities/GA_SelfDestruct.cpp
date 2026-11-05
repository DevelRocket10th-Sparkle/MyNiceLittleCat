#include "Enemy/GAS/Abilities/GA_SelfDestruct.h"
#include "Enemy/Bomb/BombEnemy.h"
#include "Enemy/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Pet/PetCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"

UGA_SelfDestruct::UGA_SelfDestruct()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_SelfDestruct::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Bomb = Cast<ABombEnemy>(GetAvatarActorFromActorInfo());
	if (!Bomb.IsValid() || Bomb->HasExploded() || !Bomb->IsAlive() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ABombEnemy* B = Bomb.Get();
	B->SetActiveSelfDestruct(this);

	// 점화되면 제자리에 멈춰서 부풀어 오름 (펫에 잡혀 있으면 펫이 옮김)
	if (AAIController* AI = Cast<AAIController>(B->GetController()))
	{
		AI->StopMovement();
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("SelfDestruct"));
		}
	}

	USkeletalMeshComponent* Mesh = B->GetMesh();
	BaseMeshScale = Mesh->GetRelativeScale3D();
	PreviousOverlay = Mesh->GetOverlayMaterial();

	if (OverlayMaterial)
	{
		OverlayMID = UMaterialInstanceDynamic::Create(OverlayMaterial, B);
		OverlayMID->SetScalarParameterValue(OverlayParameterName, 0.f);
		Mesh->SetOverlayMaterial(OverlayMID);
	}

	const FName Socket = Mesh->DoesSocketExist(FuseAttachSocket) ? FuseAttachSocket : NAME_None;
	if (FuseEffect)
	{
		FuseEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(FuseEffect, Mesh, Socket,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
	}
	if (FuseSound)
	{
		FuseSoundComponent = UGameplayStatics::SpawnSoundAttached(FuseSound, Mesh, Socket);
	}
	bVisualsApplied = true;

	Elapsed = 0.f;
	Phase = 0.f;
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.SetTimer(FuseTickTimer, this, &UGA_SelfDestruct::TickFuse, 1.f / 60.f, true);
	Timers.SetTimer(DetonateTimer, this, &UGA_SelfDestruct::DetonateNow, FuseSeconds, false);
}

void UGA_SelfDestruct::TickFuse()
{
	ABombEnemy* B = Bomb.Get();
	if (!B) return;

	const float Dt = GetWorld()->GetDeltaSeconds();
	Elapsed += Dt;

	// 0 → 1, 끝으로 갈수록 급해지게(ease-in)
	const float Alpha = FMath::Clamp(Elapsed / FuseSeconds, 0.f, 1.f);
	const float Urgency = Alpha * Alpha;

	const float Frequency = FMath::Lerp(PulseFrequencyStart, PulseFrequencyEnd, Urgency);
	Phase += Frequency * Dt * UE_TWO_PI;
	const float Wave = FMath::Sin(Phase);

	// 1. 메시 변형: 전체적으로 부풀면서, 옆으로 퍼질 때 위아래로 눌리는 맥동
	const float Inflate = InflateAmount * Urgency;
	const float Pulse = PulseAmount * (0.4f + 0.6f * Alpha) * Wave;
	const FVector Deform(1.f + Inflate + Pulse, 1.f + Inflate + Pulse, 1.f + Inflate - Pulse * 0.6f);
	B->GetMesh()->SetRelativeScale3D(BaseMeshScale * Deform);

	// 2. 오버레이 점멸: 사인파를 날카롭게 → 번쩍번쩍
	if (OverlayMID)
	{
		const float Flash = FMath::Pow(0.5f + 0.5f * Wave, 3.f);
		OverlayMID->SetScalarParameterValue(OverlayParameterName, Flash);
	}
}

void UGA_SelfDestruct::DetonateNow()
{
	ABombEnemy* B = Bomb.Get();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	if (B)
	{
		Explode(B);
	}
}

void UGA_SelfDestruct::Explode(ABombEnemy* InBomb) const
{
	if (!IsValid(InBomb) || InBomb->HasExploded()) return;

	UWorld* World = InBomb->GetWorld();
	const FVector Center = InBomb->GetActorLocation();

	// 연쇄 폭발 시 서로 다시 터뜨리지 않도록 먼저 표시
	InBomb->MarkExploded();

	// 3. 폭발 FX
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ExplosionEffect, Center, FRotator::ZeroRotator, ExplosionEffectScale);
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, ExplosionSound, Center);
	}
	if (ExplosionShake)
	{
		UGameplayStatics::PlayWorldCameraShake(World, ExplosionShake, Center, ShakeInnerRadius, ShakeOuterRadius);
	}

	// 범위 판정
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BombExplosion), false);
	Params.AddIgnoredActor(InBomb);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectTypes, FCollisionShape::MakeSphere(Radius), Params);

	TSet<AActor*> Processed;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!IsValid(Actor) || Processed.Contains(Actor)) continue;
		Processed.Add(Actor);

		const FVector Offset = Actor->GetActorLocation() - Center;
		const float Falloff = 1.f - 0.5f * FMath::Clamp(Offset.Size() / FMath::Max(Radius, 1.f), 0.f, 1.f);
		FVector Dir = Offset.GetSafeNormal2D();
		if (Dir.IsNearlyZero()) Dir = InBomb->GetActorForwardVector();

		if (APlayerCharacter* Player = Cast<APlayerCharacter>(Actor))
		{
			if (!Player->GetbIsDead())
			{
				Player->ApplyPlayerHit(PlayerDamage, Dir);   // 플레이어 넉백은 ApplyPlayerHit 안에서 처리
			}
			continue;
		}

		if (Actor->IsA<APetCharacter>()) continue;   // 펫은 무적

		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor))
		{
			if (!Enemy->IsAlive()) continue;

			if (EnemyDamageEffect)
			{
				if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy))
				{
					FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
					Context.AddSourceObject(InBomb);
					FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(EnemyDamageEffect, 1.f, Context);
					if (Spec.IsValid())
					{
						Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), EnemyDamage * Falloff);
						TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
					}
				}
			}

			// 대미지로 죽었으면(혹은 다른 폭탄이 연쇄 폭발했으면) 넉백 생략
			if (Enemy->IsAlive())
			{
				const UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement();
				const float MassScale = 100.f / FMath::Max(CMC ? CMC->Mass : 100.f, 1.f);
				Enemy->LaunchCharacter((Dir * KnockbackStrength + FVector(0.f, 0.f, KnockbackUpward)) * Falloff * MassScale, true, true);
			}
			continue;
		}

		// 상자, 포탑 등 물리 오브젝트
		if (UPrimitiveComponent* Prim = Overlap.GetComponent(); Prim && Prim->IsSimulatingPhysics())
		{
			Prim->AddRadialImpulse(Center, Radius, PhysicsImpulse, ERadialImpulseFalloff::RIF_Linear, true);
		}
	}

	InBomb->HandleExploded();
}

void UGA_SelfDestruct::RestoreVisuals()
{
	if (!bVisualsApplied) return;
	bVisualsApplied = false;

	if (ABombEnemy* B = Bomb.Get())
	{
		B->GetMesh()->SetRelativeScale3D(BaseMeshScale);
		B->GetMesh()->SetOverlayMaterial(PreviousOverlay);
	}
	if (FuseEffectComponent)
	{
		FuseEffectComponent->DestroyComponent();
		FuseEffectComponent = nullptr;
	}
	if (FuseSoundComponent)
	{
		FuseSoundComponent->Stop();
		FuseSoundComponent = nullptr;
	}
	OverlayMID = nullptr;
	PreviousOverlay = nullptr;
}

void UGA_SelfDestruct::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FuseTickTimer);
		World->GetTimerManager().ClearTimer(DetonateTimer);
	}
	RestoreVisuals();

	if (ABombEnemy* B = Bomb.Get())
	{
		B->SetActiveSelfDestruct(nullptr);
	}
	Bomb.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
