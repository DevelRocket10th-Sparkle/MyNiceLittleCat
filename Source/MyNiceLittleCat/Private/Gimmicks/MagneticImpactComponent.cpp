#include "Gimmicks/MagneticImpactComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Pet/PetCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UMagneticImpactComponent::UMagneticImpactComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;   // 물리 이동 후 속도 기록
}

void UMagneticImpactComponent::BeginPlay()
{
	Super::BeginPlay();

	Body = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	if (UPrimitiveComponent* Root = Body.Get())
	{
		Root->SetNotifyRigidBodyCollision(true);   // Hit 이벤트 받기
		Root->OnComponentHit.AddDynamic(this, &UMagneticImpactComponent::HandleHit);
	}
}

void UMagneticImpactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 충돌 처리 후 속도는 이미 줄어 있으므로, 직전 프레임 속도를 기준으로 판정
	if (const UPrimitiveComponent* Root = Body.Get())
	{
		LastVelocity = Root->IsSimulatingPhysics() ? Root->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	}
}

void UMagneticImpactComponent::HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!OtherActor || OtherActor == GetOwner() || OtherActor->IsA<APlayerCharacter>() || OtherActor->IsA<APetCharacter>()) return;

	const float Speed = LastVelocity.Size();
	if (Speed < MinImpactSpeed) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!TargetASC) return;   // ASC 없는 물체(벽 등)는 무시

	const float Now = GetWorld()->GetTimeSeconds();
	if (const float* Last = LastHitTime.Find(OtherActor); Last && Now - *Last < SameTargetCooldown) return;
	LastHitTime.Add(OtherActor, Now);

	// 대미지 (속도 비례)
	if (DamageEffectClass)
	{
		const float Scale = FMath::Clamp(Speed / ReferenceSpeed, 0.f, MaxDamageScale);

		FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
		Context.AddSourceObject(GetOwner());

		FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), BaseDamage * Scale);
			TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// 넉백 (물체가 날아가던 방향으로)
	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		const FVector Dir = LastVelocity.GetSafeNormal2D();
		//Character->LaunchCharacter(Dir * KnockbackStrength + FVector(0.f, 0.f, KnockbackUpward), true, true);
		const float Mass = Character->GetCharacterMovement() ? FMath::Max(Character->GetCharacterMovement()->Mass, 1.f) : 100.f;
		const float MassScale = 100.f / Mass;
		Character->LaunchCharacter((Dir * KnockbackStrength + FVector(0.f, 0.f, KnockbackUpward)) * MassScale, true, true);
	}

	// 물체는 부딪힌 뒤 힘이 빠지게
	if (UPrimitiveComponent* Root = Body.Get(); Root && Root->IsSimulatingPhysics())
	{
		Root->SetPhysicsLinearVelocity(LastVelocity * BounceVelocityScale);
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Hit.ImpactPoint);
	}
}
