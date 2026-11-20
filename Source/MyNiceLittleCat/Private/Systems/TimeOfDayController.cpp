// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/TimeOfDayController.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Systems/TimeOfDaySubsystem.h"

ATimeOfDayController::ATimeOfDayController()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ATimeOfDayController::BeginPlay()
{
	Super::BeginPlay();

	// 등록하면 서브시스템이 저장된 시간대(없으면 기본)를 바로 적용
	if (UTimeOfDaySubsystem* TimeOfDay = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
	{
		TimeOfDay->RegisterController(this);
	}
}

void ATimeOfDayController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTimeOfDaySubsystem* TimeOfDay = GetWorld() ? GetWorld()->GetSubsystem<UTimeOfDaySubsystem>() : nullptr)
	{
		TimeOfDay->UnregisterController(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ATimeOfDayController::ApplyTimeOfDay(const FGameplayTag& TimeOfDay)
{
	const FTimeOfDayPreset* Preset = TimeOfDay.IsValid() ? Presets.Find(TimeOfDay) : nullptr;
	ApplyPreset(Preset ? *Preset : DefaultPreset);
}

void ATimeOfDayController::ApplyPreset(const FTimeOfDayPreset& Preset)
{
	// 해: 실행 중에 바꾸므로 Movable로
	for (ADirectionalLight* Sun : Suns)
	{
		if (UDirectionalLightComponent* Light = Sun ? Cast<UDirectionalLightComponent>(Sun->GetLightComponent()) : nullptr)
		{
			Light->SetMobility(EComponentMobility::Movable);
			Sun->SetActorRotation(Preset.SunRotation);
			Light->SetIntensity(Preset.SunIntensity);
			Light->SetLightColor(Preset.SunColor);
		}
	}

	// 하늘 색: 하늘 액터가 인터페이스로 받아 자기 변수와 머티리얼을 갱신
	if (Sky && Sky->Implements<UTimeOfDayReceiver>())
	{
		ITimeOfDayReceiver::Execute_ApplyTimeOfDaySky(Sky, Preset.Sky);
	}

	// 하늘빛: 하늘을 바꾼 뒤 다시 캡처
	for (ASkyLight* SkyLightActor : SkyLights)
	{
		if (USkyLightComponent* Light = SkyLightActor ? SkyLightActor->GetLightComponent() : nullptr)
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(Preset.SkyLightIntensity);
			Light->SetLightColor(Preset.SkyLightColor);
			Light->RecaptureSky();
		}
	}
}
