#include "Stages/Stage3/MansionLightingSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

void UMansionLightingSubsystem::InitializeForStage()
{
	if (bConfigured)
	{
		return;
	}
	bConfigured = true;
	UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	if (UGameProgressSubsystem* Progress = GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr)
	{
		Progress->OnSaveDataReplaced.AddUObject(this, &UMansionLightingSubsystem::RestoreCheckpointLighting);
	}
	RestoreCheckpointLighting();
}

void UMansionLightingSubsystem::RestoreCheckpointLighting()
{
	if (!bConfigured)
	{
		return;
	}
	const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	const UGameProgressSubsystem* Progress = GameInstance ? GameInstance->GetSubsystem<UGameProgressSubsystem>() : nullptr;
	const FCheckpointSaveData* Checkpoint = Progress ? &Progress->GetCheckpointData() : nullptr;
	const bool bSameLevel = Checkpoint && Checkpoint->bValid
		&& Checkpoint->LevelName == UGameplayStatics::GetCurrentLevelName(this, true);
	SetEvening(bSameLevel && Checkpoint->bEvening);
}

void UMansionLightingSubsystem::SetEvening(bool bNewEvening)
{
	if (!bConfigured)
	{
		return;
	}
	bEvening = bNewEvening;

	// MansionMorning / MansionEvening의 원본 조명 값.
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (Light)
		{
			Light->SetMobility(EComponentMobility::Movable);
			It->SetActorRotation(FRotator(bEvening ? -90.f : -45.f, 0.f, 0.f));
			Light->SetIntensity(bEvening ? 2.5f : 3.14f);
			Light->SetLightColor(FLinearColor::FromSRGBColor(bEvening ? FColor(254, 219, 255) : FColor(244, 233, 155)));
		}
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Sky = *It;
		if (!Sky->GetClass()->GetName().StartsWith(TEXT("BP_Sky_Sphere")))
		{
			continue;
		}
		if (FBoolProperty* AutomaticColors = FindFProperty<FBoolProperty>(Sky->GetClass(), TEXT("Colors determined by sun position")))
		{
			AutomaticColors->SetPropertyValue_InContainer(Sky, !bEvening);
		}
		const auto SetSkyColor = [Sky](const TCHAR* Name, const FLinearColor& Color)
		{
			if (FStructProperty* Property = FindFProperty<FStructProperty>(Sky->GetClass(), Name);
				Property && Property->Struct == TBaseStructure<FLinearColor>::Get())
			{
				*Property->ContainerPtrToValuePtr<FLinearColor>(Sky) = Color;
			}
		};
		// 저녁은 짙은 보라색 위쪽 하늘과 분홍기가 도는 지평선을 사용한다.
		SetSkyColor(TEXT("Zenith color"), bEvening ? FLinearColor(0.035f, 0.009f, 0.065f) : FLinearColor(0.f, 0.014149f, 0.171875f));
		SetSkyColor(TEXT("Horizon color"), bEvening ? FLinearColor(0.22f, 0.085f, 0.16f) : FLinearColor(1.979559f, 2.586644f, 3.f));
		SetSkyColor(TEXT("Cloud color"), bEvening ? FLinearColor(0.16f, 0.075f, 0.20f) : FLinearColor(0.684349f, 0.919020f, 1.f));
		SetSkyColor(TEXT("Overall color"), bEvening ? FLinearColor(0.55f, 0.42f, 0.65f) : FLinearColor::White);
		if (FObjectPropertyBase* SunActor = FindFProperty<FObjectPropertyBase>(Sky->GetClass(), TEXT("Directional light actor")))
		{
			SunActor->SetObjectPropertyValue_InContainer(Sky, nullptr);
		}
		if (FNumericProperty* SunHeight = FindFProperty<FNumericProperty>(Sky->GetClass(), TEXT("Sun height")))
		{
			SunHeight->SetFloatingPointPropertyValue(SunHeight->ContainerPtrToValuePtr<void>(Sky), bEvening ? -0.5 : 0.5);
		}
		if (UFunction* Refresh = Sky->FindFunction(TEXT("RefreshMaterial")))
		{
			Sky->ProcessEvent(Refresh, nullptr);
		}
	}

	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		USkyLightComponent* Light = It->GetLightComponent();
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensity(bEvening ? 4.f : 5.5f);
		Light->SetLightColor(FLinearColor::White);
		Light->RecaptureSky();
	}
	UE_LOG(LogTemp, Log, TEXT("[Stage3Lighting] Applied %s"), bEvening ? TEXT("Evening") : TEXT("Morning"));
}
