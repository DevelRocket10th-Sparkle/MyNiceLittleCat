// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/PlayerComponents/JellyComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

// Sets default values for this component's properties
UJellyComponent::UJellyComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// ...
}

void UJellyComponent::StartJelly()
{
	if (!Mesh) return;

	Elapsed = 0.f;
	SetComponentTickEnabled(true);
}


// Called when the game starts
void UJellyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Mesh = Character->GetMesh();
		BaseScale = Mesh->GetRelativeScale3D();
	}

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (HealthAttribute.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(HealthAttribute)
				.AddUObject(this, &UJellyComponent::OnHealthChanged);
		}
	}
	
}


// Called every frame
void UJellyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Elapsed += DeltaTime;

	const float W = Amount * FMath::Exp(-Damping * Elapsed) * FMath::Cos(Frequency * Elapsed);
	Mesh->SetRelativeScale3D(BaseScale * FVector(1.f + W, 1.f + W, 1.f - W));

	if (Elapsed > 1.f)
	{
		StopJelly();
	}
}

void UJellyComponent::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue < Data.OldValue)
	{
		StartJelly();
	}
}

void UJellyComponent::StopJelly()
{
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(BaseScale);
	}
	SetComponentTickEnabled(false);
}

