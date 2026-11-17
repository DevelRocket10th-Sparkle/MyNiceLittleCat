// 발소리 컴포넌트 구현

#include "Characters/PlayerComponents/FootstepComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"

UFootstepComponent::UFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UFootstepComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<ACharacter>(GetOwner());
	if (!Character.IsValid())
	{
		SetComponentTickEnabled(false);
	}
}

void UFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Owner = Character.Get();
	const UCharacterMovementComponent* Move = Owner ? Owner->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}

	const bool bFalling = Move->IsFalling();
	if (bFalling)
	{
		AirTime += DeltaTime;
		bWasFalling = true;
		return;
	}

	// 착지
	if (bWasFalling)
	{
		bWasFalling = false;
		if (bPlayOnLanding && AirTime >= MinAirTime && Move->IsMovingOnGround())
		{
			PlayFootstep(LandingVolumeScale);
			DistanceSinceStep = 0.f;
		}
		AirTime = 0.f;
	}

	if (!Move->IsMovingOnGround())
	{
		return;
	}

	const float Speed = Move->Velocity.Size2D();
	if (Speed < MinSpeed)
	{
		// 멈추면 다음 출발 때 첫 걸음이 바로 나도록
		DistanceSinceStep = FMath::Max(WalkStride, SprintStride);
		return;
	}

	const float Alpha = FMath::Clamp((Speed - WalkSpeed) / FMath::Max(SprintSpeed - WalkSpeed, 1.f), 0.f, 1.f);
	const float Stride = FMath::Lerp(WalkStride, SprintStride, Alpha);

	DistanceSinceStep += Speed * DeltaTime;
	if (DistanceSinceStep >= Stride)
	{
		DistanceSinceStep = FMath::Fmod(DistanceSinceStep, Stride);
		PlayFootstep(FMath::Lerp(SpeedVolumeScale.X, SpeedVolumeScale.Y, Alpha));
	}
}

const FFootstepSoundSet& UFootstepComponent::FindSoundSet(FVector& OutLocation) const
{
	const ACharacter* Owner = Character.Get();
	const float HalfHeight = Owner->GetCapsuleComponent() ? Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	OutLocation = Owner->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

	if (SurfaceSounds.Num() == 0)
	{
		return DefaultSounds;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(Footstep), false, Owner);
	Params.bReturnPhysicalMaterial = true;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, OutLocation + FVector(0.f, 0.f, 20.f), OutLocation - FVector(0.f, 0.f, 60.f), ECC_Visibility, Params))
	{
		OutLocation = Hit.ImpactPoint;
		if (const UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get())
		{
			if (const FFootstepSoundSet* Set = SurfaceSounds.Find(PhysMat->SurfaceType))
			{
				if (Set->Sounds.Num() > 0)
				{
					return *Set;
				}
			}
		}
	}
	return DefaultSounds;
}

void UFootstepComponent::PlayFootstep(float VolumeMultiplier)
{
	if (!Character.IsValid())
	{
		return;
	}

	FVector Location;
	const FFootstepSoundSet& Set = FindSoundSet(Location);
	const int32 Count = Set.Sounds.Num();
	if (Count == 0)
	{
		return;
	}

	// 같은 소리가 연달아 나지 않게
	int32 Index = FMath::RandRange(0, Count - 1);
	if (Count > 1 && Index == LastIndex)
	{
		Index = (Index + 1 + FMath::RandRange(0, Count - 2)) % Count;
	}
	LastIndex = Index;

	USoundBase* Sound = Set.Sounds[Index];
	if (!Sound)
	{
		return;
	}

	const float Volume = FMath::FRandRange(VolumeRange.X, VolumeRange.Y) * Set.VolumeScale * VolumeMultiplier;
	const float Pitch = FMath::FRandRange(PitchRange.X, PitchRange.Y);
	UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, Volume, Pitch, 0.f, Attenuation);
}
